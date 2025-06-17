/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <memory>
#include <vector>
#include <bloom/foundation/node.hpp>
#include <bloom/support/string-table.hpp>

namespace blm
{
	class Context;
	class Region;

	/**
	 * @brief Represents a compilation module
	 */
	class Module
	{
	public:
		/**
		 * @brief Construct a new Module
		 *
		 * @param ctx Context that owns this module
		 * @param name Name of the module
		 */
		Module(Context &ctx, std::string_view name);

		/**
		 * @brief Destructor
		 */
		~Module();

		Module(const Module &) = delete;

		Module &operator=(const Module &) = delete;

		Module(Module &&) = delete;

		Module &operator=(Module &&) = delete;

		/**
		 * @brief Get the name of the module
		 */
		[[nodiscard]] std::string_view get_name() const;

		/**
		 * @brief Get the root region of the module
		 */
		[[nodiscard]] Region *get_root_region() const;

		/**
		 * @brief Create a new region in this module
		 *
		 * @param name Name of the region
		 * @param parent Parent region (defaults to root region)
		 * @return Region* Pointer to created region
		 */
		Region *create_region(std::string_view name, Region *parent = nullptr);

		/**
		 * @brief Find a function by name
		 *
		 * @param name Name of the function
		 * @return Node* Pointer to function node, or nullptr if not found
		 */
		[[nodiscard]] Node *find_function(std::string_view name) const;

		/**
		 * @brief Register a function node with this module
		 *
		 * @param func Function node to register
		 */
		void add_function(Node *func);

		/**
		 * @brief Intern a string literal into the read-only data region
		 *
		 * This will create a new string node in the read-only data region if it does not already exist.
		 *
		 * @param str String to intern
		 * @return Node* Pointer to the interned string node
		 */
		Node* intern_string_literal(std::string_view str);

		/**
		 * @brief Get the context that owns this module
		 */
		Context &get_context()
		{
			return context;
		}

		/**
		 * @brief Get the context that owns this module (const version)
		 */
		[[nodiscard]] const Context &get_context() const
		{
			return context;
		}

		/**
		 * @brief Get all functions in this module
		 */
		[[nodiscard]]
		const std::vector<Node *> &get_functions() const
		{
			return functions;
		}

		/**
		 * @brief Get the read-only data region
		 */
		[[nodiscard]] Region *get_rodata_region() const
		{
			return rodata_region;
		}

	private:
		std::vector<Node *> functions;
		std::vector<std::unique_ptr<Region> > regions;
		Context &context;
		Region *root_region; /* also the global region */
		Region *rodata_region; /* read-only data region */
		StringTable::StringId name_id;
	};
}
