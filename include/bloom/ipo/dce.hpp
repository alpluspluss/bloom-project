/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_set>
#include <vector>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/ipo/pass-context.hpp>
#include <bloom/ipo/pass.hpp>

namespace blm
{
	/**
	 * @brief IPO pass that removes unreachable functions across modules
	 */
	class IPODCEPass : public IPOPass
	{
	public:
		/**
		 * @brief Get the name of this pass
		 */
		[[nodiscard]] std::string_view name() const override
		{
			return "ipo-dead-code-elimination";
		}

		/**
		 * @brief Get the description of this pass
		 */
		[[nodiscard]] std::string_view description() const override
		{
			return "removes functions that are unreachable from any entry point";
		}

		/**
		 * @brief Get the type information for this pass
		 */
		[[nodiscard]] const std::type_info& blm_id() const override
		{
			return typeid(*this);
		}

		/**
		 * @brief Get the IPO analysis passes this pass requires
		 */
		[[nodiscard]] std::vector<const std::type_info*> required_passes() const override
		{
			return get_pass_types<CallGraphAnalysisPass>();
		}

		/**
		 * @brief Execute the IPO dead code elimination
		 *
		 * @param modules Vector of modules to process
		 * @param context The IPO pass context
		 * @return True if any functions were removed
		 */
		bool run(std::vector<Module*>& modules, IPOPassContext& context) override;

	private:
		/**
		 * @brief Mark functions that serve as entry points to the program
		 *
		 * @param modules The modules to scan for entry points
		 * @param reachable Set to store reachable functions
		 */
		static void mark_entry_points(std::vector<Module*>& modules, std::unordered_set<Node*>& reachable);

		/**
		 * @brief Propagate reachability through the call graph
		 *
		 * @param call_graph The call graph to traverse
		 * @param reachable Set of reachable functions (updated in place)
		 */
		static void propagate_reachability(const CallGraph& call_graph, std::unordered_set<Node*>& reachable);

		/**
		 * @brief Remove unreachable functions from a module
		 *
		 * @param module The module to clean up
		 * @param reachable Set of functions that should be kept
		 * @return Number of functions removed
		 */
		static std::size_t remove_unreachable_functions(Module* module, const std::unordered_set<Node*>& reachable);

		/**
		 * @brief Check if a function should be considered an entry point
		 *
		 * @param function The function to check
		 * @return True if the function is an entry point
		 */
		[[nodiscard]] static bool is_entry_point(Node* function);

		/**
		 * @brief Remove a function and its associated region from a module
		 *
		 * @param module The module containing the function
		 * @param function The function to remove
		 */
		static void remove_function_and_region(Module* module, Node* function);

		/**
		 * @brief Find the region associated with a function
		 *
		 * @param module The module to search in
		 * @param function The function to find the region for
		 * @return Pointer to the function's region, or nullptr if not found
		 */
		[[nodiscard]] static Region* find_function_region(Module* module, Node* function) ;
	};
}
