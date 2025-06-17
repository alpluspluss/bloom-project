/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <cstring>
#include <functional>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/transform/pre.hpp>

namespace blm
{
	std::string_view PREPass::name() const
	{
		return "partial-redundancy-elimination";
	}

	std::string_view PREPass::description() const
	{
		return "eliminates expressions that are redundantly computed along some execution paths";
	}

	bool PREPass::run(Module &m, PassContext &ctx)
	{
		pre_results.clear();

		std::size_t total_hoisted = 0;

		total_hoisted += process_region(m.get_root_region());

		for (const Node *func : m.get_functions())
		{
			if (func->ir_type != NodeType::FUNCTION)
				continue;

			for (Region *region : m.get_root_region()->get_children())
			{
				if (region->get_name() == m.get_context().get_string(func->str_id))
				{
					total_hoisted += process_region(region);
					break;
				}
			}
		}

		ctx.update_stat("pre.hoisted_expressions", total_hoisted);
		return total_hoisted > 0;
	}

	std::size_t PREPass::process_region(Region *region)
	{
		if (!region)
			return 0;

		ExpressionGroups expr_groups = collect_expressions(region);

		std::size_t hoisted_count = 0;
		for (const auto &[hash, nodes] : expr_groups)
		{
			if (nodes.size() > 1 && try_hoist_expression(nodes))
				hoisted_count++;
		}

		for (Region *child : region->get_children())
			hoisted_count += process_region(child);

		return hoisted_count;
	}

	PREPass::ExpressionGroups PREPass::collect_expressions(Region *region)
	{
		ExpressionGroups groups;

		/* collect eligible expressions from this region and all descendants */
		std::function<void(Region*)> collect = [&](Region* r)
		{
			if (!r) return;

			for (Node *node : r->get_nodes())
			{
				if (is_eligible_for_pre(node))
				{
					ExprHash hash = compute_expr_hash(node);
					groups[hash].push_back(node);
				}
			}

			for (Region *child : r->get_children())
				collect(child);
		};

		collect(region);

		/* filter to only keep groups with multiple equivalent expressions */
		ExpressionGroups filtered;
		for (const auto &[hash, nodes] : groups)
		{
			if (nodes.size() > 1 && are_all_equivalent(nodes))
				filtered[hash] = nodes;
		}

		return filtered;
	}

	bool PREPass::try_hoist_expression(const std::vector<Node *> &nodes)
	{
		Region *target = find_common_dominator(nodes);
		if (!target || !is_safe_hoist_target(target))
			return false;

		Node *template_node = nodes[0];
		if ((template_node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE)
			return false;

		if (!inputs_available_at(template_node, target))
			return false;

		Node *hoisted = create_hoisted_node(template_node, target);
		if (!hoisted)
			return false;

		std::size_t replaced = replace_dominated_occurrences(nodes, hoisted, target);

		PREResult result;
		result.original_node = template_node;
		result.hoisted_node = hoisted;
		result.target_region = target;
		result.instances_removed = replaced;
		pre_results.push_back(result);

		return replaced > 0;
	}

	bool PREPass::are_all_equivalent(const std::vector<Node *> &nodes) const
	{
		if (nodes.empty())
			return false;

		Node *first = nodes[0];
		for (std::size_t i = 1; i < nodes.size(); i++)
		{
			if (!are_expressions_equivalent(first, nodes[i]))
				return false;
		}
		return true;
	}

	bool PREPass::are_expressions_equivalent(Node *a, Node *b) const
	{
		if (a->ir_type != b->ir_type ||
		    a->type_kind != b->type_kind ||
		    a->inputs.size() != b->inputs.size())
			return false;

		/* check properties */
		if ((a->props & NodeProps::NO_OPTIMIZE) != (b->props & NodeProps::NO_OPTIMIZE))
			return false;

		/* for commutative operations, check both input orderings */
		if (is_commutative_operation(a->ir_type) && a->inputs.size() == 2)
		{
			return (a->inputs[0] == b->inputs[0] && a->inputs[1] == b->inputs[1]) ||
			       (a->inputs[0] == b->inputs[1] && a->inputs[1] == b->inputs[0]);
		}

		/* for non-commutative operations, inputs must match exactly */
		for (std::size_t i = 0; i < a->inputs.size(); i++)
		{
			if (a->inputs[i] != b->inputs[i])
				return false;
		}

		return true;
	}

	bool PREPass::is_commutative_operation(NodeType type) const
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

	Region *PREPass::find_common_dominator(const std::vector<Node *> &nodes)
	{
		if (nodes.empty())
			return nullptr;

		/* get region for first node */
		Region *common = nodes[0]->parent_region;
		if (!common)
			return nullptr;

		/* find common dominator with all other nodes */
		for (std::size_t i = 1; i < nodes.size(); i++)
		{
			if (!nodes[i]->parent_region)
				return nullptr;

			common = find_common_dominator(common, nodes[i]->parent_region);
			if (!common)
				return nullptr;
		}

		return common;
	}

	Region *PREPass::find_common_dominator(Region *r1, Region *r2) const
	{
		if (!r1 || !r2)
			return nullptr;

		if (r1 == r2)
			return r1;

		/* use proper dominance analysis */
		if (r1->dominates(r2))
			return r1;
		if (r2->dominates(r1))
			return r2;

		/* find lowest common ancestor that dominates both */
		Region *curr1 = r1->get_parent();
		while (curr1)
		{
			if (curr1->dominates(r2))
				return curr1;
			curr1 = curr1->get_parent();
		}

		Region *curr2 = r2->get_parent();
		while (curr2)
		{
			if (curr2->dominates(r1))
				return curr2;
			curr2 = curr2->get_parent();
		}

		return nullptr;
	}

	bool PREPass::inputs_available_at(Node *node, Region *target) const
	{
		for (Node *input : node->inputs)
		{
			if (!input->parent_region)
				return false;

			/* input must dominate or be in the target region */
			if (!input->parent_region->dominates(target) && input->parent_region != target)
				return false;
		}
		return true;
	}

	bool PREPass::is_safe_hoist_target(Region *target) const
	{
		if (!target)
			return false;

		if (has_unstructured_control_flow(target))
			return false;

		return true;
	}

	bool PREPass::has_unstructured_control_flow(const Region *region) const
	{
		if (!region)
			return false;

		for (Node *node : region->get_nodes())
		{
			/* JUMP and INVOKE can create unstructured control flow */
			if (node->ir_type == NodeType::JUMP || node->ir_type == NodeType::INVOKE)
				return true;
		}

		return false;
	}

	Node *PREPass::create_hoisted_node(const Node *template_node, Region *target)
	{
		Context &ctx = target->get_module().get_context();
		Node *hoisted = ctx.create<Node>();

		hoisted->ir_type = template_node->ir_type;
		hoisted->type_kind = template_node->type_kind;
		hoisted->data = template_node->data;

		for (Node *input : template_node->inputs)
		{
			hoisted->inputs.push_back(input);
			input->users.push_back(hoisted);
		}

		insert_hoisted_node(hoisted, target);

		return hoisted;
	}

	void PREPass::insert_hoisted_node(Node *hoisted, Region *target)
	{
		const auto &nodes = target->get_nodes();
		for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
		{
			Node *node = *it;
			if (node->ir_type == NodeType::RET ||
				node->ir_type == NodeType::EXIT ||
				node->ir_type == NodeType::JUMP ||
				node->ir_type == NodeType::BRANCH ||
				node->ir_type == NodeType::INVOKE)
			{
				continue;
			}

			target->insert_node_after(node, hoisted);
			return;
		}

		target->add_node(hoisted);
	}

	std::size_t PREPass::replace_dominated_occurrences(const std::vector<Node *> &nodes,
	                                                   Node *replacement,
	                                                   const Region *dominator)
	{
		std::size_t replaced = 0;

		for (Node *node : nodes)
		{
			if (!node->parent_region)
				continue;

			if (!dominator->dominates(node->parent_region) && dominator != node->parent_region)
				continue;

			std::vector<Node *> users = node->users;
			for (Node *user : users)
			{
				for (std::size_t i = 0; i < user->inputs.size(); i++)
				{
					if (user->inputs[i] == node)
					{
						user->inputs[i] = replacement;
						replacement->users.push_back(user);
					}
				}

				/* remove from original node's users */
				auto &user_list = node->users;
				user_list.erase(std::ranges::remove(user_list, user).begin(), user_list.end());
			}

			replaced++;
		}

		return replaced;
	}

	ExprHash PREPass::compute_expr_hash(Node *node) const
	{
		if (!node)
			return 0;

		auto hash = static_cast<ExprHash>(node->ir_type);
		hash = (hash * 31) + static_cast<ExprHash>(node->type_kind);

		if (node->ir_type == NodeType::LIT)
		{
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
				default:
					hash = (hash * 31) + reinterpret_cast<std::uintptr_t>(&node->data);
					break;
			}
		}

		/* hash inputs considering commutativity */
		if (is_commutative_operation(node->ir_type) && node->inputs.size() == 2)
		{
			/* sort inputs by address for consistent hashing */
			Node *a = node->inputs[0];
			Node *b = node->inputs[1];

			if (reinterpret_cast<std::uintptr_t>(a) > reinterpret_cast<std::uintptr_t>(b))
				std::swap(a, b);

			hash = (hash * 31) + reinterpret_cast<std::uintptr_t>(a);
			hash = (hash * 31) + reinterpret_cast<std::uintptr_t>(b);
		}
		else
		{
			/* hash inputs in order */
			for (Node *input : node->inputs)
				hash = (hash * 31) + reinterpret_cast<std::uintptr_t>(input);
		}

		return hash;
	}

	bool PREPass::is_eligible_for_pre(const Node *node) const
	{
		if (!node || !node->parent_region)
			return false;

		/* exclude nodes with side effects or control flow */
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
		    node->ir_type == NodeType::JUMP ||
		    node->ir_type == NodeType::BRANCH ||
		    node->ir_type == NodeType::INVOKE ||
		    node->ir_type == NodeType::LOAD ||
		    node->ir_type == NodeType::PTR_LOAD ||
		    node->ir_type == NodeType::ATOMIC_LOAD)
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
				return true;
			default:
				return false;
		}
	}
}
