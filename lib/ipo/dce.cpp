/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <functional>
#include <queue>
#include <bloom/foundation/region.hpp>
#include <bloom/ipo/dce.hpp>

namespace blm
{
	bool IPODCEPass::run(std::vector<Module*>& modules, IPOPassContext& context)
	{
		const auto* cg_result = context.get_result<CallGraphResult>();
		if (!cg_result)
		{
			auto cg_pass = CallGraphAnalysisPass();
			cg_pass.run(modules, context);
			cg_result = context.get_result<CallGraphResult>();
			if (!cg_result) /* from retrying the pass */
				return false;
		}

		std::unordered_set<Node*> reachable_functions;
		mark_entry_points(modules, reachable_functions);
		propagate_reachability(cg_result->get_call_graph(), reachable_functions);
		std::size_t total_removed = 0;
		for (Module* module : modules)
			total_removed += remove_unreachable_functions(module, reachable_functions);

		context.update_stat("ipo_dce.removed_functions", total_removed);
		return total_removed > 0;
	}

	void IPODCEPass::mark_entry_points(std::vector<Module*>& modules, std::unordered_set<Node*>& reachable)
	{
		for (Module* module : modules)
		{
			for (Node* function : module->get_functions())
			{
				if (function->ir_type == NodeType::FUNCTION && is_entry_point(function))
					reachable.insert(function);
			}
		}
	}

	void IPODCEPass::propagate_reachability(const CallGraph& call_graph, std::unordered_set<Node*>& reachable)
	{
		std::queue<Node*> worklist;
		for (Node* entry : reachable)
			worklist.push(entry);

		while (!worklist.empty())
		{
			Node* current = worklist.front();
			worklist.pop();
			const CallGraphNode* cg_node = call_graph.get_node(current);
			if (!cg_node)
				continue;

			for (const CallGraphNode* callee_node : cg_node->get_callees())
			{
				if (Node* callee_function = callee_node->get_function();
					callee_function && reachable.insert(callee_function).second)
				{
					worklist.push(callee_function);
				}
			}
		}
	}

	std::size_t IPODCEPass::remove_unreachable_functions(Module* module, const std::unordered_set<Node*>& reachable)
	{
		std::vector<Node*> functions_to_remove;
		for (Node* function : module->get_functions())
		{
			if (function->ir_type == NodeType::FUNCTION && !reachable.contains(function))
				functions_to_remove.push_back(function);
		}
		for (Node* function : functions_to_remove)
			remove_function_and_region(module, function);

		return functions_to_remove.size();
	}

	bool IPODCEPass::is_entry_point(Node* function)
	{
		if (!function || function->ir_type != NodeType::FUNCTION)
			return false;

		if ((function->props & NodeProps::DRIVER) != NodeProps::NONE)
			return true;

		if ((function->props & NodeProps::EXPORT) != NodeProps::NONE)
			return true;

		return false;
	}

	void IPODCEPass::remove_function_and_region(Module* module, Node* function)
	{
		if (Region* function_region = find_function_region(module, function))
		{
			if (Region* parent = function_region->get_parent())
			{
				auto& children = const_cast<std::vector<Region*>&>(parent->get_children());
				std::erase(children, function_region);
			}
		}

		auto& functions = const_cast<std::vector<Node*>&>(module->get_functions());
		std::erase(functions, function);
		for (Node* input : function->inputs)
		{
			if (input)
			{
				auto& users = input->users;
				std::erase(users, function);
			}
		}

		function->inputs.clear();
		function->users.clear();
	}

	Region* IPODCEPass::find_function_region(Module* module, Node* function)
	{
		if (!function || function->ir_type != NodeType::FUNCTION)
			return nullptr;

		const std::string_view function_name = module->get_context().get_string(function->str_id);
		const std::function<Region*(const Region*)> search_region = [&](const Region* region) -> Region*
		{
			if (!region)
				return nullptr;

			if (region->get_name() == function_name)
				return const_cast<Region*>(region);

			for (const Region* child : region->get_children())
			{
				if (Region* found = search_region(child))
					return found;
			}

			return nullptr;
		};

		return search_region(module->get_root_region());
	}
}
