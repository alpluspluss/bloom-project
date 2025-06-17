/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/types.hpp>
#include <bloom/foundation/transform-pass.hpp>

namespace blm
{
	/**
	 * @brief Algebraic simplification pass that applies mathematical identities and strength reduction
	 *
	 * This pass implements a comprehensive set of algebraic optimizations including:
	 * - Identity laws (x + 0 = x, x * 1 = x, etc.)
	 * - Negation sinking (-(-x) = x, x + (-y) = x - y)
	 * - Carry/borrow elimination (x - (x & y) = x & ~y)
	 * - Small constant multiplication decomposition (x * 3 = (x << 1) + x)
	 * - Power-of-2 strength reduction (x * 2^n = x << n)
	 * - Comparison optimizations for unsigned types
	 * - Bitwise pattern recognition and simplification
	 */
	class InstcombinePass final : public TransformPass
	{
	public:
		/**
		 * @brief Get the name of this pass
		 * @return Pass name for logging and debugging
		 */
		[[nodiscard]] std::string_view name() const override;

		/**
		 * @brief Get a description of what this pass does
		 * @return Human-readable description
		 */
		[[nodiscard]] std::string_view description() const override;

		/**
		 * @brief Run the algebraic simplification pass on a module
		 * @param m Module to transform
		 * @param ctx Pass context for statistics and analysis results
		 * @return True if any simplifications were made
		 */
		bool run(Module &m, PassContext &ctx) override;

	private:
		/**
		 * @brief Process a region recursively, simplifying nodes
		 * @param region Region to process
		 * @return Number of simplifications performed
		 */
		std::size_t process_region(Region *region);

		/**
		 * @brief Attempt to simplify a single node
		 * @param node Node to simplify
		 * @param region Region containing the node
		 * @return Simplified node or nullptr if no simplification possible
		 */
		Node *simplify_node(Node *node, Region *region);

		/* pattern matching methods */

		/**
		 * @brief Simplify patterns involving negation
		 * @param node Node to check for negation patterns
		 * @param region Region for creating new nodes
		 * @return Simplified node or nullptr
		 */
		Node *simplify_negation_patterns(Node *node, Region *region);

		/**
		 * @brief Simplify carry and borrow elimination patterns
		 * @param node Node to check for carry/borrow patterns
		 * @param region Region for creating new nodes
		 * @return Simplified node or nullptr
		 */
		static Node *simplify_carry_borrow_patterns(Node *node, Region *region);

		/**
		 * @brief Simplify small constant multiplication using shifts and adds
		 * @param node Multiplication node to check
		 * @param left Left operand
		 * @param right Right operand
		 * @param region Region for creating new nodes
		 * @return Simplified node or nullptr
		 */
		Node *simplify_small_constant_multiplication(Node *node, Node *left, Node *right, Region *region);

		/**
		 * @brief Simplify comparison operations with special patterns
		 * @param node Comparison node to check
		 * @param left Left operand
		 * @param right Right operand
		 * @param region Region for creating new nodes
		 * @return Simplified node or nullptr
		 */
		Node *simplify_comparison_patterns(Node *node, Node *left, Node *right, Region *region);

		/**
		 * @brief Simplify identical operands (x op x patterns)
		 * @param node Node with identical operands
		 * @param operand The repeated operand
		 * @param region Region for creating new nodes
		 * @return Simplified node or nullptr
		 */
		Node *simplify_identical_operands(Node *node, Node *operand, Region *region);

		/**
		 * @brief Simplify operations with constant operands
		 * @param node Node to simplify
		 * @param left Left operand
		 * @param right Right operand
		 * @param region Region for creating new nodes
		 * @return Simplified node or nullptr
		 */
		Node *simplify_with_constants(Node *node, Node *left, Node *right, Region *region);

		/**
		 * @brief Apply strength reduction transformations
		 * @param node Node to transform
		 * @param left Left operand
		 * @param right Right operand
		 * @param region Region for creating new nodes
		 * @return Transformed node or nullptr
		 */
		Node *apply_strength_reduction(Node *node, Node *left, Node *right, Region *region);

		/**
		 * @brief Apply advanced algebraic patterns
		 * @param node Node to check
		 * @param left Left operand
		 * @param right Right operand
		 * @return Simplified node or nullptr
		 */
		static Node *apply_advanced_patterns(Node *node, Node *left, Node *right);

		/**
		 * @brief Simplify unary operations
		 * @param node Unary operation node
		 * @param input Input operand
		 * @param region Region for creating new nodes
		 * @return Simplified node or nullptr
		 */
		Node *simplify_unary_operation(Node *node, Node *input, Region *region);

		static Node *create_binary_op(Region *region, NodeType op_type, Node *left, Node *right, Node *insert_before);

		Node *create_shift(Region *region, NodeType shift_type, Node *value, std::uint32_t amount, Node *insert_before);

		Node *create_shift_add(Region *region, Node *var, std::uint32_t shift_amount, Node *addend,
		                       Node *insert_before);

		/* helper methods for pattern recognition */

		Node *create_shift_sub(Region *region, Node *var, std::uint32_t shift_amount, Node *subtrahend,
		                       Node *insert_before);

		/**
		 * @brief Check if a node represents a negation (0 - x)
		 * @param node Node to check
		 * @return True if node is a negation
		 */
		bool is_negation(Node *node) const;

		/**
		 * @brief Extract the negated value from a negation node
		 * @param negation Negation node (0 - x)
		 * @return The value being negated (x)
		 */
		Node *get_negated_value(Node *negation) const;

		/**
		 * @brief Check if a node is a compile-time constant
		 * @param node Node to check
		 * @return True if node is a literal constant
		 */
		static bool is_constant(Node *node);

		/**
		 * @brief Check if a value is 2^n + 1 for some n
		 * @param value Value to check
		 * @return True if value is one more than a power of 2
		 */
		static bool is_power_of_two_plus_one(std::uint64_t value);

		/**
		 * @brief Check if a value is 2^n - 1 for some n
		 * @param value Value to check
		 * @return True if value is one less than a power of 2
		 */
		static bool is_power_of_two_minus_one(std::uint64_t value);

		/**
		 * @brief Check if a value is a power of 2
		 * @param value Value to check
		 * @return True if value is 2^n for some n
		 */
		static bool is_power_of_two_constant_value(std::uint64_t value);

		/**
		 * @brief Get log2 of (value - 1)
		 * @param value Input value
		 * @return log2(value - 1)
		 */
		static std::uint32_t get_log2_of_constant_minus_one(std::uint64_t value);

		/**
		 * @brief Get log2 of (value + 1)
		 * @param value Input value
		 * @return log2(value + 1)
		 */
		static std::uint32_t get_log2_of_constant_plus_one(std::uint64_t value);

		/**
		 * @brief Get log2 of a value
		 * @param value Input value (must be power of 2)
		 * @return log2(value)
		 */
		static std::uint32_t get_log2_of_constant_value(std::uint64_t value);

		/**
		 * @brief Check if a node is a power of 2 constant
		 * @param node Node to check
		 * @return True if node is a power of 2 literal
		 */
		static bool is_power_of_two_constant(Node *node) ;

		/**
		 * @brief Get log2 of a power of 2 constant node
		 * @param node Power of 2 constant node
		 * @return log2 of the constant value
		 */
		static std::uint32_t get_log2_of_constant(Node *node) ;

		/**
		 * @brief Extract the numeric value from a constant node
		 * @param node Constant node
		 * @return Numeric value as uint64_t
		 */
		static std::uint64_t get_constant_value(Node *node);

		/**
		 * @brief Check if a data type is unsigned
		 * @param type Data type to check
		 * @return True if type is unsigned integer
		 */
		static bool is_unsigned_type(DataType type);

		/**
		 * @brief Check if a node is a zero constant
		 * @param node Node to check
		 * @return True if node represents zero
		 */
		bool is_zero_constant(Node *node) const;

		/**
		 * @brief Check if a node is a one constant
		 * @param node Node to check
		 * @return True if node represents one
		 */
		bool is_one_constant(Node *node) const;

		/**
		 * @brief Check if a node is a minus one constant
		 * @param node Node to check
		 * @return True if node represents -1
		 */
		bool is_minus_one_constant(Node *node) const;

		/**
		 * @brief Check if a node is a specific constant value
		 * @tparam T C++ type of the value
		 * @tparam DT Bloom IR data type
		 * @param node Node to check
		 * @param value Expected value
		 * @return True if node has the specified value
		 */
		template<typename T, DataType DT>
		bool is_constant_value(Node *node, T value) const
		{
			if (!node || node->ir_type != NodeType::LIT || node->type_kind != DT)
				return false;
			return node->as<DT>() == value;
		}

		/* node creation helpers */

		/**
		 * @brief Create a negation node (0 - value)
		 * @param region Region to create node in
		 * @param value Value to negate
		 * @param insert_before Node to insert before
		 * @return Negation node
		 */
		Node *create_negation(Region *region, Node *value, Node *insert_before);

		/**
		 * @brief Create a bitwise NOT node
		 * @param region Region to create node in
		 * @param value Value to negate bitwise
		 * @param insert_before Node to insert before
		 * @return Bitwise NOT node
		 */
		static Node *create_bitwise_not(Region *region, Node *value, Node *insert_before);

		/**
		 * @brief Create a logical NOT for boolean values
		 * @param region Region to create node in
		 * @param value Boolean value to negate
		 * @param insert_before Node to insert before
		 * @return Logical NOT node (XOR with true)
		 */
		Node *create_logical_not(Region *region, Node *value, Node *insert_before);

		/**
		 * @brief Create a left shift node
		 * @param region Region to create node in
		 * @param value Value to shift
		 * @param amount Shift amount
		 * @param insert_before Node to insert before
		 * @return Left shift node
		 */
		Node *create_shift_left(Region *region, Node *value, std::uint32_t amount, Node *insert_before);

		/**
		 * @brief Create a bitwise NOT of a constant
		 * @param region Region to create node in
		 * @param constant Constant node to negate
		 * @param insert_before Node to insert before
		 * @return Constant with bitwise NOT applied
		 */
		Node *create_bitwise_not_constant(Region *region, Node *constant, Node *insert_before);

		/**
		 * @brief Find or create a literal constant in a region
		 * @tparam T C++ type of the literal
		 * @tparam DT Bloom IR data type
		 * @param region Region to search/create in
		 * @param value Literal value
		 * @param insert_before Node to insert before if creating
		 * @return Existing or new literal node
		 */
		template<typename T, DataType DT>
		Node *find_or_create_literal(Region *region, T value, Node *insert_before)
		{
			for (Node *node: region->get_nodes())
			{
				if (node->ir_type == NodeType::LIT && node->type_kind == DT &&
					node->as<DT>() == value)
				{
					return node;
				}
			}

			Node *lit = region->get_module().get_context().create<Node>();
			lit->ir_type = NodeType::LIT;
			lit->type_kind = DT;
			lit->data.set<T, DT>(value);
			if (insert_before)
				region->insert_node_before(insert_before, lit);
			else
				region->add_node(lit);

			return lit;
		}

		/**
		 * @brief Create a zero literal of the specified type
		 * @param region Region to create literal in
		 * @param type Data type for the zero literal
		 * @param insert_before Node to insert before
		 * @return Zero literal node
		 */
		Node *create_zero_literal(Region *region, DataType type, Node *insert_before);

		/**
		 * @brief Create a one literal of the specified type
		 * @param region Region to create literal in
		 * @param type Data type for the one literal
		 * @param insert_before Node to insert before
		 * @return One literal node
		 */
		Node *create_one_literal(Region *region, DataType type, Node *insert_before);

		/**
		 * @brief Create a minus one literal of the specified type
		 * @param region Region to create literal in
		 * @param type Data type for the minus one literal
		 * @param insert_before Node to insert before
		 * @return Minus one literal node
		 */
		Node *create_minus_one_literal(Region *region, DataType type, Node *insert_before);

		/**
		 * @brief Create a constant for shift operations (typically uint32)
		 * @param region Region to create constant in
		 * @param shift_amount Shift amount value
		 * @param insert_before Node to insert before
		 * @return Shift constant node
		 */
		Node *create_constant_for_shift(Region *region, std::uint32_t shift_amount, Node *insert_before);

		/**
		 * @brief Create a constant with a specific value and type
		 * @param region Region to create constant in
		 * @param value Constant value
		 * @param type Data type for the constant
		 * @param insert_before Node to insert before
		 * @return Constant node with specified value and type
		 */
		Node *create_constant_with_value(Region *region, std::uint64_t value, DataType type, Node *insert_before);

		/**
		 * @brief Replace all uses of a node with another node
		 * @param node_to_replace Original node
		 * @param replacement_node Replacement node
		 */
		static void replace_all_uses(Node *node_to_replace, Node *replacement_node);

		/**
		 * @brief Connect a user node to its input nodes
		 * @param user Node that uses the inputs
		 * @param inputs Vector of input nodes
		 */
		static void connect_nodes(Node *user, const std::vector<Node *> &inputs);
	};
}
