/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <queue>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/transform/adce.hpp>

namespace blm
{
	std::string_view ADCEPass::name() const
	{
		return "aggressive-dead-code-elimination";
	}

	std::string_view ADCEPass::description() const
	{
		return "aggressively removes unreachable code and dead control flow";
	}

	bool ADCEPass::run(Module& m, PassContext& ctx)
	{
		reachable_regions.clear();
		live_nodes.clear();
		dead_regions.clear();

		mark_reachable_regions(m);
		mark_live_nodes(m);
		const std::size_t removed_regions = remove_unreachable_regions(m);
		const std::size_t removed_nodes = remove_dead_nodes(m);
		const auto total_removed = removed_regions + removed_nodes;
		ctx.update_stat("adce.removed_regions", removed_regions);
		ctx.update_stat("adce.removed_nodes", removed_nodes);
		return total_removed > 0;
	}

	void ADCEPass::mark_reachable_regions(Module& m)
	{
		for (Node* func : m.get_functions())
		{
			if (func->ir_type == NodeType::FUNCTION)
			{
				if (const Region* func_region = find_function_region(func, m))
					mark_region_reachable(m, func_region);
			}
		}
		mark_region_reachable(m, m.get_root_region()); /* also mark the module's root region as reachable */
		if (const Region* rodata = m.get_rodata_region())
			mark_region_reachable(m, rodata); /* .__rodata is always alive */
	}

	void ADCEPass::mark_region_reachable(Module& m, const Region* region) // NOLINT(*-no-recursion)
	{
		if (!region || reachable_regions.contains(const_cast<Region*>(region)))
			return;

		reachable_regions.insert(const_cast<Region*>(region));

		/* follow control flow to mark successor regions */
		for (const Node* node : region->get_nodes())
		{
			switch (node->ir_type)
			{
				case NodeType::JUMP:
					if (!node->inputs.empty())
						mark_target_region_reachable(m, node->inputs[0]);
					break;

				case NodeType::BRANCH:
					if (node->inputs.size() >= 3)
					{
						mark_target_region_reachable(m, node->inputs[1]); /* true target */
						mark_target_region_reachable(m, node->inputs[2]); /* false target */
					}
					break;

				case NodeType::INVOKE:
					/* INVOKE: last two inputs are normal/exception targets */
					if (node->inputs.size() >= 2)
					{
						mark_target_region_reachable(m, node->inputs[node->inputs.size() - 2]);
						mark_target_region_reachable(m, node->inputs[node->inputs.size() - 1]);
					}
					break;

				case NodeType::CALL:
					/* calls can reach other functions */
					if (!node->inputs.empty())
					{
						if (Region* callee_region = find_function_region(node->inputs[0], m))
							mark_region_reachable(m, callee_region);
					}
					break;

				default:
					break;
			}
		}
	}

	void ADCEPass::mark_target_region_reachable(Module& m, const Node* target_entry) // NOLINT(*-no-recursion)
	{
		if (!target_entry || target_entry->ir_type != NodeType::ENTRY)
			return;

		if (const Region* target_region = target_entry->parent_region)
			mark_region_reachable(m, target_region);
	}

	void ADCEPass::mark_live_nodes(Module&)
	{
		std::queue<Node*> worklist;
		for (Region* region : reachable_regions)
		{
			for (Node* node : region->get_nodes())
			{
				if (is_critical_node(node))
				{
					live_nodes.insert(node);
					worklist.push(node);
				}
			}
		}

		/* propagate liveness backwards through def-use chains */
		while (!worklist.empty())
		{
			const Node* current = worklist.front();
			worklist.pop();
			for (Node* input : current->inputs)
			{
				if (input && live_nodes.insert(input).second)
					worklist.push(input); /* live node */
			}
		}
	}

	bool ADCEPass::is_critical_node(Node* node)
	{
		if (!node)
			return false;

		if (node->ir_type == NodeType::FUNCTION)
			return true;

		switch (node->ir_type)
		{
			/* structural/control flow and side effects nodes */
			case NodeType::ENTRY:
			case NodeType::EXIT:
			case NodeType::FUNCTION:
			case NodeType::RET:
			case NodeType::JUMP:
			case NodeType::BRANCH:
			case NodeType::INVOKE:
			case NodeType::STORE:
			case NodeType::PTR_STORE:
			case NodeType::ATOMIC_STORE:
			case NodeType::FREE:
			case NodeType::CALL:
				return true;
			default: /* nodes marked as non-optimizable */
				return (node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE;
		}
	}

	std::size_t ADCEPass::remove_unreachable_regions(Module& m)
	{
		std::size_t removed = 0;

		/* collect all regions in the module */
		std::vector<Region*> all_regions;
		collect_all_regions(m.get_root_region(), all_regions);

		/* remove unreachable regions */
		for (Region* region : all_regions)
		{
			if (!reachable_regions.contains(region))
			{
				dead_regions.insert(region);
				removed++;
			}
		}

		/* actually remove dead regions from their parents */
		for (Region* dead_region : dead_regions)
		{
			if (const Region* parent = dead_region->get_parent())
			{
				/* remove from parent's children list */
				auto& children = const_cast<std::vector<Region*>&>(parent->get_children());
				std::erase(children, dead_region);
			}
		}

		return removed;
	}

	void ADCEPass::collect_all_regions(const Region* region, std::vector<Region*>& all_regions) const // NOLINT(*-no-recursion)
	{
		if (!region)
			return;

		all_regions.push_back(const_cast<Region*>(region));
		for (const Region* child : region->get_children())
			collect_all_regions(child, all_regions);
	}

	std::size_t ADCEPass::remove_dead_nodes(Module&) const
	{
		std::size_t removed = 0;
		for (Region* region : reachable_regions)
		{
			std::vector<Node*> nodes_to_remove;
			for (Node* node : region->get_nodes())
			{
				if (!live_nodes.contains(node))
					nodes_to_remove.push_back(node);
			}
			for (Node* dead_node : nodes_to_remove)
			{
				remove_dead_node(dead_node, region);
				removed++;
			}
		}

		return removed;
	}

	void ADCEPass::remove_dead_node(Node* node, Region* region)
	{
		if (!node || !region)
			return;

		for (Node* input : node->inputs)
		{
			if (input)
				std::erase(input->users, node);
		}
		region->remove_node(node); /* remove from region */
	}

	Region* ADCEPass::find_function_region(Node* function, Module& m) const
	{
		if (!function || function->ir_type != NodeType::FUNCTION)
			return nullptr;

		const std::string_view func_name = m.get_context().get_string(function->str_id);
		return find_region_by_name(m.get_root_region(), func_name); /* search for a region with matching name */
	}

	Region* ADCEPass::find_region_by_name(const Region* region, std::string_view name) const // NOLINT(*-no-recursion)
	{
		if (!region)
			return nullptr;

		if (region->get_name() == name)
			return const_cast<Region*>(region);

		for (const Region* child : region->get_children())
		{
			if (Region* found = find_region_by_name(child, name))
				return found;
		}

		return nullptr;
	}
}
