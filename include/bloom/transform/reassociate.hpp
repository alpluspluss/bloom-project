/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/foundation/node.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	class ReassociatePass : public TransformPass
	{
	public:
		[[nodiscard]] std::string_view name() const override;

		[[nodiscard]] std::string_view description() const override;

		[[nodiscard]] std::vector<const std::type_info *> required_passes() const override;

		bool run(Module &m, PassContext &ctx) override;

	private:
		bool process_region(const Region* region);

		bool reassociate(Node* node);

		[[nodiscard]] bool is_reassociable(NodeType type) const;

		void extract_operands(Node* node, NodeType type, std::vector<Node*> &constants,
		   std::vector<Node*> &variables);

		Node* create_balanced_tree(Region* region, NodeType op_type, DataType type_kind,
		   const std::vector<Node*>& operands, Node* insertion_point) const;

		Region* find_containing_region(Node* node);

		Region* find_region_containing_node(Region* region, Node* node);

		void replace_all_uses(Node* old_node, Node* new_node);

		bool is_constant(const Node* node);

		std::size_t reassociated_count = 0;
		Module* current_module = nullptr;
	};
}
