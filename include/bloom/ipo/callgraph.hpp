/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/ipo/pass.hpp>
#include <bloom/ipo/pass-context.hpp>

namespace blm
{
	/**
	 * @brief Represents a single node in the call graph
	 */
	class CallGraphNode
	{
	public:
		/**
		 * @brief Construct a new call graph node
		 *
		 * @param function The function this node represents
		 */
		explicit CallGraphNode(Node *function);

		/**
		 * @brief Get the function this node represents
		 */
		[[nodiscard]] Node *get_function() const
		{
			return func;
		}

		/**
		 * @brief Add a callee to this node
		 *
		 * @param callee The function being called
		 */
		void add_callee(CallGraphNode *callee);

		/**
		 * @brief Add a caller to this node
		 *
		 * @param caller The function calling this one
		 */
		void add_caller(CallGraphNode *caller);

		/**
		 * @brief Get all functions this function calls
		 */
		[[nodiscard]] const std::vector<CallGraphNode *> &get_callees() const
		{
			return callees;
		}

		/**
		 * @brief Get all functions that call this function
		 */
		[[nodiscard]] const std::vector<CallGraphNode *> &get_callers() const
		{
			return callers;
		}

		/**
		 * @brief Check if this function calls another function
		 *
		 * @param other The function to check
		 */
		[[nodiscard]] bool calls(const CallGraphNode *other) const;

		/**
		 * @brief Check if this function is called by another function
		 *
		 * @param other The function to check
		 */
		[[nodiscard]] bool called_by(const CallGraphNode *other) const;

		/**
		 * @brief Get the number of call sites in this function
		 */
		[[nodiscard]] std::size_t get_call_count() const
		{
			return call_sites.size();
		}

		/**
		 * @brief Get all call sites in this function
		 */
		[[nodiscard]] const std::vector<Node *> &get_call_sites() const
		{
			return call_sites;
		}

		/**
		 * @brief Add a call site to this function
		 *
		 * @param call_node The call node
		 */
		void add_call_site(Node *call_node);

	private:
		Node *func;
		std::vector<CallGraphNode *> callees;
		std::vector<CallGraphNode *> callers;
		std::vector<Node *> call_sites;
	};

	/**
	 * @brief Represents the call graph of a module
	 */
	class CallGraph
	{
	public:
		/**
		 * @brief Construct an empty call graph
		 */
		CallGraph() = default;

		/**
		 * @brief Destructor
		 */
		~CallGraph();

		CallGraph(const CallGraph &) = delete;
		CallGraph &operator=(const CallGraph &) = delete;
		CallGraph(CallGraph &&) = delete;
		CallGraph &operator=(CallGraph &&) = delete;

		/**
		 * @brief Get a call graph node for a function
		 *
		 * @param function The function to get the node for
		 * @return Call graph node, or nullptr if not found
		 */
		[[nodiscard]] CallGraphNode *get_node(Node *function) const;

		/**
		 * @brief Get or create a call graph node for a function
		 *
		 * @param function The function to get the node for
		 * @return Call graph node
		 */
		CallGraphNode *get_or_create_node(Node *function);

		/**
		 * @brief Add an edge between two functions
		 *
		 * @param caller The calling function
		 * @param callee The called function
		 * @param call_site The call node
		 */
		void add_edge(Node *caller, Node *callee, Node *call_site);

		/**
		 * @brief Get all functions in the call graph
		 */
		[[nodiscard]] const std::vector<CallGraphNode *> &get_nodes() const
		{
			return nodes;
		}

		/**
		 * @brief Get the number of functions in the call graph
		 */
		[[nodiscard]] std::size_t size() const
		{
			return nodes.size();
		}

		/**
		 * @brief Check if the call graph is empty
		 */
		[[nodiscard]] bool empty() const
		{
			return nodes.empty();
		}

		/**
		 * @brief Get entry points (functions with no callers)
		 */
		[[nodiscard]] std::vector<CallGraphNode *> get_entry_points() const;

		/**
		 * @brief Get leaf functions (functions that call nothing)
		 */
		[[nodiscard]] std::vector<CallGraphNode *> get_leaf_functions() const;

		/**
		 * @brief Check if the call graph has cycles
		 */
		[[nodiscard]] bool has_cycles() const;

		/**
		 * @brief Get functions in topological order for bottom-up analysis
		 */
		[[nodiscard]] std::vector<CallGraphNode *> get_post_order() const;

		/**
		 * @brief Get functions in reverse topological order for top-down analysis
		 */
		[[nodiscard]] std::vector<CallGraphNode *> get_reverse_post_order() const;

	private:
		std::unordered_map<Node *, std::unique_ptr<CallGraphNode>> node_map;
		std::vector<CallGraphNode *> nodes;

		/**
		 * @brief Perform depth-first search for cycle detection
		 *
		 * @param node Current node
		 * @param visited Set of visited nodes
		 * @param in_stack Nodes currently in the recursion stack
		 * @return True if cycle found
		 */
		bool dfs_has_cycle(CallGraphNode *node,
		                   std::unordered_set<CallGraphNode *> &visited,
		                   std::unordered_set<CallGraphNode *> &in_stack) const;

		/**
		 * @brief Perform depth-first search for post-order traversal
		 *
		 * @param node Current node
		 * @param visited Set of visited nodes
		 * @param post_order Result vector
		 */
		void dfs_post_order(CallGraphNode *node,
		                    std::unordered_set<CallGraphNode *> &visited,
		                    std::vector<CallGraphNode *> &post_order) const;
	};

	/**
	 * @brief Result of call graph analysis
	 */
	class CallGraphResult : public IPOAnalysisResult
	{
	public:
		/**
		 * @brief Construct call graph result
		 *
		 * @param graph The call graph
		 */
		explicit CallGraphResult(std::unique_ptr<CallGraph> graph);

		/**
		 * @brief Get the call graph
		 */
		[[nodiscard]] const CallGraph &get_call_graph() const
		{
			return *call_graph;
		}

		/**
		 * @brief Get the call graph (mutable)
		 */
		CallGraph &get_call_graph()
		{
			return *call_graph;
		}

		/**
		 * @brief Check if this result is invalidated by a transform pass
		 *
		 * @param transform_type The type of the transform pass
		 * @return True if invalidated
		 */
		[[nodiscard]] bool invalidated_by(const std::type_info &transform_type) const override;

		/**
		 * @brief Get the modules this analysis depends on
		 *
		 * @return Set of modules this result depends on
		 */
		[[nodiscard]] std::unordered_set<Module*> depends_on_modules() const override;

	private:
		std::unique_ptr<CallGraph> call_graph;
		std::unordered_set<Module*> analyzed_modules;

		friend class CallGraphAnalysisPass;
	};

	/**
	 * @brief IPO pass that builds the call graph
	 */
	class CallGraphAnalysisPass : public IPOPass
	{
	public:
		/**
		 * @brief Get the name of this pass
		 */
		[[nodiscard]] std::string_view name() const override
		{
			return "call-graph-analysis";
		}

		/**
		 * @brief Get the description of this pass
		 */
		[[nodiscard]] std::string_view description() const override
		{
			return "builds the call graph for interprocedural analysis";
		}

		/**
		 * @brief Get the type information for this pass
		 */
		[[nodiscard]] const std::type_info& blm_id() const override
		{
			return typeid(*this);
		}

		/**
		 * @brief Execute the call graph analysis
		 *
		 * @param modules Vector of modules to analyze
		 * @param context The IPO pass context
		 * @return True if analysis completed successfully
		 */
		bool run(std::vector<Module*>& modules, IPOPassContext& context) override;

	private:
		/**
		 * @brief Collect all functions across modules
		 *
		 * @param modules The modules to scan
		 * @param functions Output vector of functions
		 */
		static void collect_functions(std::vector<Module*>& modules, std::vector<Node *> &functions);

		/**
		 * @brief Collect all global function pointers across modules
		 *
		 * @param modules The modules to scan
		 * @param global_funcs Output set of global function pointers
		 */
		static void collect_global_functions(std::vector<Module*>& modules, std::unordered_set<Node *> &global_funcs);

		/**
		 * @brief Analyze a function for call sites
		 *
		 * @param func The function to analyze
		 * @param graph The call graph being built
		 * @param global_funcs Set of global function pointers
		 * @param modules The modules being analyzed
		 */
		void analyze_function(Node *func, CallGraph &graph, const std::unordered_set<Node *> &global_funcs, std::vector<Module*>& modules);

		/**
		 * @brief Analyze a region for call sites
		 *
		 * @param region The region to analyze
		 * @param caller The calling function
		 * @param graph The call graph being built
		 * @param global_funcs Set of global function pointers
		 */
		void analyze_region(const Region *region, Node *caller, CallGraph &graph,
		                    const std::unordered_set<Node *> &global_funcs);

		/**
		 * @brief Handle a call node
		 *
		 * @param call_node The call node
		 * @param caller The calling function
		 * @param graph The call graph being built
		 * @param global_funcs Set of global function pointers
		 */
		static void handle_call(Node *call_node, Node *caller, CallGraph &graph,
		                 const std::unordered_set<Node *> &global_funcs);

		/**
		 * @brief Check if a call is direct
		 *
		 * @param call_node The call node
		 * @return True if direct call
		 */
		[[nodiscard]] static bool is_direct_call(Node *call_node) ;

		/**
		 * @brief Get the target of a direct call
		 *
		 * @param call_node The call node
		 * @return The target function, or nullptr if not direct
		 */
		[[nodiscard]] static Node *get_direct_call_target(Node *call_node) ;
	};
}
