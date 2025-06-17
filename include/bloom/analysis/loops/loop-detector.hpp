/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>

namespace blm
{
	/**
	 * @brief Represents a single loop in the loop tree (private implementation)
	 */
	class Loop
	{
	public:
		/**
		 * @brief The header region of the loop (where condition is checked)
		 */
		Region *header = nullptr;

		/**
		 * @brief All regions that belong to this loop (excluding header)
		 */
		std::unordered_set<Region *> body_regions;

		/**
		 * @brief Exit regions (where control leaves the loop)
		 */
		std::vector<Region *> exits;

		/**
		 * @brief Back-edge sources (regions that jump back to header)
		 */
		std::vector<Region *> latches;

		/**
		 * @brief Parent loop (nullptr if outermost)
		 */
		Loop *parent = nullptr;

		/**
		 * @brief Direct child loops (nested inside this loop)
		 */
		std::vector<Loop *> children;

		/**
		 * @brief Loop nesting depth (0 = outermost)
		 */
		std::size_t depth = 0;

		/**
		 * @brief Check if this loop contains a region
		 */
		[[nodiscard]] bool contains(Region *region) const
		{
			return region == header || body_regions.contains(region);
		}

		/**
		 * @brief Check if this is a natural loop (single header, single latch)
		 */
		[[nodiscard]] bool is_natural() const
		{
			return latches.size() == 1;
		}

		/**
		 * @brief Get all regions in loop (header + body)
		 */
		[[nodiscard]] std::vector<Region *> get_all_regions() const
		{
			std::vector<Region *> all;
			all.push_back(header);
			for (Region *region : body_regions)
				all.push_back(region);
			return all;
		}
	};

	/**
	 * @brief Represents the complete loop tree for a function (private implementation)
	 */
	class LoopTree
	{
	public:
		/**
		 * @brief Root loops (not nested in any other loop)
		 */
		std::vector<Loop *> root_loops;

		/**
		 * @brief All loops in the tree
		 */
		std::vector<std::unique_ptr<Loop>> all_loops;

		/**
		 * @brief Map from region to innermost containing loop
		 */
		std::unordered_map<Region *, Loop *> region_to_loop;

		/**
		 * @brief Maximum nesting depth
		 */
		std::size_t max_depth = 0;

		/**
		 * @brief Get the innermost loop containing a region
		 */
		[[nodiscard]] Loop *get_loop_for(Region *region) const
		{
			const auto it = region_to_loop.find(region);
			return it != region_to_loop.end() ? it->second : nullptr;
		}

		/**
		 * @brief Visit loops in post-order (children before parent)
		 */
		template<typename Func>
		void visit_post_order(Func &&func)
		{
			for (Loop *root : root_loops)
				visit_post_order_impl(root, func);
		}

		/**
		 * @brief Visit loops in pre-order (parent before children)
		 */
		template<typename Func>
		void visit_pre_order(Func &&func)
		{
			for (Loop *root : root_loops)
				visit_pre_order_impl(root, func);
		}

	private:
		template<typename Func>
		void visit_post_order_impl(Loop *loop, Func &&func)
		{
			for (Loop *child : loop->children)
				visit_post_order_impl(child, func);
			func(loop);
		}

		template<typename Func>
		void visit_pre_order_impl(Loop *loop, Func &&func)
		{
			func(loop);
			for (Loop *child : loop->children)
				visit_pre_order_impl(child, func);
		}
	};

	/**
	 * @brief Detects loops and builds loop tree from region hierarchy (private implementation)
	 */
	class LoopDetector
	{
	public:
		/**
		 * @brief Analyze a function and build its loop tree
		 * @param function_region The root region of the function
		 * @return LoopTree containing all detected loops
		 */
		static LoopTree analyze_function(Region *function_region);

	private:
		/**
		 * @brief Represents a back-edge in the control flow graph
		 */
		struct BackEdge
		{
			Region *source; /* region containing the jump/branch */
			Region *target; /* header region being jumped to */
		};

		/**
		 * @brief Find all back-edges in the function
		 * @param root The root region to analyze
		 * @return Vector of back-edges
		 */
		static std::vector<BackEdge> find_back_edges(Region *root);

		/**
		 * @brief Build a natural loop from a back-edge
		 * @param back_edge The back-edge defining the loop
		 * @return Unique pointer to the created loop
		 */
		static std::unique_ptr<Loop> build_natural_loop(const BackEdge &back_edge);

		/**
		 * @brief Get all target regions from a control flow node
		 * @param node The control flow node (JUMP, BRANCH, INVOKE)
		 * @return Vector of target regions
		 */
		static std::vector<Region *> get_control_targets(const Node *node);

		/**
		 * @brief Visit all regions in the hierarchy
		 * @param root The root region
		 * @param visitor Function to call for each region
		 */
		template<typename Func>
		static void visit_regions(Region *root, Func &&visitor)
		{
			if (!root)
				return;

			visitor(root);
			for (Region *child : root->get_children())
				visit_regions(child, visitor);
		}

		/**
		 * @brief Find all regions that can reach the latch without going through header
		 * @param header The loop header
		 * @param latch The back-edge source
		 * @return Set of regions in the loop body
		 */
		static std::unordered_set<Region *> find_loop_body(Region *header, Region *latch);

		/**
		 * @brief Build the loop tree hierarchy from individual loops
		 * @param loops Vector of all detected loops
		 * @return Constructed loop tree
		 */
		static LoopTree build_loop_tree(std::vector<std::unique_ptr<Loop>> loops);

		/**
		 * @brief Determine parent-child relationships between loops
		 * @param loops Vector of all loops
		 */
		static void establish_loop_hierarchy(std::vector<std::unique_ptr<Loop>> &loops);
	};
}
