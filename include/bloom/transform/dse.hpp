/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_set>
#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	/**
	 * @brief Dead Store Elimination pass
	 *
	 * Removes stores that are never read before being overwritten.
	 * Uses enhanced alias analysis to determine which stores are redundant.
	 */
	class DSEPass : public TransformPass
	{
	public:
		[[nodiscard]] std::string_view name() const override;

		[[nodiscard]] std::string_view description() const override;

		[[nodiscard]] std::vector<const std::type_info*> required_passes() const override;

		bool run(Module& m, PassContext& ctx) override;

	private:
		std::unordered_set<Node*> dead_stores;

		/**
		 * @brief Process a region to find dead stores
		 */
		std::size_t process_region(const Region* region, const LocalAliasResult& alias_result);

		/**
		 * @brief Check if a node is a store operation
		 */
		static bool is_store_node(Node* node) ;

		/**
		 * @brief Check if a node is a load operation
		 */
		static bool is_load_node(Node* node) ;

		/**
		 * @brief Check if a node is a function call
		 */
		static bool is_call_node(Node* node);

		/**
		 * @brief Get the address being stored to
		 */
		static Node* get_store_address(Node* store);

		/**
		 * @brief Get the address being accessed by a memory operation
		 */
		static Node* get_memory_address(Node* mem_op) ;

		/**
		 * @brief Remove a dead store from the IR
		 */
		static void remove_dead_store(Node* store);

		/**
		 * @brief Check if a partially overlapping store can be eliminated
		 */
		bool can_eliminate_partial_overlap(Node* old_store, Node* new_store,
										   Node* old_addr, Node* new_addr,
										   const LocalAliasResult& alias_result) const;

		/**
		 * @brief Get the data type of the value being stored
		 */
		static DataType get_store_value_type(Node* store) ;
	};
}
