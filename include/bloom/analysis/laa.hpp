/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <bloom/foundation/analysis-pass.hpp>

namespace blm
{
	/**
	 * @brief Result of alias analysis between two pointers
	 */
	enum class AliasResult
	{
		/** @brief Pointers never alias */
		NO_ALIAS,
		/** @brief Pointers might alias */
		MAY_ALIAS,
		/** @brief Pointers definitely alias */
		MUST_ALIAS,
		/** @brief Pointers partially overlap */
		PARTIAL_ALIAS
	};

	/**
	 * @brief Represents a memory location with a base and offset
	 */
	struct MemoryLocation
	{
		Node *base = nullptr; /* base pointer or allocation site */
		int64_t offset = -1;  /* offset from base if known or -1 */
		uint64_t size = 0;    /* size in bytes if known or 0 */

		bool operator==(const MemoryLocation &) const = default;
	};

	/**
	 * @brief Hash function for memory location
	 */
	struct MemoryLocationHash
	{
		std::size_t operator()(const MemoryLocation &loc) const
		{
			std::size_t h = std::hash<Node *> {}(loc.base);
			h = h * 31 + std::hash<int64_t> {}(loc.offset);
			h = h * 31 + std::hash<uint64_t> {}(loc.size);
			return h;
		}
	};

	/**
	 * @brief Result class for local alias analysis
	 */
	class LocalAliasResult final : public AnalysisResult
	{
	public:
		/**
		 * @brief Check whether a transform pass invalidates this analysis
		 */
		[[nodiscard]] bool invalidated_by(const std::type_info &transform_type) const override;

		/**
		 * @brief Add a memory location for a pointer
		 */
		void add_location(Node *ptr, MemoryLocation loc);

		/**
		 * @brief Get the memory location for a pointer
		 */
		[[nodiscard]] const MemoryLocation *get_location(Node *ptr) const;

		/**
		 * @brief Determine the alias relationship between two pointers
		 */
		[[nodiscard]] AliasResult alias(Node *a, Node *b) const;

		/**
		 * @brief Get whether two pointers may alias
		 */
		[[nodiscard]] bool may_alias(Node *a, Node *b) const
		{
			const auto result = alias(a, b);
			return result == AliasResult::MAY_ALIAS ||
			       result == AliasResult::MUST_ALIAS ||
			       result == AliasResult::PARTIAL_ALIAS;
		}

		/**
		 * @brief Get whether two pointers must alias
		 */
		[[nodiscard]] bool must_alias(Node *a, Node *b) const
		{
			return alias(a, b) == AliasResult::MUST_ALIAS;
		}

		/**
		 * @brief Add an allocation site to track
		 */
		void add_allocation_site(Node *node, uint64_t size = 0);

		/**
		 * @brief Check if a node is an allocation site
		 */
		[[nodiscard]] bool is_allocation_site(Node *node) const;

		/**
		 * @brief Mark a pointer as having escaped the current scope
		 */
		void mark_escaped(Node *ptr);

		/**
		 * @brief Check if a pointer has escaped
		 */
		[[nodiscard]] bool has_escaped(Node *ptr) const;

		/**
		 * @brief Add a pointer copy relationship
		 */
		void add_pointer_copy(Node *dest, Node *src);

		/**
		 * @brief Get the ultimate source of a pointer through copy chains
		 */
		[[nodiscard]] Node *get_pointer_source(Node *ptr) const;

		/**
		 * @brief Adds a store operation to the analysis.
		 * @param store The node representing the store operation.
		 */
		void add_store_operation(Node* store);

		/**
		 * @brief Adds a load operation to the analysis.
		 * @param load The node representing the load operation.
		 */
		void add_load_operation(Node* load);

		/**
		 * @brief Establishes a relationship between a store and a load operation.
		 * @param store The node representing the store operation.
		 * @param load The node representing the load operation.
		 */
		void add_store_load_relation(Node* store, Node* load);

		/**
		 * @brief Retrieves the list of store operations that may affect a given load operation.
		 * @param load The node representing the load operation.
		 * @return A vector of nodes representing the affecting store operations.
		 */
		std::vector<Node*> get_affecting_stores(Node* load) const;

		/**
		 * @brief Retrieves the list of load operations that may be affected by a given store operation.
		 * @param store The node representing the store operation.
		 * @return A vector of nodes representing the affected load operations.
		 */
		std::vector<Node*> get_affected_loads(Node* store) const;

		/**
		 * @brief Determines if a load operation may be modified by a store operation.
		 * @param load The node representing the load operation.
		 * @param store The node representing the store operation.
		 * @return True if the load may be modified by the store, false otherwise.
		 */
		bool maybe_modified_by(Node* load, Node* store) const;

		std::unordered_set<Node*> get_all_loads()
		{
			return all_loads;
		}

		std::unordered_set<Node*> get_all_stores()
		{
			return all_stores;
		}
	private:
		std::unordered_map<Node *, MemoryLocation> memory_locations;
		std::unordered_set<Node *> allocation_sites;
		std::unordered_set<Node *> escaped_pointers;
		std::unordered_map<Node *, Node *> pointer_copies;
		std::unordered_map<Node*, std::set<Node*>> store_to_loads;
		std::unordered_map<Node*, std::set<Node*>> load_to_stores;
		std::unordered_set<Node*> all_stores;
		std::unordered_set<Node*> all_loads;
	};

	/**
	 * @brief Performs local alias analysis on the ir
	 */
	class LocalAliasAnalysisPass final : public AnalysisPass
	{
	public:
		[[nodiscard]] std::string_view name() const override
		{
			return "local-alias-analysis";
		}

		[[nodiscard]] std::string_view description() const override
		{
			return "analyzes pointer relationships and escape behavior within function boundaries";
		}

		[[nodiscard]] std::vector<const std::type_info *> required_passes() const override
		{
			return {};
		}

		/**
		 * @brief perform local alias analysis on a module
		 */
		std::unique_ptr<AnalysisResult> analyze(Module &module, PassContext &context) override;

	private:
		void handle_store(LocalAliasResult& result, Node* node);

		void analyze_store_load_relations(LocalAliasResult& result);

		bool addresses_may_alias(LocalAliasResult& result, Node* addr1, Node* addr2) const;

		void analyze_function(LocalAliasResult &result, Node *func, Module &module);

		void analyze_region(LocalAliasResult &result, const Region *region);

		void analyze_node(LocalAliasResult &result, Node *node);

		void handle_allocation(LocalAliasResult &result, Node *node) const;

		void handle_pointer_arithmetic(LocalAliasResult &result, Node *node);

		void handle_address_of(LocalAliasResult &result, Node *node) const;

		void handle_parameter(LocalAliasResult &result, Node *node) const;

		void handle_load(LocalAliasResult &result, Node *node) const;

		void handle_function_call(LocalAliasResult &result, const Node *node) const;

		void handle_return(LocalAliasResult &result, const Node *node) const;

		void handle_cast(LocalAliasResult &result, Node *node) const;

		void perform_escape_analysis(LocalAliasResult &result, Module &module);

		bool propagate_escapes_in_region(LocalAliasResult &result, const Region *region);

		std::uint64_t extract_integer_literal(Node *node) const;

		std::int64_t compute_pointer_offset(Node *node, Node *&base_ptr);

		std::uint64_t get_access_size(Node *node) const;

		[[nodiscard]] std::uint64_t get_type_size(DataType type) const;
	};
}
