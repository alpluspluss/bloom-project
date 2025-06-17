/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/foundation/module.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	/**
	 * @brief Constant folding optimization pass
	 *
	 * Evaluates constant expressions at compile time and replaces them with
	 * their computed values.
	 */
	class ConstantFoldingPass final : public TransformPass
	{
	public:
		[[nodiscard]] std::string_view name() const override;

		[[nodiscard]] std::string_view description() const override;

		[[nodiscard]] std::vector<const std::type_info *> required_passes() const override
		{
			return {};
		}

		bool run(Module &module, PassContext &context) override;

	private:
		Module *current_module = nullptr;

		std::size_t process_region(Region *region);

		Node *create_copy_propagation_node(const Node *node) const;

		[[nodiscard]] bool is_constant(const Node *node) const;

		[[nodiscard]] Node *create_folded_node(const Node *node) const;

		[[nodiscard]] Node *fold_arithmetic(const Node *node, const Node *lhs, const Node *rhs) const;

		[[nodiscard]] Node *fold_comparison(const Node *node, const Node *lhs, const Node *rhs) const;

		[[nodiscard]] Node *fold_bitwise(const Node *node, const Node *lhs, const Node *rhs) const;

		bool has_global_inputs(const Node *node) const;
	};
}