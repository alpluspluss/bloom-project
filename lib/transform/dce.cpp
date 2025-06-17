/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <queue>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/support/relation.hpp>
#include <bloom/transform/dce.hpp>

namespace blm
{
	// ReSharper disable once CppMemberFunctionMayBeStatic
	std::string_view DCEPass::name() const // NOLINT(*-convert-member-functions-to-static)
	{
		return "dead-code-elimination";
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	std::string_view DCEPass::description() const // NOLINT(*-convert-member-functions-to-static)
	{
		return "eliminates code that has no observable effects";
	}

	bool DCEPass::run(Module &m, PassContext &ctx)
	{
		alive.clear();
		dead.clear();

		find_live_nodes(m.get_root_region());
		for (const Node *fn: m.get_functions())
		{
			if (fn->ir_type != NodeType::FUNCTION)
				continue;

			for (const Region *c: m.get_root_region()->get_children())
			{
				if (c->get_name() == m.get_context().get_string(fn->str_id))
				{
					find_live_nodes(c);
					break;
				}
			}
		}

		find_dead_nodes(m.get_root_region());
		const auto removed = static_cast<std::int64_t>(remove_dead_nodes(m.get_root_region()));

		ctx.update_stat("dce.removed_nodes", removed);
		return removed > 0;
	}

	void DCEPass::find_live_nodes(const Region *region) // NOLINT(*-no-recursion)
	{
		if (!region)
			return;

		std::queue<Node *> worklist;
		for (Node *node: region->get_nodes())
		{
			if (is_root_node(node))
			{
				worklist.push(node);
				alive.insert(node);
			}
		}

		while (!worklist.empty())
		{
			const Node *current = worklist.front();
			worklist.pop();

			for (Node *input: current->inputs)
			{
				/* node was not previously marked alive */
				if (alive.insert(input).second)
					worklist.push(input);
			}
		}

		for (const Region *child: region->get_children())
			find_live_nodes(child);
	}

	void DCEPass::find_dead_nodes(const Region *region) // NOLINT(*-no-recursion)
	{
		if (!region)
			return;

		/* any node that isn't in the alive set is considered dead */
		for (Node *node: region->get_nodes())
		{
			if (!alive.contains(node))
				dead.insert(node);
		}

		for (const Region *child: region->get_children())
			find_dead_nodes(child);
	}

	std::size_t DCEPass::remove_dead_nodes(Region *region) // NOLINT(*-no-recursion)
	{
		if (!region || dead.empty())
			return 0;

		std::size_t removed = 0;
		for (Node *node: dead)
		{
			/* remove from users list of all inputs */
			for (Node *input: node->inputs)
			{
				auto &users = input->users;
				std::erase(users, node);
			}

			/* remove from this region if it belongs here */
			region->remove_node(node);
			removed++;
		}

		for (Region *child: region->get_children())
			remove_dead_nodes(child);
		return removed;
	}

	bool DCEPass::is_root_node(const Node *node)
	{
		if (is_global_scope(node->parent_region))
			return true;

		/* structural nodes should be preserved */
		if (node->ir_type == NodeType::ENTRY ||
		    node->ir_type == NodeType::FUNCTION)
		{
			return true;
		}

		/* return nodes, volatile operations, and nodes with side effects are roots */
		if (node->ir_type == NodeType::RET ||
		    node->ir_type == NodeType::EXIT ||
		    node->ir_type == NodeType::PARAM)
		{
			return true;
		}

		/* control flow nodes must be preserved */
		if (node->ir_type == NodeType::BRANCH ||
		    node->ir_type == NodeType::JUMP ||
		    node->ir_type == NodeType::INVOKE)
		{
			return true;
		}

		/* side effects */
		if (node->ir_type == NodeType::STORE ||
		    node->ir_type == NodeType::PTR_STORE ||
		    node->ir_type == NodeType::ATOMIC_STORE ||
		    node->ir_type == NodeType::FREE)
		{
			return true;
		}

		/* check for CALL nodes with potential side effects */
		if (node->ir_type == NodeType::CALL)
		{
			/* we could be more precise here by checking if the called function
			 * has side effects, but conservatively assume all calls are roots */
			/* TLDR; the current best way to safely perform DCE is through IPO */
			return true;
		}

		return (node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE;
	}
}
