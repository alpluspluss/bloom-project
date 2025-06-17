/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <cstring>
#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/transform/cse.hpp>

namespace blm
{
	std::string_view CSEPass::name() const
	{
		return "common-subexpression-elimination";
	}

	std::string_view CSEPass::description() const
	{
		return "eliminates redundant computations by reusing previously computed values";
	}

	std::vector<const std::type_info *> CSEPass::required_passes() const
	{
		return get_pass_types<LocalAliasAnalysisPass>();
	}

	bool CSEPass::run(Module &m, PassContext &ctx)
	{
		const auto *alias_result = ctx.get_result<LocalAliasResult>();

		std::unique_ptr<LocalAliasResult> local_result;
		if (!alias_result)
		{
			LocalAliasAnalysisPass laa;
			local_result = std::unique_ptr<LocalAliasResult>(
				dynamic_cast<LocalAliasResult *>(laa.analyze(m, ctx).release()));
			alias_result = local_result.get();
		}

		value_numbers.clear();
		expression_to_node.clear();
		next_value_number = 1;

		const auto eliminated = static_cast<std::int64_t>(process_function(m, *alias_result));
		ctx.update_stat("cse.eliminated_expressions", eliminated);
		return eliminated > 0;
	}

	std::size_t CSEPass::process_function(Module &module, const LocalAliasResult &alias_result)
	{
		std::size_t eliminated = 0;

		for (Node *func_node : module.get_functions())
		{
			if (func_node->ir_type != NodeType::FUNCTION)
				continue;

			for (const Region *child : module.get_root_region()->get_children())
			{
				if (child->get_name() == module.get_context().get_string(func_node->str_id))
				{
					eliminated += process_region(child, alias_result);
					break;
				}
			}
		}

		eliminated += process_region(module.get_root_region(), alias_result);
		return eliminated;
	}

	std::size_t CSEPass::process_region(const Region *region, const LocalAliasResult &alias_result) // NOLINT(*-no-recursion)
	{
		if (!region)
			return 0;

		std::size_t eliminated = 0;
		for (Node *node : region->get_nodes())
		{
			if (!is_eligible_for_cse(node))
				continue;

			ValueNumber vn = compute_value_number(node, alias_result);

			if (vn == 0)
				continue;

			if (auto it = expression_to_node.find(vn); it != expression_to_node.end())
			{
				Node *existing = it->second;

				if (is_load_operation(node) && is_load_operation(existing))
				{
					if (loads_may_alias(existing, node, alias_result))
						continue;
				}

				if (replace_all_uses(node, existing))
				{
					eliminated++;
					continue;
				}
			}

			expression_to_node[vn] = node;
			value_numbers[node] = vn;
		}

		for (const Region *child : region->get_children())
		{
			eliminated += process_region(child, alias_result);
		}

		return eliminated;
	}

	ValueNumber CSEPass::compute_value_number(Node *node, const LocalAliasResult &alias_result)
	{
		if (auto it = value_numbers.find(node); it != value_numbers.end())
			return it->second;

		ValueNumber vn = 0;
		if (node->ir_type == NodeType::LIT)
			vn = compute_literal_value_number(node);
		else if (is_load_operation(node))
			vn = compute_load_value_number(node, alias_result);
		else if (has_inputs(node))
			vn = compute_expression_value_number(node);
		else
			vn = next_value_number++;

		if (vn != 0)
			value_numbers[node] = vn;

		return vn;
	}

	ValueNumber CSEPass::compute_literal_value_number(Node *node)
	{
		/* note: there was once a mysterious hash-related bug here that
		 * disappeared when we tried to debug it. if CSE starts acting
		 * weird again, see #118 and try to check value number computation order */

		auto hash = static_cast<std::uint64_t>(node->type_kind);
		switch (node->type_kind)
		{
			case DataType::INT8:
				hash = (hash * 31) + static_cast<std::uint64_t>(node->as<DataType::INT8>());
				break;
			case DataType::INT16:
				hash = (hash * 31) + static_cast<std::uint64_t>(node->as<DataType::INT16>());
				break;
			case DataType::INT32:
				hash = (hash * 31) + static_cast<std::uint64_t>(node->as<DataType::INT32>());
				break;
			case DataType::INT64:
				hash = (hash * 31) + static_cast<std::uint64_t>(node->as<DataType::INT64>());
				break;
			case DataType::UINT8:
				hash = (hash * 31) + static_cast<std::uint64_t>(node->as<DataType::UINT8>());
				break;
			case DataType::UINT16:
				hash = (hash * 31) + static_cast<std::uint64_t>(node->as<DataType::UINT16>());
				break;
			case DataType::UINT32:
				hash = (hash * 31) + static_cast<std::uint64_t>(node->as<DataType::UINT32>());
				break;
			case DataType::UINT64:
				hash = (hash * 31) + node->as<DataType::UINT64>();
				break;
			case DataType::FLOAT32:
			{
				const float value = node->as<DataType::FLOAT32>();
				std::uint32_t bits;
				std::memcpy(&bits, &value, sizeof(bits));
				hash = (hash * 31) + bits;
				break;
			}
			case DataType::FLOAT64:
			{
				const double value = node->as<DataType::FLOAT64>();
				std::uint64_t bits;
				std::memcpy(&bits, &value, sizeof(bits));
				hash = (hash * 31) + bits;
				break;
			}
			case DataType::BOOL:
				hash = (hash * 31) + (node->as<DataType::BOOL>() ? 1 : 0);
				break;
			case DataType::STRING:
			{
				hash = (hash * 31) + std::hash<std::string>{}(node->as<DataType::STRING>());
				break;
			}
			default:
				return 0;
		}

		return hash == 0 ? 1 : hash;
	}

	ValueNumber CSEPass::compute_expression_value_number(Node *node)
	{
		auto hash = static_cast<std::uint64_t>(node->ir_type);
		hash = (hash * 31) + static_cast<std::uint64_t>(node->type_kind);

		std::vector<ValueNumber> input_vns;
		input_vns.reserve(node->inputs.size());
		for (Node *input : node->inputs)
		{
			ValueNumber input_vn = value_numbers.contains(input) ? value_numbers[input] : 0;
			if (input_vn == 0)
				return 0;
			input_vns.push_back(input_vn);
		}

		if (is_commutative(node->ir_type) && input_vns.size() == 2)
		{
			if (input_vns[0] > input_vns[1])
				std::swap(input_vns[0], input_vns[1]);
		}
		for (const ValueNumber vn : input_vns)
			hash = (hash * 31) + vn;
		return hash == 0 ? 1 : hash;
	}

	ValueNumber CSEPass::compute_load_value_number(Node *node, const LocalAliasResult &alias_result)
	{
		Node *address = nullptr;
		if (node->ir_type == NodeType::LOAD ||
			node->ir_type == NodeType::PTR_LOAD ||
			node->ir_type == NodeType::ATOMIC_LOAD)
		{
			address = node->inputs.empty() ? nullptr : node->inputs[0];
		}

		if (!address)
			return next_value_number++;

		ValueNumber addr_vn = value_numbers.contains(address) ? value_numbers[address] : 0;
		if (addr_vn == 0)
			return 0;

		const MemoryLocation *loc = alias_result.get_location(address);
		auto hash = static_cast<std::uint64_t>(node->ir_type);
		hash = (hash * 31) + static_cast<std::uint64_t>(node->type_kind);
		hash = (hash * 31) + addr_vn;

		if (loc)
		{
			hash = (hash * 31) + std::hash<Node*>{}(loc->base);
			if (loc->offset != -1)
				hash = (hash * 31) + static_cast<std::uint64_t>(loc->offset);
			if (loc->size != 0)
				hash = (hash * 31) + loc->size;
		}

		/* include atomic ordering in hash if present */
		if (node->ir_type == NodeType::ATOMIC_LOAD && node->inputs.size() > 1)
		{
			Node *ordering = node->inputs[1];
			const ValueNumber ordering_vn = value_numbers.contains(ordering) ? value_numbers[ordering] : 0;
			if (ordering_vn == 0)
				return 0;  /* can't compute value number without ordering */
			hash = (hash * 31) + ordering_vn;
		}

		return hash == 0 ? 1 : hash;
	}

	bool CSEPass::is_load_operation(const Node *node)
	{
		return node->ir_type == NodeType::LOAD ||
		       node->ir_type == NodeType::PTR_LOAD ||
		       node->ir_type == NodeType::ATOMIC_LOAD;
	}

	bool CSEPass::has_inputs(const Node *node)
	{
		return !node->inputs.empty();
	}

	bool CSEPass::are_equivalent_expressions(Node *a, Node *b)
	{
		const auto a_vn = value_numbers.find(a);
		const auto b_vn = value_numbers.find(b);

		if (a_vn == value_numbers.end() || b_vn == value_numbers.end())
			return false;

		return a_vn->second == b_vn->second;
	}

	bool CSEPass::is_commutative(const NodeType type)
	{
		switch (type)
		{
			case NodeType::ADD:
			case NodeType::MUL:
			case NodeType::BAND:
			case NodeType::BOR:
			case NodeType::BXOR:
			case NodeType::EQ:
			case NodeType::NEQ:
				return true;

			default:
				return false;
		}
	}

	bool CSEPass::is_eligible_for_cse(const Node *node)
	{
		if (node->ir_type == NodeType::CALL ||
		    node->ir_type == NodeType::STORE ||
		    node->ir_type == NodeType::PTR_STORE ||
		    node->ir_type == NodeType::ATOMIC_STORE ||
		    node->ir_type == NodeType::FREE ||
		    node->ir_type == NodeType::HEAP_ALLOC ||
		    node->ir_type == NodeType::STACK_ALLOC ||
		    node->ir_type == NodeType::RET ||
		    node->ir_type == NodeType::EXIT ||
		    node->ir_type == NodeType::ENTRY ||
		    node->ir_type == NodeType::FUNCTION ||
		    node->ir_type == NodeType::BRANCH ||
		    node->ir_type == NodeType::JUMP ||
		    node->ir_type == NodeType::INVOKE)
		{
			return false;
		}

		if ((node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE)
			return false;

		switch (node->ir_type)
		{
			case NodeType::ADD:
			case NodeType::SUB:
			case NodeType::MUL:
			case NodeType::DIV:
			case NodeType::MOD:
			case NodeType::GT:
			case NodeType::GTE:
			case NodeType::LT:
			case NodeType::LTE:
			case NodeType::EQ:
			case NodeType::NEQ:
			case NodeType::BAND:
			case NodeType::BOR:
			case NodeType::BXOR:
			case NodeType::BNOT:
			case NodeType::BSHL:
			case NodeType::BSHR:
			case NodeType::LOAD:
			case NodeType::PTR_LOAD:
			case NodeType::ADDR_OF:
			case NodeType::PTR_ADD:
			case NodeType::REINTERPRET_CAST:
			case NodeType::ATOMIC_LOAD:
			case NodeType::LIT:
			case NodeType::PARAM:
				return true;

			default:
				return false;
		}
	}

	bool CSEPass::loads_may_alias(Node *a, Node *b, const LocalAliasResult &alias_result)
	{
		if (!a || !b)
			return true;

		if (are_equivalent_expressions(a, b))
			return false;

		Node *addr_a = nullptr;
		Node *addr_b = nullptr;

		if (a->ir_type == NodeType::LOAD ||
		    a->ir_type == NodeType::PTR_LOAD ||
		    a->ir_type == NodeType::ATOMIC_LOAD)
		{
			addr_a = a->inputs.empty() ? nullptr : a->inputs[0];
		}

		if (b->ir_type == NodeType::LOAD ||
		    b->ir_type == NodeType::PTR_LOAD ||
		    b->ir_type == NodeType::ATOMIC_LOAD)
		{
			addr_b = b->inputs.empty() ? nullptr : b->inputs[0];
		}

		if (!addr_a || !addr_b)
			return true;

		return alias_result.may_alias(addr_a, addr_b);
	}

	bool CSEPass::replace_all_uses(Node *node_to_replace, Node *replacement_node)
	{
		if (node_to_replace == replacement_node || node_to_replace->ir_type == NodeType::ENTRY)
			return false;

		for (const std::vector<Node *> users_copy = node_to_replace->users;
		     Node *user : users_copy)
		{
			for (size_t i = 0; i < user->inputs.size(); i++)
			{
				if (user->inputs[i] == node_to_replace)
				{
					user->inputs[i] = replacement_node;
					if (std::ranges::find(replacement_node->users, user) == replacement_node->users.end())
						replacement_node->users.push_back(user);
				}
			}
		}

		node_to_replace->users.clear();
		return true;
	}
}
