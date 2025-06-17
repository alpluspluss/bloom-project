/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <cstdint>

namespace blm
{
	enum class DataType : std::uint16_t
	{
		VOID,
		BOOL,
		INT8, INT16, INT32, INT64,
		UINT8, UINT16, UINT32, UINT64,
		FLOAT32, FLOAT64,
		POINTER,
		ARRAY,
		STRUCT,
		FUNCTION,
		VECTOR,
		STRING,
		EXTENDED /* for complex data that doesn't fit inline */
	};

	/**
	 * @brief Type flags encoded in high bits of extended type IDs
	 */
	enum class TypeFlags : std::uint16_t
	{
		NONE = 0,
		VECTOR = 1u << 11,   /* bit 11: vector type */
		POINTER = 1u << 12,  /* bit 12: pointer type */
		ARRAY = 1u << 13,    /* bit 13: array type */
		STRUCT = 1u << 14,   /* bit 14: struct type */
		FUNCTION = 1u << 15, /* bit 15: function type */
		/* bits 0-11 store actual type ID; 4096 unique types */
	};

	/**
	 * @brief Fast type checking functions using bitwise operations
	 */
	constexpr  DataType get_base_type_id(DataType type) noexcept
	{
		constexpr std::uint16_t ID_MASK = 0x07FF;
		return static_cast<DataType>(static_cast<std::uint16_t>(type) & ID_MASK);
	}

	constexpr DataType encode_type_flags(DataType base_id, TypeFlags flags) noexcept
	{
		return static_cast<DataType>(static_cast<std::uint16_t>(base_id) | static_cast<std::uint16_t>(flags));
	}

	constexpr bool is_pointer_type(DataType type) noexcept
	{
		if (type == DataType::POINTER)
			return true;
		return (static_cast<std::uint16_t>(type) & static_cast<std::uint16_t>(TypeFlags::POINTER)) != 0;
	}

	constexpr bool is_array_type(DataType type) noexcept
	{
		if (type == DataType::ARRAY)
			return true;
		return (static_cast<std::uint16_t>(type) & static_cast<std::uint16_t>(TypeFlags::ARRAY)) != 0;
	}

	constexpr bool is_struct_type(DataType type) noexcept
	{
		if (type == DataType::STRUCT)
			return true;
		return (static_cast<std::uint16_t>(type) & static_cast<std::uint16_t>(TypeFlags::STRUCT)) != 0;
	}

	constexpr bool is_function_type(DataType type) noexcept
	{
		if (type == DataType::FUNCTION)
			return true;
		return (static_cast<std::uint16_t>(type) & static_cast<std::uint16_t>(TypeFlags::FUNCTION)) != 0;
	}

	constexpr bool is_vector_type(DataType type) noexcept
	{
		return (static_cast<std::uint16_t>(type) & static_cast<std::uint16_t>(TypeFlags::VECTOR)) != 0;
	}

	constexpr DataType get_vector_element_type(const DataType vector_type) noexcept
	{
		return get_base_type_id(vector_type);
	}
}
