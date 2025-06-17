/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/types.hpp>

namespace blm
{
   /**
    * @brief Modern IR printer with consistent naming and clean output
    */
   class IRPrinter
   {
   public:
   	/**
   	 * @brief Print options for controlling output format
   	 */
   	struct PrintOptions
   	{
   		bool include_debug_info = false;
   		bool include_type_annotations = true;
   		bool include_node_comments = false;
   		bool compact_literals = false;
   		std::size_t indent_size = 4;
   		bool use_spaces = true;  /* false = tabs */
   	};

   	/**
	 * @brief Construct a new IR printer
	 * @param os Output stream to write to
	 * @description for backend compatibility, this constructor is deprecated.
	 */
   	explicit IRPrinter(std::ostream &os);

   	/**
   	 * @brief Construct a new IR printer
   	 * @param os Output stream to write to
   	 * @param options Print formatting options
   	 */
   	explicit IRPrinter(std::ostream &os, const PrintOptions &options);

   	/**
   	 * @brief Print a complete module
   	 * @param module Module to print
   	 */
   	void print_module(const Module &module);

   	/**
   	 * @brief Print a single function
   	 * @param func Function to print
   	 * @param module Module containing the function (for context)
   	 */
   	void print_function(Node *func, const Module &module);

   	/**
   	 * @brief Print a region (basic block)
   	 * @param region Region to print
   	 * @param indent_level Current indentation level
   	 */
   	void print_region(const Region &region, std::size_t indent_level = 0);

   	/**
   	 * @brief Print a single instruction
   	 * @param node Instruction to print
   	 * @param indent_level Current indentation level
   	 */
   	void print_instruction(Node *node, std::size_t indent_level = 0);

   	/**
   	 * @brief Print a data type
   	 * @param type Type to print
   	 * @param ctx Context for type resolution
   	 */
   	void print_type(DataType type, const Context &ctx);

   	/**
   	 * @brief Get the string representation of a node (for references)
   	 * @param node Node to get name for
   	 * @return String representation (e.g., "%1", "$func_name")
   	 */
   	std::string get_node_name(Node *node);

   	/**
   	 * @brief Get the string representation of a basic block
   	 * @param region Region to get name for
   	 * @return String representation (e.g., "entry", "loop_header")
   	 */
   	std::string get_block_name(const Region *region);

   	/**
   	 * @brief Reset naming state (useful for printing multiple modules)
   	 */
   	void reset_names();

   private:
   	std::ostream &os;
   	PrintOptions options;

   	/* naming state */
   	std::unordered_map<Node *, std::string> node_names;
   	std::unordered_map<const Region *, std::string> region_names;
   	std::unordered_set<DataType> printed_types;

   	/* counters for generating unique names */
   	std::size_t next_ssa_id = 0;
   	std::size_t next_block_id = 0;
   	std::size_t next_temp_func_id = 0;

   	/* current context for name resolution */
   	const Module *current_module = nullptr;

   	/**
   	 * @brief Pre-populate names for all module entities
   	 * @param module Module to analyze
   	 */
   	void build_name_mappings(const Module &module);

   	/**
   	 * @brief Collect and print user-defined type declarations
   	 * @param module Module to scan for types
   	 */
   	void print_type_declarations(const Module &module);

   	/**
   	 * @brief Generate a unique SSA name for a value-producing node
   	 * @param node Node to name
   	 * @return Generated name (e.g., "%0", "%result")
   	 */
   	std::string generate_ssa_name(Node *node);

   	/**
   	 * @brief Generate a function name
   	 * @param node Function node
   	 * @return Generated name (e.g., "$main", "$func_0")
   	 */
   	std::string generate_function_name(Node *node);

   	/**
   	 * @brief Generate a basic block name
   	 * @param region Region to name
   	 * @return Generated name (e.g., "entry", "loop_header")
   	 */
   	std::string generate_block_name(const Region *region);

   	/**
   	 * @brief Print function signature (name, parameters, return type)
   	 * @param func Function node
   	 * @param func_region Region containing function body
   	 * @param ctx Context for type resolution
   	 */
   	void print_function_signature(Node *func, const Region *func_region, const Context &ctx);

   	/**
   	 * @brief Print function parameters
   	 * @param func_region Region containing parameters
   	 * @param ctx Context for name resolution
   	 */
   	void print_function_parameters(const Region *func_region, const Context &ctx);

   	/**
   	 * @brief Print a literal value
   	 * @param node Literal node
   	 */
   	void print_literal_value(Node *node);

   	/**
   	 * @brief Print node attributes (extern, static, etc.)
   	 * @param node Node to print attributes for
   	 */
   	void print_node_attributes(Node *node);

   	/**
   	 * @brief Print debug information as a comment
   	 * @param node Node to print debug info for
   	 * @param region Region containing the node
   	 */
   	void print_debug_comment(Node *node, const Region *region);

   	/**
   	 * @brief Generate indentation string using structured control flow
   	 * @param level Indentation level
   	 * @return Indentation string
   	 */
   	[[nodiscard]] std::string indent(std::size_t level) const;

   	/**
   	 * @brief Check if a type needs explicit declaration
   	 * @param type Type to check
   	 * @param ctx Context for type analysis
   	 * @return True if type should be declared
   	 */
   	[[nodiscard]] static bool needs_type_declaration(DataType type, const Context &ctx);

   	/**
   	 * @brief Find the region containing a function's body
   	 * @param func Function node
   	 * @param module Module containing the function
   	 * @return Function's body region, or nullptr if not found
   	 */
   	static const Region *find_function_region(Node *func, const Module &module);

   	/**
   	 * @brief Check if a node produces a value
   	 * @param node Node to check
   	 * @return True if node produces a value
   	 */
   	static bool is_value_producing(Node *node);

   	/**
   	 * @brief Print operand list for instructions
   	 * @param operands List of operand nodes
   	 * @param start_index Index to start printing from (default 0)
   	 */
   	void print_operand_list(const std::vector<Node *> &operands, std::size_t start_index = 0);

   	/**
   	 * @brief Print a binary operation
   	 * @param node Operation node
   	 * @param op_symbol Symbol for the operation (e.g., "+", "*")
   	 * @param indent_level Current indentation
   	 */
   	void print_binary_op(Node *node, const std::string &op_symbol, std::size_t indent_level);

   	/**
   	 * @brief Print a unary operation
   	 * @param node Operation node
   	 * @param op_symbol Symbol for the operation (e.g., "~", "-")
   	 * @param indent_level Current indentation
   	 */
   	void print_unary_op(Node *node, const std::string &op_symbol, std::size_t indent_level);

   	/**
   	 * @brief Print a comparison operation
   	 * @param node Comparison node
   	 * @param op_symbol Symbol for the comparison (e.g., "==", "<")
   	 * @param indent_level Current indentation
   	 */
   	void print_comparison_op(Node *node, const std::string &op_symbol, std::size_t indent_level);

   	/**
   	 * @brief Print memory operations (load/store)
   	 * @param node Memory operation node
   	 * @param indent_level Current indentation
   	 */
   	void print_memory_op(Node *node, std::size_t indent_level);

   	/**
   	 * @brief Print control flow operations (branch/jump)
   	 * @param node Control flow node
   	 * @param indent_level Current indentation
   	 */
   	void print_control_flow_op(Node *node, std::size_t indent_level);

   	/**
   	 * @brief Print function call operations
   	 * @param node Call node
   	 * @param indent_level Current indentation
   	 */
   	void print_call_op(Node *node, std::size_t indent_level);

   	void print_rodata_section(const Module &module);

   	void print_globals_section(const Module &module);
   };
}
