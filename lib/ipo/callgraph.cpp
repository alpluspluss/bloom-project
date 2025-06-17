/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/foundation/region.hpp>

namespace blm
{
	CallGraphNode::CallGraphNode(Node *function) : func(function) {}

	void CallGraphNode::add_callee(CallGraphNode *callee)
	{
		if (std::ranges::find(callees, callee) == callees.end())
		{
			callees.push_back(callee);
		}
	}

	void CallGraphNode::add_caller(CallGraphNode *caller)
	{
		if (std::ranges::find(callers, caller) == callers.end())
		{
			callers.push_back(caller);
		}
	}

	bool CallGraphNode::calls(const CallGraphNode *other) const
	{
		return std::ranges::find(callees, other) != callees.end();
	}

	bool CallGraphNode::called_by(const CallGraphNode *other) const
	{
		return std::ranges::find(callers, other) != callers.end();
	}

	void CallGraphNode::add_call_site(Node *call_node)
	{
		call_sites.push_back(call_node);
	}

	CallGraph::~CallGraph() = default;

	CallGraphNode *CallGraph::get_node(Node *function) const
	{
		const auto it = node_map.find(function);
		return it != node_map.end() ? it->second.get() : nullptr;
	}

	CallGraphNode *CallGraph::get_or_create_node(Node *function)
	{
		if (const auto it = node_map.find(function);
			it != node_map.end())
		{
			return it->second.get();
		}

		auto [inserted_it, success] = node_map.emplace(function, std::make_unique<CallGraphNode>(function));
		CallGraphNode *node_ptr = inserted_it->second.get();
		nodes.push_back(node_ptr);
		return node_ptr;
	}

	void CallGraph::add_edge(Node *caller, Node *callee, Node *call_site)
	{
		CallGraphNode *caller_node = get_or_create_node(caller);
		CallGraphNode *callee_node = get_or_create_node(callee);

		caller_node->add_callee(callee_node);
		callee_node->add_caller(caller_node);
		caller_node->add_call_site(call_site);
	}

	std::vector<CallGraphNode *> CallGraph::get_entry_points() const
	{
		std::vector<CallGraphNode *> entry_points;
		for (CallGraphNode *node : nodes)
		{
			if (node->get_callers().empty())
				entry_points.push_back(node);
		}
		return entry_points;
	}

	std::vector<CallGraphNode *> CallGraph::get_leaf_functions() const
	{
		std::vector<CallGraphNode *> leaf_functions;
		for (CallGraphNode *node : nodes)
		{
			if (node->get_callees().empty())
				leaf_functions.push_back(node);
		}
		return leaf_functions;
	}

	bool CallGraph::has_cycles() const
	{
		std::unordered_set<CallGraphNode *> visited;
		std::unordered_set<CallGraphNode *> in_stack;

		for (CallGraphNode *node : nodes)
		{
			if (!visited.contains(node))
			{
				if (dfs_has_cycle(node, visited, in_stack))
					return true;
			}
		}
		return false;
	}

	std::vector<CallGraphNode *> CallGraph::get_post_order() const
	{
		std::vector<CallGraphNode *> post_order;
		std::unordered_set<CallGraphNode *> visited;

		for (CallGraphNode *node : nodes)
		{
			if (!visited.contains(node))
			{
				dfs_post_order(node, visited, post_order);
			}
		}
		return post_order;
	}

	std::vector<CallGraphNode *> CallGraph::get_reverse_post_order() const
	{
		auto post_order = get_post_order();
		std::ranges::reverse(post_order);
		return post_order;
	}

	bool CallGraph::dfs_has_cycle(CallGraphNode *node, // NOLINT(*-no-recursion)
	                              std::unordered_set<CallGraphNode *> &visited,
	                              std::unordered_set<CallGraphNode *> &in_stack) const
	{
		visited.insert(node);
		in_stack.insert(node);

		for (CallGraphNode *callee : node->get_callees())
		{
			if (!visited.contains(callee))
			{
				if (dfs_has_cycle(callee, visited, in_stack))
					return true;
			}
			else if (in_stack.contains(callee))
			{
				return true;
			}
		}

		in_stack.erase(node);
		return false;
	}

	void CallGraph::dfs_post_order(CallGraphNode *node, // NOLINT(*-no-recursion)
	                               std::unordered_set<CallGraphNode *> &visited,
	                               std::vector<CallGraphNode *> &post_order) const
	{
		visited.insert(node);
		for (CallGraphNode *callee : node->get_callees())
		{
			if (!visited.contains(callee))
				dfs_post_order(callee, visited, post_order);
		}

		post_order.push_back(node);
	}

	CallGraphResult::CallGraphResult(std::unique_ptr<CallGraph> graph)
		: call_graph(std::move(graph)) {}

	bool CallGraphResult::invalidated_by(const std::type_info &) const
	{
		return true;
	}

	std::unordered_set<Module*> CallGraphResult::depends_on_modules() const
	{
		return analyzed_modules;
	}

	bool CallGraphAnalysisPass::run(std::vector<Module*>& modules, IPOPassContext& context)
	{
		auto call_graph = std::make_unique<CallGraph>();
		std::vector<Node *> all_functions;
		std::unordered_set<Node *> global_funcs;

		collect_functions(modules, all_functions);
		collect_global_functions(modules, global_funcs);

		for (Node *func : all_functions)
			analyze_function(func, *call_graph, global_funcs, modules);

		auto result = std::make_unique<CallGraphResult>(std::move(call_graph));
		for (Module *module : modules)
			result->analyzed_modules.insert(module);

		std::size_t total_edges = 0;
		for (const auto *node : result->get_call_graph().get_nodes())
			total_edges += node->get_callees().size();

		context.store_result<CallGraphResult>(std::move(result));
		context.update_stat("callgraph.functions_analyzed", all_functions.size());
		context.update_stat("callgraph.global_functions", global_funcs.size());
		context.update_stat("callgraph.total_edges", total_edges);

		return true;
	}

	void CallGraphAnalysisPass::collect_functions(std::vector<Module*>& modules,
	                                              std::vector<Node *> &functions)
	{
		for (Module *module : modules)
		{
			for (Node *func : module->get_functions())
			{
				if (func->ir_type == NodeType::FUNCTION)
					functions.push_back(func);
			}
		}
	}

	void CallGraphAnalysisPass::collect_global_functions(std::vector<Module*>& modules,
	                                                     std::unordered_set<Node *> &global_funcs)
	{
		for (Module *module : modules)
		{
			for (Node *node : module->get_root_region()->get_nodes())
			{
				if (is_function_type(node->type_kind))
				{
					global_funcs.insert(node);
				}
			}
		}
	}

	void CallGraphAnalysisPass::analyze_function(Node *func, CallGraph &graph,
	                                             const std::unordered_set<Node *> &global_funcs,
	                                             std::vector<Module*>& modules)
	{
		if (func->ir_type != NodeType::FUNCTION)
			return;

		const Region *func_region = nullptr;
		for (Module *module : modules)
		{
			for (const Region *child : module->get_root_region()->get_children())
			{
				if (child->get_name() == module->get_context().get_string(func->str_id))
				{
					func_region = const_cast<Region*>(child);
					break;
				}
			}
			if (func_region)
				break;
		}

		if (func_region)
			analyze_region(func_region, func, graph, global_funcs);
	}

	void CallGraphAnalysisPass::analyze_region(const Region *region, Node *caller, CallGraph &graph, // NOLINT(*-no-recursion)
	                                           const std::unordered_set<Node *> &global_funcs)
	{
		if (!region)
			return;

		for (Node *node : region->get_nodes())
		{
			if (node->ir_type == NodeType::CALL || node->ir_type == NodeType::INVOKE)
				handle_call(node, caller, graph, global_funcs);
		}

		for (const Region *child : region->get_children())
			analyze_region(child, caller, graph, global_funcs);
	}

	void CallGraphAnalysisPass::handle_call(Node *call_node, Node *caller, CallGraph &graph,
	                                        const std::unordered_set<Node *> &global_funcs)
	{
		if (is_direct_call(call_node))
		{
			/* direct */
			if (Node *target = get_direct_call_target(call_node))
				graph.add_edge(caller, target, call_node);
		}
		else
		{
			/* indirect call */
			if (!call_node->inputs.empty())
			{
				Node *func_ptr = call_node->inputs[0];
				if (global_funcs.contains(func_ptr))
				{
					graph.add_edge(caller, func_ptr, call_node);
				}
				else
				{
					/* we will assume it could call any address-taken function for safety */
					for (Node *global_func : global_funcs)
					{
						if ((global_func->props & NodeProps::EXPORT) != NodeProps::NONE)
							graph.add_edge(caller, global_func, call_node);
					}
				}
			}
		}
	}

	bool CallGraphAnalysisPass::is_direct_call(Node *call_node)
	{
		if (call_node->inputs.empty())
			return false;
		Node *target = call_node->inputs[0];
		return target->ir_type == NodeType::FUNCTION;
	}

	Node *CallGraphAnalysisPass::get_direct_call_target(Node *call_node)
	{
		if (!is_direct_call(call_node))
			return nullptr;
		return call_node->inputs[0];
	}
}
