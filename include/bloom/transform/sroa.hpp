/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	/**
	 * @brief Scalar Replacement of Aggregates pass
	 *
	 * Replaces struct allocations with individual scalar allocations when
	 * the struct is only accessed through field operations and does not escape.
	 * This eliminates unnecessary memory traffic and enables further optimizations.
	 */
	class SROAPass final : public TransformPass
	{
	public:
		/**
		 * @brief Get the name of this pass
		 */
		[[nodiscard]] std::string_view name() const override;

		/**
		 * @brief Get the description of this pass
		 */
		[[nodiscard]] std::string_view description() const override;

		/**
		 * @brief Get the passes required before this one
		 */
		[[nodiscard]] std::vector<const std::type_info *> required_passes() const override;

		/**
		 * @brief Run the SROA pass on the given module
		 */
		bool run(Module &module, PassContext &context) override;

	private:
		/**
		 * @brief Information about a struct allocation candidate
		 */
		struct AllocationInfo
		{
			Node *alloc_node;							/* the original STACK_ALLOC */
			DataType struct_type;						/* the struct type being allocated */
			std::vector<Node *> scalar_allocs;			/* replacement scalar allocations */
			std::vector<std::pair<std::string, DataType>> fields; /* struct field info */
			std::unordered_set<std::size_t> escaped_fields; /* field indices that escaped */
			bool fully_promotable;						/* true if all fields can be promoted */
		};

		/**
		 * @brief Information about a field access
		 */
		struct FieldAccess
		{
			Node *access_node;		/* the LOAD/STORE using this field */
			std::size_t field_index; /* which field (0-based) */
			bool is_store;			/* true for stores, false for loads */
		};

		std::unordered_map<Node *, AllocationInfo> candidates;
		std::unordered_map<Node *, std::vector<FieldAccess>> field_accesses;

		/**
		 * @brief Find all struct allocation candidates in the module
		 */
		void find_candidates(Module &module, const LocalAliasResult &alias_result);

		/**
		 * @brief Find candidates in a specific region
		 */
		void find_candidates_in_region(const Region *region, const LocalAliasResult &alias_result);

		/**
		 * @brief Analyze uses of a struct allocation to determine promotability
		 */
		bool analyze_struct_uses(Node *alloc, AllocationInfo &info, const LocalAliasResult &alias_result);

		/**
		 * @brief Analyze uses within a specific region
		 */
		bool analyze_uses_in_region(const Region *region, Node *alloc, AllocationInfo &info,
		                           const LocalAliasResult &alias_result);

		/**
		 * @brief Check if a node represents a field access pattern
		 * @param node The node to check
		 * @param alloc The struct allocation being accessed
		 * @param field_index Output parameter for the field index
		 * @return true if this is a valid field access
		 */
		bool is_field_access(Node *node, Node *alloc, std::size_t &field_index);

		/**
		 * @brief Get the field index from a PTR_ADD offset
		 */
		static std::size_t get_field_index_from_offset(std::int64_t offset, const std::vector<std::pair<std::string, DataType>> &fields);

		/**
		 * @brief Transform a promotable struct allocation
		 */
		bool transform_allocation(AllocationInfo &info, Module &module);

		/**
		 * @brief Create scalar allocations for struct fields
		 */
		static void create_scalar_allocations(AllocationInfo &info, Module &module);

		/**
		 * @brief Replace field accesses with scalar accesses
		 */
		void replace_field_accesses(AllocationInfo &info);

		/**
		 * @brief Handle partial SROA by creating a reduced struct type
		 */
		static DataType create_reduced_struct_type(const AllocationInfo &info, Module &module);

		/**
		 * @brief Get the size of a data type in bytes
		 */
		static std::uint64_t get_type_size(DataType type);

		/**
		 * @brief Calculate field offsets for a struct type
		 */
		static std::vector<std::uint64_t> calculate_field_offsets(const std::vector<std::pair<std::string, DataType>> &fields);
	};
}
