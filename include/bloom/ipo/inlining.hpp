/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <vector>
#include <bloom/ipo/pass.hpp>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/ipo/specializer.hpp>

namespace blm
{
	/**
	 * @brief IPO pass that performs function inlining and specialization
	 */
	class IPOInliningPass : public IPOPass
	{
	public:
		[[nodiscard]] std::string_view name() const override
		{
			return "ipo-inlining";
		}

		[[nodiscard]] std::string_view description() const override
		{
			return "inlines small functions and specializes functions with constant arguments";
		}

		[[nodiscard]] const std::type_info& blm_id() const override
		{
			return typeid(*this);
		}

		[[nodiscard]] std::vector<const std::type_info*> required_passes() const override
		{
			return get_pass_types<CallGraphAnalysisPass>();
		}

		bool run(std::vector<Module*>& modules, IPOPassContext& context) override;

	private:
		/**
		 * @brief Information about a function call that could be optimized
		 */
		struct InlineCandidate
		{
			Node* call_site = nullptr;
			Node* callee_function = nullptr;
			Module* caller_module = nullptr;
			Module* callee_module = nullptr;
			std::size_t function_size = 0;
			std::size_t benefit_score = 0;
			bool has_constant_args = false;
		};

		/**
		 * @brief Find all potential candidates for inlining or specialization
		 */
		static std::vector<InlineCandidate> find_candidates(const CallGraph& call_graph,
		                                            std::vector<Module*>& modules);

		/**
		 * @brief Determine if a candidate should be optimized
		 */
		[[nodiscard]] bool should_optimize(const InlineCandidate& candidate) const;

		/**
		 * @brief Check if call would create recursion
		 */
		[[nodiscard]] static bool is_recursive_call(const InlineCandidate& candidate);

		/**
		 * @brief Try to specialize function with constant arguments
		 * @return pointer to specialized function if successful, nullptr otherwise
		 */
		Node* try_specialize(const InlineCandidate& candidate);

		/**
		 * @brief Inline a small function by copying its body
		 * @return true if inlining succeeded
		 */
		bool try_inline(const InlineCandidate& candidate);

		/**
		 * @brief Extract return value from inlined function body
		 * @return pointer to the return value node, or nullptr if no return
		 */
		static Node* extract_return_value(Region* inlined_region);

		/**
		 * @brief Calculate benefit score for optimization
		 */
		static std::size_t calculate_benefit(const InlineCandidate& candidate);

		/**
		 * @brief Estimate function size for cost analysis
		 */
		static std::size_t estimate_function_size(Node* function, std::vector<Module*>& modules);

		/**
		 * @brief Find which module contains a function
		 */
		static Module* find_module_for_function(Node* function, std::vector<Module*>& modules);

		/**
		 * @brief Check if call has constant arguments
		 */
		static bool has_constant_arguments(Node* call_site);

		/**
		 * @brief Find the region for a function
		 */
		static Region* find_function_region(Node* function, std::vector<Module*>& modules);

		/**
		 * @brief Clone function body into caller's module
		 */
		static Region* clone_function_body(Node* function, Module& target_module,
									std::vector<Module*>& all_modules,
		                            std::unordered_map<Node*, Node*>& node_mapping);

		/**
		 * @brief Clone a single node
		 */
		static Node* clone_node(Node* original, Module& target_module);

		/**
		 * @brief Fix up connections after cloning
		 */
		static void fixup_connections(const Region* original, Region* cloned,
		                             const std::unordered_map<Node*, Node*>& mapping);

		/**
		 * @brief Replace parameters with call arguments
		 */
		static void substitute_parameters(Region* inlined_region, Node* call_site);

		/**
		 * @brief Replace call with inlined function body
		 * @param call_site The call node to replace
		 * @param inlined_region The inlined function body
		 * @param return_value The return value from the inlined function
		 */
		static void replace_call_with_body(Node* call_site, Region* inlined_region, Node* return_value);

		std::size_t max_inline_size = 15;        /* keep it small for real inlining */
		std::size_t min_benefit_threshold = 3;
		bool enable_specialization = true;

		FunctionSpecializer specializer;
	};
}
