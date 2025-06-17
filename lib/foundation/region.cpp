/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>

namespace blm
{
	Region::Region(Context &ctx, Module &mod, const std::string_view name, Region *parent) : ctx(ctx),
		module(mod),
		parent(parent),
		debug_info(*this),
		name_id(ctx.intern_string(name))
	{
		if (parent)
			parent->add_child(this);
	}

	Region::~Region()
	{
		/* clear all connections */
		for (Node *node: nodes)
		{
			if (node)
				node->parent_region = nullptr;
		}

		nodes.clear();
		children.clear();
		control_dependency = nullptr;
	}

	std::string_view Region::get_name() const
	{
		return ctx.get_string(name_id);
	}

	void Region::add_child(Region *child)
	{
		if (child && std::ranges::find(children, child) == children.end())
		{
			children.push_back(child);
			child->parent = this;
		}
	}

	void Region::add_node(Node *node)
	{
		if (node && std::ranges::find(nodes, node) == nodes.end())
		{
			nodes.push_back(node);
			node->parent_region = this;
		}
	}

	void Region::remove_node(Node *node)
	{
		if (!node)
			return;

		if (const auto it = std::ranges::find(nodes, node);
			it != nodes.end())
		{
			node->parent_region = nullptr;
			nodes.erase(it);
		}
	}

	void Region::insert_node_before(Node *before, Node *node)
	{
		if (!node)
			return;

		if (std::ranges::find(nodes, node) != nodes.end())
			return;

		if (const auto it = std::ranges::find(nodes, before);
			it != nodes.end())
		{
			nodes.insert(it, node);
			node->parent_region = this;
		}
		else
		{
			/* if before node not found, append to end */
			add_node(node);
		}
	}

	void Region::insert_node_after(Node *after, Node *node)
	{
		if (!node || !after)
			return;

		if (const auto it = std::ranges::find(nodes, after);
			it != nodes.end())
		{
			nodes.insert(it + 1, node);
			node->parent_region = this;
		}
		else
		{
			/* if after node not found, append to end */
			add_node(node);
		}
	}

	void Region::insert_at_beginning(Node *node)
	{
		if (!node)
			return;

		if (std::ranges::find(nodes, node) != nodes.end())
			return;

		nodes.insert(nodes.begin(), node);
		node->parent_region = this;
	}

	bool Region::is_terminated() const
	{
		if (nodes.empty())
			return false;

		const Node *last = nodes.back();
		if (!last)
			return false;

		/* note: EXIT is not considered a terminator as it can be used
		 * to restore stack register */
		switch (last->ir_type)
		{
			case NodeType::RET:
			case NodeType::JUMP:
			case NodeType::BRANCH:
			case NodeType::INVOKE:
				return true;
			default:
				return false;
		}
	}

	bool Region::dominates(const Region *possible_dominated) const
	{
		if (!possible_dominated)
			return false;

		if (this == possible_dominated)
			return true;

		/* check if this region has unstructured control flow that might bypass
		 * normal parent-child dominance */
		if (has_unstructured_jumps_to(possible_dominated))
			return false;

		/* check if any ancestor region has unstructured control flow that might
		 * allow possible_dominated to be reached without going through this region */
		const Region *ancestor = this->get_parent();
		while (ancestor)
		{
			if (ancestor->has_unstructured_jumps_to(possible_dominated))
				return false;
			ancestor = ancestor->get_parent();
		}

		/* standard tree-based dominance check */
		ancestor = possible_dominated->get_parent();
		while (ancestor)
		{
			if (ancestor == this)
				return true;
			ancestor = ancestor->get_parent();
		}

		return false;
	}

	bool Region::has_unstructured_jumps_to(const Region *target) const
	{
		if (!target)
			return false;

		for (const Node *node: nodes)
		{
			/* check JUMP nodes */
			if (node->ir_type == NodeType::JUMP)
			{
				if (!node->inputs.empty())
				{
					if (const Node *target_entry = node->inputs[0];
						target_entry && target_entry->parent_region == target)
					{
						/* this is a jump to the target region;
						 * we need to check if it breaks dominance */
						if (!this->dominates_via_tree(target))
							return true;
					}
				}
			}
			/* check BRANCH nodes */
			else if (node->ir_type == NodeType::BRANCH)
			{
				if (node->inputs.size() >= 3)
				{
					/* inputs[1] = ENTRY node for true target */
					/* inputs[2] = ENTRY node for false target */
					const Node *true_entry = node->inputs[1];
					const Node *false_entry = node->inputs[2];

					if ((true_entry && true_entry->parent_region == target) ||
					    (false_entry && false_entry->parent_region == target))
					{
						/* this is a branch to the target region */
						if (!this->dominates_via_tree(target))
							return true;
					}
				}
			}
			/* check INVOKE nodes */
			else if (node->ir_type == NodeType::INVOKE)
			{
				if (node->inputs.size() >= 2)
				{
					/* last two inputs are ENTRY nodes for normal/exception paths */
					const Node *normal_entry = node->inputs[node->inputs.size() - 2];
					const Node *exception_entry = node->inputs[node->inputs.size() - 1];

					if ((normal_entry && normal_entry->parent_region == target) ||
					    (exception_entry && exception_entry->parent_region == target))
					{
						if (!this->dominates_via_tree(target))
							return true;
					}
				}
			}
		}

		return false;
	}

	bool Region::dominates_via_tree(const Region *possible_dominated) const
	{
		if (!possible_dominated)
			return false;

		if (this == possible_dominated)
			return true;

		/* pure tree-based dominance; parent always dominates children */
		const Region *ancestor = possible_dominated->get_parent();
		while (ancestor)
		{
			if (ancestor == this)
				return true;
			ancestor = ancestor->get_parent();
		}

		return false;
	}

	bool Region::replace_node(Node *old_node, Node *new_node, const bool update_connections)
	{
		if (!old_node || !new_node)
			return false;

		const auto it = std::ranges::find(nodes, old_node);
		if (it == nodes.end())
			return false;

		/* replace in node list */
		*it = new_node;
		new_node->parent_region = this;
		old_node->parent_region = nullptr;

		if (update_connections)
		{
			/* update all users of old_node to point to new_node */
			for (const auto users = old_node->users;
			     Node *user: users)
			{
				for (std::size_t i = 0; i < user->inputs.size(); i++)
				{
					if (user->inputs[i] == old_node)
					{
						user->inputs[i] = new_node;
						if (std::ranges::find(new_node->users, user) == new_node->users.end())
							new_node->users.push_back(user);
					}
				}
			}

			/* transfer inputs from old_node to new_node if new_node has none */
			if (new_node->inputs.empty() && !old_node->inputs.empty())
			{
				for (Node *input: old_node->inputs)
				{
					new_node->inputs.push_back(input);

					/* update input's users list */
					auto &input_users = input->users;
					if (const auto user_it = std::ranges::find(input_users, old_node);
						user_it != input_users.end())
					{
						*user_it = new_node;
					}
					else
					{
						input_users.push_back(new_node);
					}
				}
			}

			/* clear old connections */
			old_node->users.clear();
			old_node->inputs.clear();
		}

		return true;
	}
}
