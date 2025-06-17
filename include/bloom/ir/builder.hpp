/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <vector>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/types.hpp>

namespace blm
{
	class Builder;

	/**
	 * @brief Represents a function being built with fluent API
	 */
	class FunctionBuilder
	{
	public:
		FunctionBuilder(Builder &builder, Node *function_node, Region *function_region);

		/**
		 * @brief Add a parameter to this function
		 * @param name Parameter name
		 * @param type Parameter type
		 * @return Parameter node
		 */
		Node *add_parameter(std::string_view name, DataType type);

		/**
		 * @brief Define the function body using a lambda
		 * @param body_func Lambda that builds the function body
		 */
		void body(const std::function<void()> &body_func);

		/**
		 * @brief Get the function node
		 */
		[[nodiscard]] Node *get_function() const
		{
			return function;
		}

		/**
		 * @brief Get the function region
		 */
		[[nodiscard]] Region *get_region() const
		{
			return region;
		}

	private:
		Builder &builder;
		Node *function;
		Region *region;
		std::vector<Node *> parameters;
	};

	/**
	 * @brief Represents a basic block being built
	 */
	class BlockBuilder
	{
	public:
		BlockBuilder(Builder &builder, Region *block_region);

		/**
		 * @brief Execute code in this block's context
		 * @param block_func Lambda that builds block contents
		 */
		void operator()(const std::function<void()> &block_func);

		/**
		 * @brief Create a return statement in this block
		 * @param value Optional return value
		 */
		void ret(Node *value = nullptr);

		/**
		 * @brief Get the block region
		 */
		[[nodiscard]] Region *get_region() const
		{
			return region;
		}

	private:
		Builder &builder;
		Region *region;
	};

	/**
	 * @brief Represents the result of creating invoke blocks
	 */
	struct InvokeBlocks
	{
		BlockBuilder normal;
		BlockBuilder except;

		InvokeBlocks(Builder &builder, Region *normal_region, Region *except_region) : normal(builder, normal_region),
			except(builder, except_region) {}
	};

	/**
	 * @brief Represents a loop structure being built
	 */
	struct LoopStructure
	{
		BlockBuilder header;
		BlockBuilder body;
		BlockBuilder exit;

		LoopStructure(Builder &builder, Region *header_region, Region *body_region,
		              Region *exit_region) : header(builder, header_region), body(builder, body_region),
		                                     exit(builder, exit_region) {}
	};

	/**
	 * @brief Modern, fluent API for building Bloom IR
	 */
	class Builder
	{
	public:
		/**
		 * @brief Construct a new Builder
		 * @param ctx Context for creating nodes and modules
		 */
		explicit Builder(Context &ctx);

		/**
		 * @brief Create a new module
		 * @param name Module name
		 * @return Pointer to created module
		 */
		Module *create_module(std::string_view name);

		/**
		 * @brief Set the current module
		 * @param module Module to set as current
		 */
		void set_current_module(Module *module);

		/**
		 * @brief Get the current module
		 */
		[[nodiscard]] Module *get_current_module() const
		{
			return current_module;
		}

		/**
		 * @brief Create a function with fluent API
		 * @param name Function name
		 * @param param_types Parameter types
		 * @param return_type Return type
		 * @param is_vararg Whether function accepts variable arguments
		 * @return FunctionBuilder for defining the function
		 */
		FunctionBuilder create_function(std::string_view name,
		                                const std::vector<DataType> &param_types,
		                                DataType return_type,
		                                bool is_vararg = false);

		/**
		 * @brief Create a function type
		 */
		DataType function_type(DataType return_type,
		                       const std::vector<DataType> &param_types,
		                       bool is_vararg = false);

		/**
		 * @brief Create a pointer type
		 */
		DataType pointer_type(DataType pointee, std::uint32_t addr_space = 0);

		/**
		 * @brief Create an array type
		 */
		DataType array_type(DataType element_type, std::uint64_t count);

		/**
		 * @brief Create a struct type
		 */
		DataType struct_type(const std::vector<std::pair<std::string, DataType> > &fields,
		                     std::uint32_t size, std::uint32_t align);

		/**
		 * @brief Create integer literal
		 */
		template<typename T>
		Node *literal(T value)
		{
			Node *lit = create_node(NodeType::LIT);
			if constexpr (std::is_same_v<T, std::int8_t>)
			{
				lit->type_kind = DataType::INT8;
				lit->data.set<std::int8_t, DataType::INT8>(value);
			}
			else if constexpr (std::is_same_v<T, std::int16_t>)
			{
				lit->type_kind = DataType::INT16;
				lit->data.set<std::int16_t, DataType::INT16>(value);
			}
			else if constexpr (std::is_same_v<T, std::int32_t>)
			{
				lit->type_kind = DataType::INT32;
				lit->data.set<std::int32_t, DataType::INT32>(value);
			}
			else if constexpr (std::is_same_v<T, std::int64_t>)
			{
				lit->type_kind = DataType::INT64;
				lit->data.set<std::int64_t, DataType::INT64>(value);
			}
			else if constexpr (std::is_same_v<T, std::uint8_t>)
			{
				lit->type_kind = DataType::UINT8;
				lit->data.set<std::uint8_t, DataType::UINT8>(value);
			}
			else if constexpr (std::is_same_v<T, std::uint16_t>)
			{
				lit->type_kind = DataType::UINT16;
				lit->data.set<std::uint16_t, DataType::UINT16>(value);
			}
			else if constexpr (std::is_same_v<T, std::uint32_t>)
			{
				lit->type_kind = DataType::UINT32;
				lit->data.set<std::uint32_t, DataType::UINT32>(value);
			}
			else if constexpr (std::is_same_v<T, std::uint64_t>)
			{
				lit->type_kind = DataType::UINT64;
				lit->data.set<std::uint64_t, DataType::UINT64>(value);
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				lit->type_kind = DataType::INT32;
				lit->data.set<std::int32_t, DataType::INT32>(static_cast<std::int32_t>(value));
			}
			else
			{
				static_assert(sizeof(T) == 0, "unsupported literal type");
			}

			return lit;
		}

		/**
		 * @brief Create string literal
		 * @param str String value
		 */
		Node *literal(std::string_view str);

		/**
		 * @brief Create string literal
		 * @param str C-style string
		 * @note Literally here because of template deduction
		 */
		Node* literal(const char* str);

		/**
		 * @brief Create boolean literal
		 */
		Node *literal(bool value);

		/**
		 * @brief Create floating-point literal
		 */
		Node *literal(float value);

		Node *literal(double value);

		/**
		 * @brief Create addition: lhs + rhs
		 */
		Node *add(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create subtraction: lhs - rhs
		 */
		Node *sub(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create multiplication: lhs * rhs
		 */
		Node *mul(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create division: lhs / rhs
		 */
		Node *div(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create modulo: lhs % rhs
		 */
		Node *mod(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create bitwise AND: lhs & rhs
		 */
		Node *band(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create bitwise OR: lhs | rhs
		 */
		Node *bor(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create bitwise XOR: lhs ^ rhs
		 */
		Node *bxor(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create bitwise NOT: ~value
		 */
		Node *bnot(Node *value);

		/**
		 * @brief Create left shift: lhs << rhs
		 */
		Node *bshl(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create right shift: lhs >> rhs
		 */
		Node *bshr(Node *lhs, Node *rhs, std::optional<DataType> result_type = std::nullopt);

		/**
		 * @brief Create equality comparison: lhs == rhs
		 */
		Node *eq(Node *lhs, Node *rhs);

		/**
		 * @brief Create inequality comparison: lhs != rhs
		 */
		Node *neq(Node *lhs, Node *rhs);

		/**
		 * @brief Create less than: lhs < rhs
		 */
		Node *lt(Node *lhs, Node *rhs);

		/**
		 * @brief Create less than or equal: lhs <= rhs
		 */
		Node *lte(Node *lhs, Node *rhs);

		/**
		 * @brief Create greater than: lhs > rhs
		 */
		Node *gt(Node *lhs, Node *rhs);

		/**
		 * @brief Create greater than or equal: lhs >= rhs
		 */
		Node *gte(Node *lhs, Node *rhs);

		/**
		 * @brief Create load: load value from address
		 * Convention: inputs[0] = address
		 */
		Node *load(Node *address, DataType result_type);

		/**
		 * @brief Create store: store value at address
		 * Convention: inputs[0] = value, inputs[1] = address
		 */
		Node *store(Node *value, Node *address);

		/**
		 * @brief Create pointer load: *ptr
		 * Convention: inputs[0] = pointer
		 */
		Node *ptr_load(Node *ptr, DataType result_type);

		/**
		 * @brief Create pointer store: *ptr = value
		 * Convention: inputs[0] = value, inputs[1] = pointer
		 */
		Node *ptr_store(Node *value, Node *ptr);

		/**
		 * @brief Create pointer arithmetic: ptr + offset
		 * Convention: inputs[0] = base_pointer, inputs[1] = offset
		 */
		Node *ptr_add(Node *base_ptr, Node *offset);

		/**
		 * @brief Create atomic load with optional ordering
		 * Convention: inputs[0] = address, inputs[1] = ordering (optional)
		 */
		Node *atomic_load(Node *address, DataType result_type, Node *ordering = nullptr);

		/**
		 * @brief Create atomic store with optional ordering
		 * Convention: inputs[0] = value, inputs[1] = address, inputs[2] = ordering (optional)
		 */
		Node *atomic_store(Node *value, Node *address, Node *ordering = nullptr);

		/**
		 * @brief Create atomic compare-and-swap with optional ordering
		 * Convention: inputs[0] = address, inputs[1] = expected, inputs[2] = new_value, inputs[3] = ordering (optional)
		 */
		Node *atomic_cas(Node *address, Node *expected, Node *new_value, Node *ordering = nullptr);

		/**
		 * @brief Create atomic ordering literal
		 */
		Node *atomic_ordering(AtomicOrdering ordering);

		/**
		 * @brief Create address-of: &variable
		 * Convention: inputs[0] = variable
		 */
		Node *addr_of(Node *variable);

		/**
		 * @brief Create stack allocation
		 */
		Node *stack_alloc(Node *size, DataType type, std::uint32_t alignment = 0);

		/**
		 * @brief Create heap allocation
		 */
		Node *heap_alloc(Node *function, Node *size, DataType type, std::uint32_t alignment = 0);

		/**
		 * @brief Create free operation
		 */
		Node *free(Node *ptr);

		/**
		 * @brief Create function call
		 * Convention: inputs[0] = function, inputs[1..n] = arguments
		 */
		Node *call(Node *function, const std::vector<Node *> &args = {});

		/**
		 * @brief Create return statement
		 * Convention: inputs[0] = return_value (optional)
		 */
		Node *ret(Node *value = nullptr);

		/**
		 * @brief Create branch instruction
		 * Convention: inputs[0] = condition, inputs[1] = true_target, inputs[2] = false_target
		 */
		Node *branch(Node *condition, Node *true_target, Node *false_target);

		/**
		 * @brief Create jump instruction
		 * Convention: inputs[0] = target
		 */
		Node *jump(Node *target);

		/**
		 * @brief Create invoke with exception handling
		 * Convention: inputs[0] = function, inputs[1..n] = args,
		 *            inputs[n+1] = normal_target, inputs[n+2] = except_target
		 */
		Node *invoke(Node *function, const std::vector<Node *> &args,
		             Node *normal_target, Node *except_target);

		/**
		 * @brief Create invoke blocks for exception handling
		 */
		InvokeBlocks create_invoke_blocks(std::string_view normal_name,
		                                  std::string_view except_name);

		/**
		 * @brief Create if-else structure
		 */
		std::pair<BlockBuilder, BlockBuilder> create_if(Node *condition,
		                                                std::string_view true_name = "if_true",
		                                                std::string_view false_name = "if_false");

		/**
		 * @brief Create while loop structure
		 */
		LoopStructure create_while_loop(std::string_view header_name = "loop_header",
		                                std::string_view body_name = "loop_body",
		                                std::string_view exit_name = "loop_exit");

		/**
		 * @brief Create a basic block
		 */
		BlockBuilder create_block(std::string_view name, Region *parent = nullptr);

		/**
		 * @brief Get current insertion region
		 */
		[[nodiscard]] Region *get_current_region() const
		{
			return current_region;
		}

		/**
		 * @brief Set insertion point
		 */
		void set_insertion_point(Region *region);

		/**
		 * @brief Create entry node for current region
		 */
		Node *create_entry();

		/**
		 * @brief Create exit node for current region
		 */
		Node *create_exit();

		/**
		 * @brief Get the context
		 */
		[[nodiscard]] Context &get_context() const
		{
			return ctx;
		}

		/**
		 * @brief Create a node and add it to current region
		 */
		Node *create_node(NodeType type, DataType result_type = DataType::VOID);

		/**
		 * @brief Create a node with automatic naming
		 */
		Node *name_node(Node *node, std::string_view name);

	private:
		Context &ctx;
		Module *current_module = nullptr;
		Region *current_region = nullptr;

		/**
		 * @brief Connect nodes following operand conventions
		 */
		static void connect_nodes(Node *user, const std::vector<Node *> &inputs);

		/**
		 * @brief Infer binary operation result type
		 */
		static DataType infer_binary_result_type(NodeType op_type, Node *lhs, Node *rhs);

		/**
		 * @brief Create region with automatic entry node
		 */
		Region *create_region_with_entry(std::string_view name, Region *parent = nullptr);
	};
}
