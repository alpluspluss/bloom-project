/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <iostream>
#include <bloom/foundation/module.hpp>

namespace blm
{
	/**
	 * @brief Prints a tree visualization of module structure for debugging
	 */
	class TreePrinter
	{
	public:
		/**
		 * @brief Construct a new TreePrinter
		 * @param os Output stream to print to
		 */
		explicit TreePrinter(std::ostream& os = std::cout);

		/**
		 * @brief Print module tree structure
		 * @param module The module to visualize
		 */
		void print_module(const Module& module);

	private:
		/**
		 * @brief Print function and its regions
		 * @param func Function node to print
		 * @param module Module context for string lookups
		 */
		void print_function_tree(Node* func, const Module& module);

		/**
		 * @brief Print a region with proper indentation
		 * @param region Region to print
		 * @param depth Indentation depth
		 * @param is_last Whether this is the last child at this level
		 */
		void print_region(const Region* region, int depth, bool is_last);

		/**
		 * @brief Check if region has ENTRY node
		 * @param region Region to check
		 * @return True if region has ENTRY node
		 */
		static bool has_entry_node(const Region* region);

		/**
		 * @brief Generate indentation string for tree structure
		 * @param depth Current depth
		 * @param is_last Whether this is the last item at this level
		 * @return Indentation string with tree symbols
		 */
		static std::string get_indent(int depth, bool is_last);

		std::ostream& os;
	};
}
