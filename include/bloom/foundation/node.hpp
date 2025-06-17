/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <cassert>
#include <cstdint>
#include <vector>
#include <bloom/foundation/typed-data.hpp>
#include <bloom/foundation/types.hpp>
#include <bloom/support/string-table.hpp>

namespace blm
{
	class Region;
	/**
	 * @brief Represents the type of operation an IR node performs
	 * @note This enum is used to identify the type of operation of an IR node
	 */
	enum class NodeType : std::uint16_t
	{
		/** @brief Entry point of a basic block or function */
		ENTRY,
		/** @brief Exit point of a basic block or function */
		EXIT,
		/** @brief Function parameter */
		PARAM,
		/** @brief Literal/constant value; includes string literal */
		LIT,
		/** @brief Arithmetic addition operation */
		ADD,
		/** @brief Arithmetic subtraction operation */
		SUB,
		/** @brief Arithmetic multiplication operation */
		MUL,
		/** @brief Arithmetic division operation */
		DIV,
		/** @brief Arithmetic modulus operation */
		MOD,
		/** @brief Comparison greater than operation */
		GT,
		/** @brief Comparison greater than or equal operation */
		GTE,
		/** @brief Comparison less than operation */
		LT,
		/** @brief Comparison less than or equal operation */
		LTE,
		/** @brief Comparison equal to operation */
		EQ,
		/** @brief Comparison is not equal to operation */
		NEQ,
		/** @brief Bitwise AND operation */
		BAND,
		/** @brief Bitwise OR operation */
		BOR,
		/** @brief Bitwise XOR operation */
		BXOR,
		/** @brief Bitwise NOT operation */
		BNOT,
		/** @brief Bitwise SHL operation */
		BSHL,
		/** @brief Bitwise SHR operation */
		BSHR,
		/** @brief Return statement */
		RET,
		/** @brief Function definition */
		FUNCTION,
		/** @brief Function call */
		CALL,
		/** @brief Function call parameter */
		CALL_PARAM,
		/** @brief Function return value */
		CALL_RESULT,
		/** @brief Stack memory allocation */
		STACK_ALLOC,
		/** @brief Dynamic memory allocation */
		HEAP_ALLOC,
		/** @brief Dynamic memory deallocation */
		FREE,
		/** @brief Load from a named memory location */
		LOAD,
		/** @brief Store to a named memory location */
		STORE,
		/** @brief Get address of a variable; &var */
		ADDR_OF,
		/** @brief Load via pointer dereference; *ptr */
		PTR_LOAD,
		/** @brief Store via pointer dereference; *ptr = var */
		PTR_STORE,
		/** @brief Pointer arithmetic; ptr + offset */
		PTR_ADD,
		/** @brief Type reinterpretion cast */
		REINTERPRET_CAST,
		/** @brief Thread-safe memory load */
		ATOMIC_LOAD,
		/** @brief Thread-safe memory store */
		ATOMIC_STORE,
		/** @brief Thread-safe memory exchange */
		ATOMIC_CAS,
		/** @brief A jump */
		JUMP,
		/** @brief A conditional jump */
		BRANCH,
		/** @brief A function call with exception handling */
		INVOKE,
		/** @brief Build a vector from scalar values of different operand */
		VECTOR_BUILD,
		/** @brief Extract a scalar from a vector */
		VECTOR_EXTRACT,
		/** @brief Build a vector from scalar values of same operand */
		VECTOR_SPLAT,
	};

	enum class AtomicOrdering : std::uint8_t
	{
		RELAXED = 0,
		ACQUIRE = 1 << 0,
		RELEASE = 1 << 1,

		EXCLUSIVE = 1 << 4,  /* note: ARM lowering will prefer LDXR/STXR */

		/* common combinations */
		ACQ_REL = ACQUIRE | RELEASE,
		SEQ_CST = ACQUIRE | RELEASE | (1 << 3),
	};

	/**
	 * @brief Bit flags representing node properties
	 */
	enum class NodeProps : std::uint16_t
	{
		/** @brief No special properties */
		NONE = 0,
		/** @brief Represents a static entity; Internal linkage */
		STATIC = 1 << 0,
		/** @brief Represents an expression that can be evaluated at compile time */
		CONSTEXPR = 1 << 1,
		/** @brief Represents an external entity; External linkage */
		EXTERN = 1 << 2,
		/** @brief Represents a driver function; entry point of the program */
		DRIVER = 1 << 3,
		/** @brief Represents a symbol to resolve across modules */
		EXPORT = 1 << 4,
		/** @brief Represents a node that should not be optimized e.g. C/C++'s `volatile` */
		NO_OPTIMIZE = 1 << 5,
		/** @brief Represents a read-only type */
		READONLY = 1 << 6
	};

	inline NodeProps operator|(NodeProps lhs, NodeProps rhs)
	{
		return static_cast<NodeProps>(
			static_cast<std::underlying_type_t<NodeProps>>(lhs) |
			static_cast<std::underlying_type_t<NodeProps>>(rhs)
		);
	}

	inline NodeProps &operator|=(NodeProps &lhs, const NodeProps rhs)
	{
		lhs = lhs | rhs;
		return lhs;
	}

	inline NodeProps operator&(NodeProps lhs, NodeProps rhs)
	{
		return static_cast<NodeProps>(
			static_cast<std::underlying_type_t<NodeProps>>(lhs) &
			static_cast<std::underlying_type_t<NodeProps>>(rhs)
		);
	}

	inline NodeProps &operator&=(NodeProps &lhs, const NodeProps rhs)
	{
		lhs = lhs & rhs;
		return lhs;
	}

	inline NodeProps operator^(NodeProps lhs, NodeProps rhs)
	{
		return static_cast<NodeProps>(
			static_cast<std::underlying_type_t<NodeProps>>(lhs) ^
			static_cast<std::underlying_type_t<NodeProps>>(rhs)
		);
	}

	inline NodeProps &operator^=(NodeProps &lhs, const NodeProps rhs)
	{
		lhs = lhs ^ rhs;
		return lhs;
	}

	inline NodeProps operator~(NodeProps prop)
	{
		return static_cast<NodeProps>(
			~static_cast<std::underlying_type_t<NodeProps>>(prop)
		);
	}

	/**
	 * @brief Represent an IR node
	 */
	struct Node
	{
		/** @brief Data that this IR node holds */
		TypedData data;
		/** @brief IR nodes that are depended on this node */
		std::vector<Node *> inputs = {};
		/** @brief IR Nodes that has dependency on this node */
		std::vector<Node *> users = {};
		/** @brief String interning reference */
		StringTable::StringId str_id = {};
		/** @brief Region this node belongs to */
		Region* parent_region = nullptr;
		/** @brief Operation type */
		NodeType ir_type = {};
		/** @brief Value type this node produces */
		DataType type_kind = DataType::VOID;
		/** @brief Type properties */
		NodeProps props = NodeProps::NONE;

		/**
		 * @brief Access node data with type safety
		 *
		 * @tparam T The DataType to cast the node's data to
		 * @return Reference to the type-safe data
		 */
		template<DataType T>
		typename DataTypeTraits<T>::type &as()
		{
			return data.get<T>();
		}

		/**
		 * @brief Access node data with type safety
		 *
		 * @tparam T The DataType to cast the node's data to
		 * @return Const reference to the type-safe data
		 */
		template<DataType T>
		const typename DataTypeTraits<T>::type &as() const
		{
			return data.get<T>();
		}

		/**
		 * @brief Access the referenced node for LOAD or CALL nodes
		 *
		 * @return Reference to the Node pointer stored in this node
		 * @note This is only valid for NodeType::LOAD or NodeType::CALL
		 */
		Node *&as_node_ref()
		{
			assert(ir_type == NodeType::LOAD || ir_type == NodeType::CALL || ir_type == NodeType::PTR_LOAD
				|| ir_type == NodeType::PTR_STORE);

			auto &ptr_data = data.get<DataType::POINTER>();
			return *std::launder(reinterpret_cast<Node **>(&ptr_data));
		}
	};
}
