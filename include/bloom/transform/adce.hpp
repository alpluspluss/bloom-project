/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_set>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	/**
	 * @brief Aggressive Dead Code Elimination pass
	 *
	 * More aggressive than regular DCE. Removes unreachable regions
	 * and dead nodes by following control flow paths. Does not perform
	 * simplification which is constfold's job.
	 */
	class ADCEPass final : public TransformPass
	{
	public:
		[[nodiscard]] std::string_view name() const override;

		[[nodiscard]] std::string_view description() const override;

		bool run(Module& m, PassContext& ctx) override;

	private:
		std::unordered_set<Region*> reachable_regions;
		std::unordered_set<Node*> live_nodes;
		std::unordered_set<Region*> dead_regions;

		/**
		 * @brief Mark all reachable regions starting from entry points
		 */
		void mark_reachable_regions(Module& m);

		/**
		 * @brief Mark a region and its control flow targets as reachable
		 */
		void mark_region_reachable(Module& m, const Region* region);

		/**
		 * @brief Mark the region containing a target entry node as reachable
		 */
		void mark_target_region_reachable(Module& m, const Node* target_entry);

		/**
		 * @brief Mark live nodes using worklist algorithm
		 */
		void mark_live_nodes(Module& m);

		/**
		 * @brief Check if a node is critical (must be preserved)
		 */
		static bool is_critical_node(Node* node) ;

		/**
		 * @brief Remove unreachable regions from the module
		 */
		std::size_t remove_unreachable_regions(Module& m);

		/**
		 * @brief Collect all regions in the module recursively
		 */
		void collect_all_regions(const Region* region, std::vector<Region*>& all_regions) const;

		/**
		 * @brief Remove dead nodes from live regions
		 */
		std::size_t remove_dead_nodes(Module& m) const;

		/**
		 * @brief Remove a single dead node from a region
		 */
		static void remove_dead_node(Node* node, Region* region);

		/**
		 * @brief Find the region containing a function's body
		 */
		Region* find_function_region(Node* function, Module& m) const;

		/**
		 * @brief Find a region by name recursively
		 */
		Region* find_region_by_name(const Region* region, std::string_view name) const;
	};
}
