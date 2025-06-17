/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <functional>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/transform-pass.hpp>
#include <bloom/transform/vectorize/slp.hpp>

namespace blm
{
	std::string_view SLPPass::name() const
	{
		return "superword-level-parallelism";
	}

	std::string_view SLPPass::description() const
	{
		return "aggressively vectorizes consecutive scalar operations into SIMD operations";
	}

	std::vector<const std::type_info *> SLPPass::required_passes() const
	{
		return get_pass_types<LocalAliasAnalysisPass>();
	}

	bool SLPPass::run(Module &module, PassContext &context)
	{
		const auto *alias_result = context.get_result<LocalAliasResult>();
		std::unique_ptr<LocalAliasResult> local_result;
		if (!alias_result)
		{
			LocalAliasAnalysisPass laa;
			local_result = std::unique_ptr<LocalAliasResult>(
				dynamic_cast<LocalAliasResult *>(laa.analyze(module, context).release()));
			alias_result = local_result.get();
		}

		processed_nodes.clear();
		candidates.clear();
		std::size_t vectorized_count = 0;

		for (Node *func: module.get_functions())
		{
			if (func->ir_type != NodeType::FUNCTION)
				continue;

			if (Region *func_region = find_function_region(func, module))
				process_region(func_region, *alias_result, module.get_context());
		}

		std::ranges::sort(candidates,
		                  [](const VectorCandidate &a, const VectorCandidate &b)
		                  {
			                  return a.scalar_ops.size() > b.scalar_ops.size();
		                  });

		for (const auto &candidate: candidates)
		{
			if (candidate.is_valid())
			{
				if (Region *target_region = candidate.scalar_ops[0]->parent_region)
				{
					vectorize_candidate(candidate, target_region, module.get_context());
					vectorized_count += candidate.scalar_ops.size();
				}
			}
		}

		context.update_stat("slp.vectorized_operations", vectorized_count);
		context.update_stat("slp.vector_groups", candidates.size());
		return vectorized_count > 0;
	}

	void SLPPass::process_region(Region *region, const LocalAliasResult &alias_result, Context &ctx) // NOLINT(*-no-recursion)
	{
		if (!region)
			return;

		DependencyGraph dep_graph = build_dependency_graph(region);
		for (auto region_candidates = find_vectorization_candidates(region);
		     const auto &candidate: region_candidates)
		{
			if (candidate.is_valid() && can_vectorize_together(candidate.scalar_ops, alias_result, dep_graph))
				candidates.push_back(candidate);
		}

		for (Region *child: region->get_children())
			process_region(child, alias_result, ctx);
	}

	DependencyGraph SLPPass::build_dependency_graph(Region *region)
	{
		DependencyGraph graph;
		for (Node *node: region->get_nodes())
		{
			graph.nodes.insert(node);

			for (Node *input: node->inputs)
			{
				if (graph.nodes.contains(input))
				{
					graph.edges[input].insert(node);
					graph.reverse_edges[node].insert(input);
				}
			}
		}

		return graph;
	}

	bool SLPPass::has_dependency_cycle(const std::vector<Node *> &ops, const DependencyGraph &graph)
	{
		std::unordered_set op_set(ops.begin(), ops.end());
		for (Node *op: ops)
		{
			std::unordered_set<Node *> visited;
			std::function<bool(Node *)> dfs = [&](Node *current) -> bool
			{
				if (visited.contains(current))
					return false;
				visited.insert(current);

				if (auto it = graph.edges.find(current);
					it != graph.edges.end())
				{
					for (Node *successor: it->second)
					{
						if (op_set.contains(successor) && successor != op)
							return true;
						if (dfs(successor))
							return true;
					}
				}
				return false;
			};

			if (dfs(op))
				return true;
		}

		return false;
	}

	std::vector<VectorCandidate> SLPPass::find_vectorization_candidates(Region *region)
	{
		std::vector<VectorCandidate> region_candidates;

		for (Node *node: region->get_nodes())
		{
			if (processed_nodes.contains(node) || !can_vectorize_operation(node->ir_type))
				continue;
			std::vector node_candidates = {
				try_build_candidate(node, region),
				try_build_isomorphic_candidate(node, region),
				try_build_chain_candidate(node, region)
			};

			for (auto &candidate: node_candidates)
			{
				if (candidate.is_valid() && candidate.scalar_ops.size() >= 2)
				{
					region_candidates.push_back(candidate);
					for (Node *op: candidate.scalar_ops)
						processed_nodes.insert(op);
					break;
				}
			}
		}

		return region_candidates;
	}

	VectorCandidate SLPPass::try_build_candidate(Node *start_node, Region *region) const
	{
		VectorCandidate candidate;
		candidate.operation = start_node->ir_type;
		candidate.element_type = start_node->type_kind;
		candidate.scalar_ops.push_back(start_node);

		std::uint32_t max_width = get_max_vector_width(candidate.element_type);
		for (Node *node: region->get_nodes())
		{
			if (node == start_node || processed_nodes.contains(node))
				continue;
			if (node->ir_type == candidate.operation &&
			    node->type_kind == candidate.element_type &&
			    is_simd_compatible_type(node->type_kind) &&
			    candidate.scalar_ops.size() < max_width)
			{
				candidate.scalar_ops.push_back(node);
			}
		}

		candidate.vector_width = static_cast<std::uint32_t>(candidate.scalar_ops.size());
		return candidate;
	}

	VectorCandidate SLPPass::try_build_isomorphic_candidate(Node *start_node, Region *region) const
	{
		VectorCandidate candidate;
		candidate.operation = start_node->ir_type;
		candidate.element_type = start_node->type_kind;
		candidate.scalar_ops.push_back(start_node);

		std::uint32_t max_width = get_max_vector_width(candidate.element_type);
		for (Node *node: region->get_nodes())
		{
			if (node == start_node || processed_nodes.contains(node))
				continue;

			if (node->ir_type == candidate.operation &&
			    node->type_kind == candidate.element_type &&
			    node->inputs.size() == start_node->inputs.size() &&
			    is_simd_compatible_type(node->type_kind) &&
			    candidate.scalar_ops.size() < max_width)
			{
				if (are_operations_isomorphic(start_node, node))
					candidate.scalar_ops.push_back(node);
			}
		}

		candidate.vector_width = static_cast<std::uint32_t>(candidate.scalar_ops.size());
		return candidate;
	}

	VectorCandidate SLPPass::try_build_chain_candidate(Node *start_node, Region *)
	{
		VectorCandidate candidate;
		candidate.operation = start_node->ir_type;
		candidate.element_type = start_node->type_kind;

		std::vector<Node *> chain = find_operation_chain(start_node);
		if (chain.size() >= 2)
		{
			candidate.scalar_ops = chain;
			candidate.vector_width = static_cast<std::uint32_t>(chain.size());
		}

		return candidate;
	}

	std::vector<Node *> SLPPass::find_operation_chain(Node *start)
	{
		std::vector<Node *> chain;
		std::unordered_set<Node *> visited;

		std::function<void(Node *)> build_chain = [&](Node *current)
		{
			if (visited.contains(current) || !can_vectorize_operation(current->ir_type))
				return;

			visited.insert(current);
			chain.push_back(current);
			for (Node *user: current->users)
			{
				if (user->ir_type == start->ir_type &&
				    user->type_kind == start->type_kind &&
				    chain.size() < get_max_vector_width(start->type_kind))
				{
					build_chain(user);
				}
			}
		};

		build_chain(start);
		return chain;
	}

	bool SLPPass::are_operations_isomorphic(Node *a, Node *b)
	{
		if (a->ir_type != b->ir_type || a->inputs.size() != b->inputs.size())
			return false;

		for (std::size_t i = 0; i < a->inputs.size(); ++i)
		{
			Node *input_a = a->inputs[i];
			Node *input_b = b->inputs[i];
			if (input_a != input_b && input_a->ir_type != input_b->ir_type)
				return false;
		}

		return true;
	}

	bool SLPPass::can_vectorize_operation(NodeType op_type)
	{
		switch (op_type)
		{
			case NodeType::ADD:
			case NodeType::SUB:
			case NodeType::MUL:
			case NodeType::DIV:
			case NodeType::BAND:
			case NodeType::BOR:
			case NodeType::BXOR:
			case NodeType::BSHL:
			case NodeType::BSHR:
			case NodeType::LOAD:
			case NodeType::STORE:
			case NodeType::PTR_LOAD:
			case NodeType::PTR_STORE:
				return true;
			default:
				return false;
		}
	}

	bool SLPPass::can_vectorize_together(const std::vector<Node *> &ops,
	                                     const LocalAliasResult &alias_result,
	                                     const DependencyGraph &dep_graph)
	{
		if (ops.size() < 2)
			return false;
		if (has_dependency_cycle(ops, dep_graph))
			return false;
		if (!check_memory_safety(ops, alias_result))
			return false;
		if (!can_reorder_operations(ops, dep_graph))
			return false;

		return true;
	}

	bool SLPPass::check_memory_safety(const std::vector<Node *> &ops, const LocalAliasResult &alias_result)
	{
		std::vector<Node *> memory_ops;

		for (Node *op: ops)
		{
			if (op->ir_type == NodeType::LOAD || op->ir_type == NodeType::STORE ||
			    op->ir_type == NodeType::PTR_LOAD || op->ir_type == NodeType::PTR_STORE)
			{
				memory_ops.push_back(op);
			}
		}
		for (size_t i = 0; i < memory_ops.size(); ++i)
		{
			for (size_t j = i + 1; j < memory_ops.size(); ++j)
			{
				Node *op_a = memory_ops[i];
				Node *op_b = memory_ops[j];
				Node *addr_a = op_a->inputs.empty() ? nullptr : op_a->inputs[0];
				Node *addr_b = op_b->inputs.empty() ? nullptr : op_b->inputs[0];
				if (!addr_a || !addr_b)
					continue;

				/* if both are stores or one is store and other is load, check aliasing */
				bool has_store = (op_a->ir_type == NodeType::STORE || op_a->ir_type == NodeType::PTR_STORE ||
				                  op_b->ir_type == NodeType::STORE || op_b->ir_type == NodeType::PTR_STORE);

				if (has_store && alias_result.may_alias(addr_a, addr_b))
					return false;
			}
		}

		return true;
	}

	bool SLPPass::can_reorder_operations(const std::vector<Node *> &ops, const DependencyGraph &dep_graph)
	{
		/* operations can be reordered if they don't have dependencies on each other
		 * and don't violate any ordering constraints */
		std::unordered_set op_set(ops.begin(), ops.end());
		for (Node *op: ops)
		{
			if (auto it = dep_graph.reverse_edges.find(op);
				it != dep_graph.reverse_edges.end())
			{
				for (Node *dep: it->second)
				{
					if (op_set.contains(dep))
					{
						if (!is_safe_shared_dependency(dep, ops))
							return false;
					}
				}
			}
		}

		return true;
	}

	bool SLPPass::is_safe_shared_dependency(Node *producer, const std::vector<Node *> &ops)
	{
		/* a shared dependency is safe if the producer is used by multiple operations in the group
		 * OR the producer doesn't have side effects
		 * OR the operations using it can be executed in parallel */

		if (!can_vectorize_operation(producer->ir_type))
			return true;

		int usage_count = 0;
		for (Node *op: ops)
		{
			if (std::ranges::find(op->inputs, producer) != op->inputs.end())
				usage_count++;
		}
		return usage_count > 1;
	}

	void SLPPass::vectorize_candidate(const VectorCandidate &candidate, Region *region, Context &ctx)
	{
		Node *vector_op = create_vector_operation(candidate, region, ctx);
		scatter_results(vector_op, candidate.scalar_ops, region, ctx);
		remove_scalar_operations(candidate.scalar_ops, region);
	}

	Node *SLPPass::create_vector_operation(const VectorCandidate &candidate, Region *region, Context &ctx)
	{
		DataType vector_type = ctx.create_vector_type(candidate.element_type, candidate.vector_width);
		Node *vector_op = ctx.create<Node>();
		vector_op->ir_type = candidate.operation;
		vector_op->type_kind = vector_type;

		std::size_t num_operands = candidate.scalar_ops[0]->inputs.size();
		for (std::size_t i = 0; i < num_operands; ++i)
		{
			if (Node *vector_operand = gather_operands(candidate.scalar_ops, i, region, ctx))
			{
				vector_op->inputs.push_back(vector_operand);
				vector_operand->users.push_back(vector_op);
			}
		}

		region->insert_node_before(candidate.scalar_ops[0], vector_op);
		return vector_op;
	}

	Node *SLPPass::gather_operands(const std::vector<Node *> &scalar_ops, std::size_t operand_index,
	                               Region *region, Context &ctx)
	{
		std::vector<Node *> operands;
		for (Node *scalar_op: scalar_ops)
		{
			if (operand_index < scalar_op->inputs.size())
				operands.push_back(scalar_op->inputs[operand_index]);
		}

		if (operands.empty())
			return nullptr;

		if (std::ranges::all_of(operands,
		                        [&](Node *op)
		                        {
			                        return op == operands[0];
		                        }))
		{
			const DataType vector_type = ctx.create_vector_type(operands[0]->type_kind,
			                                                    static_cast<std::uint32_t>(operands.size()));

			Node *broadcast = ctx.create<Node>();
			broadcast->ir_type = NodeType::VECTOR_SPLAT;
			broadcast->type_kind = vector_type;
			broadcast->inputs.push_back(operands[0]);
			operands[0]->users.push_back(broadcast);

			Node *earliest_user = scalar_ops[0];
			region->insert_node_before(earliest_user, broadcast);
			return broadcast;
		}

		const DataType element_type = operands[0]->type_kind;
		const DataType vector_type = ctx.create_vector_type(element_type, static_cast<std::uint32_t>(operands.size()));

		Node *vector_build = ctx.create<Node>();
		vector_build->ir_type = NodeType::VECTOR_BUILD;
		vector_build->type_kind = vector_type;

		for (Node *operand: operands)
		{
			vector_build->inputs.push_back(operand);
			operand->users.push_back(vector_build);
		}

		Node *earliest_user = scalar_ops[0];
		for (Node *scalar_op: scalar_ops)
		{
			const auto &nodes = region->get_nodes();
			auto earliest_pos = std::find(nodes.begin(), nodes.end(), earliest_user);
			auto current_pos = std::find(nodes.begin(), nodes.end(), scalar_op);
			if (current_pos < earliest_pos)
				earliest_user = scalar_op;
		}

		region->insert_node_before(earliest_user, vector_build);
		return vector_build;
	}

	void SLPPass::scatter_results(Node *vector_result, const std::vector<Node *> &scalar_ops,
	                              Region *region, Context &ctx)
	{
		Node *insert_after = vector_result;

		for (std::size_t i = 0; i < scalar_ops.size(); ++i)
		{
			Node *extract = ctx.create<Node>();
			extract->ir_type = NodeType::VECTOR_EXTRACT;
			extract->type_kind = scalar_ops[i]->type_kind;

			extract->inputs.push_back(vector_result);
			vector_result->users.push_back(extract);

			Node *index = ctx.create<Node>();
			index->ir_type = NodeType::LIT;
			index->type_kind = DataType::UINT32;
			index->data.set<std::uint32_t, DataType::UINT32>(static_cast<std::uint32_t>(i));

			extract->inputs.push_back(index);
			index->users.push_back(extract);

			region->insert_node_after(insert_after, index);
			region->insert_node_after(index, extract);
			replace_node_uses(scalar_ops[i], extract);
			insert_after = extract;
		}
	}

	void SLPPass::replace_node_uses(Node *old_node, Node *new_node)
	{
		std::vector<Node *> users_copy = old_node->users;
		for (Node *user: users_copy)
		{
			for (size_t i = 0; i < user->inputs.size(); i++)
			{
				if (user->inputs[i] == old_node)
				{
					user->inputs[i] = new_node;
					new_node->users.push_back(user);
				}
			}
		}
		old_node->users.clear();
	}

	bool SLPPass::is_simd_compatible_type(DataType type)
	{
		switch (type)
		{
			case DataType::INT8:
			case DataType::INT16:
			case DataType::INT32:
			case DataType::INT64:
			case DataType::UINT8:
			case DataType::UINT16:
			case DataType::UINT32:
			case DataType::UINT64:
			case DataType::FLOAT32:
			case DataType::FLOAT64:
				return true;
			default:
				return false;
		}
	}

	std::uint32_t SLPPass::get_max_vector_width(DataType element_type)
	{
		switch (element_type)
		{
			case DataType::INT8:
			case DataType::UINT8:
				return 64; /* up to 64x i8 */
			case DataType::INT16:
			case DataType::UINT16:
				return 32; /* up to 32x i16 */
			case DataType::INT32:
			case DataType::UINT32:
			case DataType::FLOAT32:
				return 32; /* up to 32x i32/f32 */
			case DataType::INT64:
			case DataType::UINT64:
			case DataType::FLOAT64:
				return 16; /* up to 16x i64/f64 */
			default:
				return 1;
		}
	}

	void SLPPass::remove_scalar_operations(const std::vector<Node *> &ops, Region *region)
	{
		for (Node *op: ops)
		{
			for (Node *input: op->inputs)
			{
				if (input)
					std::erase(input->users, op);
			}
			region->remove_node(op);
		}
	}

	Region *SLPPass::find_function_region(Node *function, Module &module) const
	{
		if (!function || function->ir_type != NodeType::FUNCTION)
			return nullptr;

		const std::string_view func_name = module.get_context().get_string(function->str_id);
		return find_region_by_name(module.get_root_region(), func_name);
	}

	Region *SLPPass::find_region_by_name(const Region *region, std::string_view name) const // NOLINT(*-no-recursion)
	{
		if (!region)
			return nullptr;

		if (region->get_name() == name)
			return const_cast<Region *>(region);

		for (const Region *child: region->get_children())
		{
			if (Region *found = find_region_by_name(child, name))
				return found;
		}

		return nullptr;
	}
}
