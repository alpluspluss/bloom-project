/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	using ValueNumber = std::uint64_t;

	/**
	 * @brief Common Subexpression Elimination Pass using Value Numbering
	 */
	class CSEPass final : public TransformPass
	{
	public:
		[[nodiscard]] std::string_view name() const override;

		[[nodiscard]] std::string_view description() const override;

		[[nodiscard]] std::vector<const std::type_info *> required_passes() const override;

		bool run(Module &m, PassContext &ctx) override;

	private:
		std::unordered_map<Node *, ValueNumber> value_numbers;
		std::unordered_map<ValueNumber, Node *> expression_to_node;
		ValueNumber next_value_number = 1;

		std::size_t process_function(Module &module, const LocalAliasResult &alias_result);

		std::size_t process_region(const Region *region, const LocalAliasResult &alias_result);

		ValueNumber compute_value_number(Node *node, const LocalAliasResult &alias_result);

		static ValueNumber compute_literal_value_number(Node *node);

		ValueNumber compute_expression_value_number(Node *node);

		ValueNumber compute_load_value_number(Node *node, const LocalAliasResult &alias_result);

		[[nodiscard]] static bool is_load_operation(const Node *node);

		[[nodiscard]] static bool has_inputs(const Node *node);

		[[nodiscard]] bool are_equivalent_expressions(Node *a, Node *b);

		[[nodiscard]] static bool is_commutative(NodeType type);

		[[nodiscard]] static bool is_eligible_for_cse(const Node *node);

		[[nodiscard]] bool loads_may_alias(Node *a, Node *b, const LocalAliasResult &alias_result);

		static bool replace_all_uses(Node *node_to_replace, Node *replacement_node);
	};
}
