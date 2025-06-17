/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <vector>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	/**
	 * @brief Result of a PRE optimization
	 */
	struct PREResult
	{
		/** @brief The original expression that was hoisted */
		Node *original_node = nullptr;

		/** @brief The hoisted computation */
		Node *hoisted_node = nullptr;

		/** @brief The region where the computation was hoisted to */
		Region *target_region = nullptr;

		/** @brief Number of redundant instances that were removed */
		std::size_t instances_removed = 0;
	};

	using ExprHash = std::uint64_t;

	/**
	 * @brief Partial Redundancy Elimination optimization pass
	 *
	 * This pass identifies expressions that are redundantly computed along some
	 * but not necessarily all execution paths, and hoists them to a common
	 * dominator to eliminate the redundancy.
	 */
	class PREPass final : public TransformPass
	{
	public:
		[[nodiscard]] std::string_view name() const override;

		[[nodiscard]] std::string_view description() const override;

		bool run(Module &m, PassContext &ctx) override;

	private:
		using ExpressionGroups = std::unordered_map<ExprHash, std::vector<Node *> >;

		std::size_t process_region(Region *region);

		ExpressionGroups collect_expressions(Region *region);

		bool try_hoist_expression(const std::vector<Node *> &nodes);

		[[nodiscard]] bool are_all_equivalent(const std::vector<Node *> &nodes) const;

		bool are_expressions_equivalent(Node *a, Node *b) const;

		[[nodiscard]] bool is_commutative_operation(NodeType type) const;

		Region *find_common_dominator(const std::vector<Node *> &nodes);

		Region *find_common_dominator(Region *r1, Region *r2) const;

		bool inputs_available_at(Node *node, Region *target) const;

		bool is_safe_hoist_target(Region *target) const;

		Node *create_hoisted_node(const Node *template_node, Region *target);

		void insert_hoisted_node(Node *hoisted, Region *target);

		std::size_t replace_dominated_occurrences(const std::vector<Node *> &nodes,
		                                          Node *replacement,
		                                          const Region *dominator);

		ExprHash compute_expr_hash(Node *node) const;

		bool is_eligible_for_pre(const Node *node) const;

		bool has_unstructured_control_flow(const Region *region) const;

		std::vector<PREResult> pre_results;
	};
}
