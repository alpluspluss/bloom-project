/* this project is part of the bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <set>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	class DCEPass final : public TransformPass
	{
	public:
		[[nodiscard]] std::string_view name() const override;

		[[nodiscard]] std::string_view description() const override;

		bool run(Module &m, PassContext &ctx) override;

	private:
		std::set<Node*> dead;
		std::set<Node*> alive;

		void find_live_nodes(const Region* region);

		void find_dead_nodes(const Region* region);

		std::size_t remove_dead_nodes(Region* region);

		static bool is_root_node(const Node* node);
	};
}
