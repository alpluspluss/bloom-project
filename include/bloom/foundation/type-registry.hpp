/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <vector>
#include <bloom/foundation/typed-data.hpp>
#include <bloom/foundation/types.hpp>

namespace blm
{
    class Context;

    /**
     * @brief Registry for managing and deduplicating types in the IR system
     */
    class TypeRegistry
    {
    public:
       /**
        * @brief Constructs a new TypeRegistry
        */
       explicit TypeRegistry();

       /**
        * @brief Registers a type in the registry
        *
        * If the type already exists in the registry (determined by structural equality),
        * returns the existing type ID instead of creating a new one.
        *
        * @param type_data The type data to register
        * @return DataType The registered type's ID
        */
       DataType register_type(TypedData &&type_data);

       /**
        * @brief Creates a pointer type
        *
        * @param pointee The type being pointed to
        * @param addr_space The address space (default: 0)
        * @return DataType The pointer type ID
        */
       DataType create_pointer_type(DataType pointee, std::uint32_t addr_space = 0);

       /**
        * @brief Creates an array type
        *
        * @param element_type The type of elements in the array
        * @param count The number of elements in the array
        * @return DataType The array type ID
        */
       DataType create_array_type(DataType element_type, std::uint64_t count);

       /**
        * @brief Creates a struct type
        *
        * @param fields The struct fields (name/type pairs)
        * @param size The size of the struct in bytes
        * @param align The alignment of the struct in bytes
        * @return DataType The struct type ID
        */
       DataType create_struct_type(
          const std::vector<std::pair<std::string, DataType>>& fields,
          std::uint32_t size, std::uint32_t align);

       /**
        * @brief Creates a function type
        *
        * @param return_type The function's return type
        * @param param_types The function's parameter types
        * @param is_vararg Whether the function accepts variable arguments
        * @return DataType The function type ID
        */
       DataType create_function_type(
          DataType return_type,
          const std::vector<DataType>& param_types,
          bool is_vararg = false);

       /**
        * @brief Creates a vector type
        *
        * @param element_type The type of elements in the vector
        * @param count The number of elements in the vector
        * @return DataType The vector type ID
        */
       DataType create_vector_type(DataType element_type, std::uint32_t count);

       /**
        * @brief Gets a type's data by its ID
        *
        * @param type The type ID
        * @return const TypedData& Reference to the type data
        * @throws std::logic_error if the type ID is invalid
        */
       [[nodiscard]]
       const TypedData& get_type(DataType type) const;

       /**
        * @brief Reserves a type ID for forward declaration
        *
        * Allows creating a placeholder type that can be completed later
        * with the actual type definition.
        *
        * @return DataType The reserved type ID
        */
       DataType reserve_type_id();

       /**
        * @brief Completes a forward-declared type
        *
        * Associates a previously reserved type ID with an actual type.
        *
        * @param placeholder The placeholder type ID from reserve_type_id()
        * @param actual The actual type ID to associate with the placeholder
        */
       void complete_type(DataType placeholder, DataType actual);

    private:
       std::unordered_map<DataType, TypedData> types;
       std::uint32_t nid = static_cast<std::uint32_t>(DataType::EXTENDED);

       struct TypeHasher
       {
          std::size_t operator()(const TypedData& data) const;
       };

       struct TypeEqual
       {
          bool operator()(const TypedData& a, const TypedData& b) const;
       };

       std::unordered_map<TypedData, std::uint32_t, TypeHasher, TypeEqual> type_lookup;
    };
}
