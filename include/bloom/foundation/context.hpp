/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <memory>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/type-registry.hpp>
#include <bloom/support/allocator.hpp>
#include <bloom/support/string-table.hpp>

namespace blm
{
	struct Node;
	class Module;
	class Region;

	class Context
	{
	public:
		/**
		 * @brief Construct a new Context
		 */
		Context();

		/**
		 * @brief Destructor
		 */
		~Context();

		Context &operator=(const Context &) = delete;

		Context(Context &&) = delete;

		Context &operator=(Context &&) = delete;

		/**
		 * @brief Create a node object in the context
		 *
		 * Allocates memory for the node and constructs it with the given arguments.
		 *
		 * @tparam T Type of node to create (must derive from Node)
		 * @tparam Args Types of constructor arguments
		 * @param args Constructor arguments
		 * @return T* Pointer to created node
		 */
		template<typename T, typename... Args>
			requires(std::is_base_of_v<Node, T>)
		T *create(Args &&... args)
		{
			constexpr auto size = sizeof(T);
			void *m = allocator.allocate(size);
			if (!m)
				throw std::bad_alloc();
			return new(m) T(std::forward<Args>(args)...);
		}

		/**
		 * @brief Create a new module
		 * @param name Name of the module
		 * @return Pointer to the created module
		 */
		Module *create_module(std::string_view name);

		/**
		 * @brief Get a module by name
		 * @param name Name of the module to find
		 * @return Pointer to the module, or nullptr if not found
		 */
		Module *find_module(std::string_view name);

		/**
		 * @brief Intern a string
		 * @return The id of the interned string
		 */
		StringTable::StringId intern_string(std::string_view str);

		/**
		 * @param id String index that is produced from the table
		 * @return Read-only version of the initial string from the sparse-dense mapping
		 */
		std::string_view get_string(StringTable::StringId id) const;

		/**
		 * @brief Get the string table
		 * @return Reference to the string table
		 */
		StringTable &get_string_table()
		{
			return string_table;
		}

		/**
		 * @brief Get the string table (const version)
		 * @return Const reference to the string table
		 */
		const StringTable &get_string_table() const
		{
			return string_table;
		}

		/**
		 * @brief Get the type registry
		 * @return Reference to the type registry
		 */
		TypeRegistry &get_type_registry()
		{
			return type_registry;
		}

		/**
		 * @brief Get the type registry (const version)
		 * @return Const reference to the type registry
		 */
		const TypeRegistry &get_type_registry() const
		{
			return type_registry;
		}

		/**
		 * @brief Create a pointer type
		 * @param pointee The type being pointed to
		 * @param addr_space The address space (default: 0)
		 * @return DataType The pointer type ID
		 */
		DataType create_pointer_type(const DataType pointee, const std::uint32_t addr_space = 0)
		{
			return type_registry.create_pointer_type(pointee, addr_space);
		}

		/**
		 * @brief Create an array type
		 * @param element_type The type of elements in the array
		 * @param count The number of elements in the array
		 * @return DataType The array type ID
		 */
		DataType create_array_type(const DataType element_type, const std::uint64_t count)
		{
			return type_registry.create_array_type(element_type, count);
		}

		/**
		 * @brief Create a struct type
		 * @param fields The struct fields (name/type pairs)
		 * @param size The size of the struct in bytes
		 * @param align The alignment of the struct in bytes
		 * @return DataType The struct type ID
		 */
		DataType create_struct_type(
			const std::vector<std::pair<std::string, DataType> > &fields,
			const std::uint32_t size, const std::uint32_t align)
		{
			return type_registry.create_struct_type(fields, size, align);
		}

		/**
		 * @brief Create a function type
		 * @param return_type The function's return type
		 * @param param_types The function's parameter types
		 * @param is_vararg Whether the function accepts variable arguments
		 * @return DataType The function type ID
		 */
		DataType create_function_type(
			const DataType return_type,
			const std::vector<DataType> &param_types,
			const bool is_vararg = false)
		{
			return type_registry.create_function_type(return_type, param_types, is_vararg);
		}

		DataType create_vector_type(const DataType element_type, const std::uint32_t count)
		{
			return type_registry.create_vector_type(element_type, count);
		}

		/**
		 * @brief Get a type's data by its ID
		 * @param type The type ID
		 * @return const TypedData& Reference to the type data
		 */
		const TypedData &get_type(const DataType type) const
		{
			return type_registry.get_type(type);
		}

		/**
		 * @brief Register a type
		 * @param type_data The type data to register
		 * @return DataType The registered type's ID
		 */
		DataType register_type(TypedData &&type_data)
		{
			return type_registry.register_type(std::move(type_data));
		}

	private:
		ach::allocator<char> allocator;
		std::unordered_map<StringTable::StringId, Module *> module_map;
		std::vector<std::unique_ptr<Module> > modules;
		StringTable string_table;
		TypeRegistry type_registry;
	};
}
