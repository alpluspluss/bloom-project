/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/transform/reassociate.hpp>

namespace blm
{
	std::string_view ReassociatePass::name() const
	{
		return "reassociate";
	}

	std::string_view ReassociatePass::description() const
	{
		return "reorder associative expression for better optimization opportunity in next phrases";
	}

	std::vector<const std::type_info *> ReassociatePass::required_passes() const
	{
		return get_pass_types<LocalAliasAnalysisPass>();
	}

	bool ReassociatePass::run(Module &m, PassContext &ctx)
	{
		/* store module reference for later use */
		current_module = &m;
		reassociated_count = 0;

		bool changed = process_region(m.get_root_region());

		/* update statistics if changes were made */
		if (changed)
			ctx.update_stat("reassociate.count", reassociated_count);

		return changed;
	}

	bool ReassociatePass::process_region(const Region *region)
	{
		if (!region)
			return false;

		auto changed = false;
		std::vector<Node *> nodes_to_process = region->get_nodes();
		for (Node *n: nodes_to_process)
		{
			if (reassociate(n))
				changed = true;
		}

		for (Region *child: region->get_children())
		{
			if (process_region(child))
				changed = true;
		}
		return changed;
	}

	bool ReassociatePass::reassociate(Node *node)
	{
		if (!node || !is_reassociable(node->ir_type))
			return false;

		if ((node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE)
			return false; /* volatile nodes are guarunteed to be non-reassociable */

		std::vector<Node *> constants;
		std::vector<Node *> variables;
		extract_operands(node, node->ir_type, constants, variables);

		/* zero benefits to reassociate when the size is this low; maybe this will change if proven cheap */
		if (constants.size() < 2 && (constants.size() + variables.size()) <= 2)
			return false;

		Region *reg = find_containing_region(node);
		if (!reg)
			return false;

		Node *const_part = nullptr;
		if (!constants.empty())
			const_part = create_balanced_tree(reg, node->ir_type, node->type_kind, constants, node);

		Node *var_part = nullptr;
		if (!variables.empty())
			var_part = create_balanced_tree(reg, node->ir_type, node->type_kind, variables, node);

		Node *res = nullptr;
		if (const_part && var_part)
		{
			res = reg->get_module().get_context().create<Node>();
			res->ir_type = node->ir_type;
			res->type_kind = node->type_kind;

			res->inputs.push_back(const_part);
			res->inputs.push_back(var_part);
			const_part->users.push_back(res);
			var_part->users.push_back(res);

			reg->insert_node_before(node, res);
		}
		else if (const_part)
		{
			res = const_part;
		}
		else if (var_part)
		{
			res = var_part;
		}
		else
		{
			return false;
		}

		replace_all_uses(node, res);
		reassociated_count++;
		return true;
	}

	Region *ReassociatePass::find_containing_region(Node *node)
	{
		if (!node || !current_module)
			return nullptr;

		return find_region_containing_node(current_module->get_root_region(), node);
	}

	Region *ReassociatePass::find_region_containing_node(Region *region, Node *node) // NOLINT(*-no-recursion)
	{
		if (!region)
			return nullptr;

		const auto &nodes = region->get_nodes();
		if (std::ranges::find(nodes, node) != nodes.end())
			return region;

		for (Region *child: region->get_children())
		{
			if (Region *result = find_region_containing_node(child, node))
				return result;
		}

		return nullptr;
	}

	bool ReassociatePass::is_reassociable(const NodeType type) const
	{
		switch (type)
		{
			case NodeType::ADD:
			case NodeType::MUL:
			case NodeType::BAND:
			case NodeType::BOR:
			case NodeType::BXOR:
				return true;
			default:
				return false;
		}
	}

	void ReassociatePass::extract_operands(Node *node, NodeType type, std::vector<Node *> &constants,
	                                       std::vector<Node *> &variables)
	{
		if (!node)
			return;

		if (node->ir_type == type)
		{
			/* same operations, recursively extract its inputs */
			for (Node *in: node->inputs)
				extract_operands(in, node->ir_type, constants, variables);
		}
		else
		{
			if (is_constant(node))
				constants.push_back(node);
			else
				variables.push_back(node);
		}
	}

	Node *ReassociatePass::create_balanced_tree(Region *region, const NodeType op_type, const DataType type_kind,
	                                            const std::vector<Node *> &operands, Node* insertion_point) const
	{
		if (operands.empty())
			return nullptr;

		if (operands.size() == 1)
			return operands[0];

		std::vector<Node *> current_level = operands;
		while (current_level.size() > 1)
		{
			std::vector<Node *> next_lvl;
			for (std::size_t i = 0; i < current_level.size(); i += 2)
			{
				if (i + 1 < current_level.size())
				{
					/* Create node using context to avoid automatic placement */
					Node *new_node = region->get_module().get_context().create<Node>();
					new_node->ir_type = op_type;
					new_node->type_kind = type_kind;

					new_node->inputs.push_back(current_level[i]);
					new_node->inputs.push_back(current_level[i + 1]);
					current_level[i]->users.push_back(new_node);
					current_level[i + 1]->users.push_back(new_node);

					/* Insert the new node before the insertion point */
					region->insert_node_before(insertion_point, new_node);

					next_lvl.push_back(new_node);
				}
				else
				{
					/* odd number of nodes, pass the last one up */
					next_lvl.push_back(current_level[i]);
				}
			}
			current_level = std::move(next_lvl);
		}
		return current_level[0];
	}

	void ReassociatePass::replace_all_uses(Node *old_node, Node *new_node)
	{
		if (old_node == new_node)
			return;

		/* Make a copy of the users list to avoid modification while iterating */
		std::vector<Node *> old_users = old_node->users;

		for (Node *usr: old_users)
		{
			if (!usr)
				continue;

			for (std::size_t i = 0; i < usr->inputs.size(); i++)
			{
				if (usr->inputs[i] == old_node)
				{
					usr->inputs[i] = new_node;
					if (std::ranges::find(new_node->users, usr) == new_node->users.end())
						new_node->users.push_back(usr);
				}
			}
		}

		/* Clear the old node's users list */
		old_node->users.clear();

		/* Remove the old node from the region if it has no more uses */
		Region* region = find_containing_region(old_node);
		if (region && old_node->users.empty())
		{
			region->remove_node(old_node);
		}
	}

	bool ReassociatePass::is_constant(const Node *node)
	{
		if (!node)
			return false;

		if (node->ir_type == NodeType::LIT)
			return true;

		if (is_reassociable(node->ir_type))
		{
			for (Node *in: node->inputs)
			{
				if (!is_constant(in))
					return false;
			}
			return !node->inputs.empty();
		}
		return false;
	}
}
