/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	class Context;
	class Module;
	struct Node;
	class PassContext;
	class Region;

	/**
	 * @brief Represents a dependency graph for analyzing node relationships
	 */
	struct DependencyGraph
	{
		std::unordered_set<Node*> nodes;
		std::unordered_map<Node*, std::unordered_set<Node*>> edges;         /* node -> dependents */
		std::unordered_map<Node*, std::unordered_set<Node*>> reverse_edges; /* node -> dependencies */
	};

	/**
	 * @brief Represents a candidate for vectorization
	 */
	struct VectorCandidate
	{
		NodeType operation = NodeType::ADD;
		DataType element_type = DataType::VOID;
		std::uint32_t vector_width = 0;
		std::vector<Node*> scalar_ops;

		/**
		 * @brief Check if this candidate is valid for vectorization
		 * @return true if the candidate has at least 2 operations of the same type
		 */
		bool is_valid() const
		{
			return scalar_ops.size() >= 2 && vector_width >= 2;
		}
	};

	/**
	 * @brief Superword Level Parallelism (SLP) vectorization pass
	 *
	 * This pass identifies opportunities to combine multiple scalar operations
	 * into vector operations, enabling SIMD execution. It performs sophisticated
	 * dependency analysis to determine which operations can be safely vectorized
	 * together without violating program semantics.
	 */
	class SLPPass : public TransformPass
	{
	public:
		/**
		 * @brief Get the name of this pass
		 * @return Pass name for identification and logging
		 */
		std::string_view name() const override;

		/**
		 * @brief Get a description of what this pass does
		 * @return Human-readable description of the pass functionality
		 */
		std::string_view description() const override;

		/**
		 * @brief Get the list of analysis passes this pass requires
		 * @return Vector of type_info pointers for required passes
		 */
		std::vector<const std::type_info*> required_passes() const override;

		/**
		 * @brief Execute the SLP vectorization pass
		 * @param module The module to transform
		 * @param context Pass execution context for accessing analysis results
		 * @return true if any vectorizations were performed, false otherwise
		 */
		bool run(Module& module, PassContext& context) override;

	private:
		std::unordered_set<Node*> processed_nodes;
		std::vector<VectorCandidate> candidates;

		/**
		 * @brief Process a region to find vectorization candidates
		 * @param region The region to analyze
		 * @param alias_result Alias analysis results for memory safety
		 * @param ctx Context for creating new nodes
		 */
		void process_region(Region* region, const LocalAliasResult& alias_result, Context& ctx);

		/**
		 * @brief Build a dependency graph for the given region
		 * @param region Region to analyze for dependencies
		 * @return Dependency graph showing node relationships
		 */
		static DependencyGraph build_dependency_graph(Region* region);

		/**
		 * @brief Check if a set of operations would create a dependency cycle
		 * @param ops Operations to check for cycles
		 * @param graph Dependency graph for the region
		 * @return true if vectorizing these operations would create a cycle
		 */
		static bool has_dependency_cycle(const std::vector<Node*>& ops, const DependencyGraph& graph);

		/**
		 * @brief Find potential vectorization candidates in a region
		 * @param region Region to search for candidates
		 * @return Vector of potential vectorization candidates
		 */
		std::vector<VectorCandidate> find_vectorization_candidates(Region* region);

		/**
		 * @brief Try to build a basic vectorization candidate from a starting node
		 * @param start_node Node to start building candidate from
		 * @param region Region containing the operations
		 * @return Vectorization candidate (may be invalid if no suitable operations found)
		 */
		VectorCandidate try_build_candidate(Node* start_node, Region* region) const;

		/**
		 * @brief Try to build a candidate based on isomorphic operation patterns
		 * @param start_node Node to start building candidate from
		 * @param region Region containing the operations
		 * @return Vectorization candidate based on similar operation structures
		 */
		VectorCandidate try_build_isomorphic_candidate(Node* start_node, Region* region) const;

		/**
		 * @brief Try to build a candidate from a chain of dependent operations
		 * @param start_node Node to start building candidate from
		 * @param region Region containing the operations
		 * @return Vectorization candidate from operation chain
		 */
		static VectorCandidate try_build_chain_candidate(Node* start_node, Region* region);

		/**
		 * @brief Find a chain of operations that can be vectorized together
		 * @param start Starting node for the chain
		 * @return Vector of nodes forming a vectorizable chain
		 */
		static std::vector<Node*> find_operation_chain(Node *start);

		/**
		 * @brief Check if two operations have similar structure (isomorphic)
		 * @param a First operation to compare
		 * @param b Second operation to compare
		 * @return true if operations have compatible input patterns
		 */
		static bool are_operations_isomorphic(Node* a, Node* b);

		/**
		 * @brief Check if an operation type can be vectorized
		 * @param op_type The operation type to check
		 * @return true if the operation supports vectorization
		 */
		static bool can_vectorize_operation(NodeType op_type) ;

		/**
		 * @brief Check if a set of operations can be safely vectorized together
		 * @param ops Operations to check for vectorization compatibility
		 * @param alias_result Alias analysis for memory safety checking
		 * @param dep_graph Dependency graph for the region
		 * @return true if operations can be safely vectorized together
		 */
		static bool can_vectorize_together(const std::vector<Node*>& ops,
		                           const LocalAliasResult& alias_result,
		                           const DependencyGraph& dep_graph);

		/**
		 * @brief Check memory safety for memory operations
		 * @param ops Operations to check (may include memory operations)
		 * @param alias_result Alias analysis results
		 * @return true if memory operations don't have unsafe aliasing
		 */
		static bool check_memory_safety(const std::vector<Node*>& ops, const LocalAliasResult& alias_result);

		/**
		 * @brief Check if operations can be safely reordered
		 * @param ops Operations to check for reordering safety
		 * @param dep_graph Dependency graph for analysis
		 * @return true if operations can be reordered without violating dependencies
		 */
		static bool can_reorder_operations(const std::vector<Node*>& ops, const DependencyGraph& dep_graph);

		/**
		 * @brief Check if a shared dependency between operations is safe
		 * @param producer Operation that produces the dependency
		 * @param ops All operations in the vectorization group
		 * @return true if the shared dependency doesn't prevent vectorization
		 */
		static bool is_safe_shared_dependency(Node *producer, const std::vector<Node *> &ops);

		/**
		 * @brief Perform vectorization on a candidate
		 * @param candidate The vectorization candidate to process
		 * @param region Region containing the operations
		 * @param ctx Context for creating new nodes
		 */
		static void vectorize_candidate(const VectorCandidate& candidate, Region* region, Context& ctx);

		/**
		 * @brief Create a vector operation from scalar operations
		 * @param candidate Vectorization candidate containing scalar operations
		 * @param region Region to insert the vector operation into
		 * @param ctx Context for creating nodes
		 * @return Pointer to the created vector operation node
		 */
		static Node* create_vector_operation(const VectorCandidate& candidate, Region* region, Context& ctx);

		/**
		 * @brief Gather operands for a vector operation
		 * @param scalar_ops Scalar operations to gather operands from
		 * @param operand_index Which operand position to gather
		 * @param region Region to insert gather operations into
		 * @param ctx Context for creating nodes
		 * @return Pointer to the vector operand (either vector_build or broadcast)
		 */
		static Node* gather_operands(const std::vector<Node*>& scalar_ops, std::size_t operand_index,
		                     Region* region, Context& ctx);

		/**
		 * @brief Extract scalar results from vector operation
		 * @param vector_result The vector operation result
		 * @param scalar_ops Original scalar operations to replace
		 * @param region Region to insert extract operations into
		 * @param ctx Context for creating nodes
		 */
		static void scatter_results(Node* vector_result, const std::vector<Node*>& scalar_ops,
		                    Region* region, Context& ctx);

		/**
		 * @brief Replace all uses of an old node with a new node
		 * @param old_node Node to replace
		 * @param new_node Replacement node
		 */
		static void replace_node_uses(Node* old_node, Node* new_node);

		/**
		 * @brief Check if a data type is compatible with SIMD operations
		 * @param type The data type to check
		 * @return true if the type can be used in vector operations
		 */
		static bool is_simd_compatible_type(DataType type) ;

		/**
		 * @brief Get the maximum vector width for a given element type
		 * @param element_type The element type for the vector
		 * @return Maximum number of elements for this type
		 */
		static std::uint32_t get_max_vector_width(DataType element_type);

		/**
		 * @brief Remove scalar operations that have been vectorized
		 * @param ops Operations to remove
		 * @param region Region containing the operations
		 */
		static void remove_scalar_operations(const std::vector<Node*>& ops, Region* region);

		/**
		 * @brief Find the region corresponding to a function
		 * @param function Function node to find region for
		 * @param module Module containing the function
		 * @return Pointer to function region, or nullptr if not found
		 */
		Region* find_function_region(Node* function, Module& module) const;

		/**
		 * @brief Find a region by name within a region hierarchy
		 * @param region Root region to search from
		 * @param name Name of region to find
		 * @return Pointer to found region, or nullptr if not found
		 */
		Region* find_region_by_name(const Region* region, std::string_view name) const;
	};
}
