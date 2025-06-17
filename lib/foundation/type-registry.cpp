/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <cassert>
#include <mutex>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/type-registry.hpp>
#include <bloom/foundation/typed-data.hpp>
#include <bloom/foundation/types.hpp>

namespace blm
{
	TypeRegistry::TypeRegistry() = default;

	DataType TypeRegistry::register_type(TypedData &&type_data)
	{
		if (const auto it = type_lookup.find(type_data);
			it != type_lookup.end())
		{
			return static_cast<DataType>(it->second);
		}

		std::uint32_t id = nid++;
		types[static_cast<DataType>(id)] = std::move(type_data);
		type_lookup[types[static_cast<DataType>(id)]] = id;
		return static_cast<DataType>(id);
	}

	DataType TypeRegistry::create_pointer_type(const DataType pointee, const std::uint32_t addr_space)
	{
		auto ptr_data = DataTypeTraits<DataType::POINTER>::type { pointee, addr_space };
		TypedData type_data;
		type_data.set<DataTypeTraits<DataType::POINTER>::type, DataType::POINTER>(ptr_data);
		const DataType base_id = register_type(std::move(type_data));
		return encode_type_flags(base_id, TypeFlags::POINTER);
	}

	DataType TypeRegistry::create_array_type(const DataType element_type, const std::uint64_t count)
	{
		auto arr_data = DataTypeTraits<DataType::ARRAY>::type { element_type, count };
		TypedData type_data;
		type_data.set<DataTypeTraits<DataType::ARRAY>::type, DataType::ARRAY>(arr_data);
		DataType base_id = register_type(std::move(type_data));
		return encode_type_flags(base_id, TypeFlags::ARRAY);
	}

	DataType TypeRegistry::create_struct_type(
		const std::vector<std::pair<std::string, DataType> > &fields,
		const std::uint32_t size, std::uint32_t align)
	{
		auto struct_data = DataTypeTraits<DataType::STRUCT>::type { size, align, fields };
		TypedData type_data;
		type_data.set<DataTypeTraits<DataType::STRUCT>::type, DataType::STRUCT>(std::move(struct_data));
		DataType base_id = register_type(std::move(type_data));
		return encode_type_flags(base_id, TypeFlags::STRUCT);
	}

	DataType TypeRegistry::create_function_type(
		const DataType return_type,
		const std::vector<DataType> &param_types,
		const bool is_vararg)
	{
		auto func_data = DataTypeTraits<DataType::FUNCTION>::type { param_types, return_type, is_vararg };
		TypedData type_data;
		type_data.set<DataTypeTraits<DataType::FUNCTION>::type, DataType::FUNCTION>(std::move(func_data));
		DataType base_id = register_type(std::move(type_data));
		return encode_type_flags(base_id, TypeFlags::FUNCTION);
	}

	DataType TypeRegistry::create_vector_type(const DataType element_type, const std::uint32_t count)
	{
		auto vec_data = DataTypeTraits<DataType::VECTOR>::type { element_type, count };
		TypedData type_data;
		type_data.set<DataTypeTraits<DataType::VECTOR>::type, DataType::VECTOR>(vec_data);
		DataType base_id = register_type(std::move(type_data));
		return encode_type_flags(base_id, TypeFlags::VECTOR);
	}

	const TypedData &TypeRegistry::get_type(const DataType type) const
	{
		const DataType base_id = get_base_type_id(type);
		const auto it = types.find(base_id);
		assert(it != types.end() && "invalid type ID");
		return it->second;
	}

	DataType TypeRegistry::reserve_type_id()
	{
		std::uint32_t id = nid++;
		if (constexpr std::uint16_t FLAG_MASK = 0xF000;
			(id & FLAG_MASK) != 0)
		{
			throw std::runtime_error("type ID space exhausted");
		}
		return static_cast<DataType>(id);
	}

	void TypeRegistry::complete_type(DataType placeholder, DataType actual)
	{
		DataType actual_base = get_base_type_id(actual);

		auto ph_id = static_cast<std::uint32_t>(placeholder);
		auto actual_id = static_cast<std::uint32_t>(actual_base);

		assert(ph_id >= static_cast<std::uint32_t>(DataType::EXTENDED) && "invalid type ID");
		assert(!types.contains(static_cast<DataType>(ph_id)) && "type already defined");

		types[static_cast<DataType>(ph_id)] = types[static_cast<DataType>(actual_id)];
		type_lookup[types[static_cast<DataType>(ph_id)]] = ph_id;
	}

	size_t TypeRegistry::TypeHasher::operator()(const TypedData &data) const
	{
		size_t hash = std::hash<int>()(static_cast<int>(data.type()));

		/* golden ratio hash type shit */
		switch (data.type())
		{
			case DataType::BOOL:
				hash ^= std::hash<bool>()(data.get<DataType::BOOL>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::INT8:
				hash ^= std::hash<int8_t>()(data.get<DataType::INT8>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::INT16:
				hash ^= std::hash<int16_t>()(data.get<DataType::INT16>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::INT32:
				hash ^= std::hash<int32_t>()(data.get<DataType::INT32>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::INT64:
				hash ^= std::hash<int64_t>()(data.get<DataType::INT64>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::UINT8:
				hash ^= std::hash<uint8_t>()(data.get<DataType::UINT8>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::UINT16:
				hash ^= std::hash<uint16_t>()(data.get<DataType::UINT16>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::UINT32:
				hash ^= std::hash<uint32_t>()(data.get<DataType::UINT32>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;

			case DataType::UINT64:
				hash ^= std::hash<uint64_t>()(data.get<DataType::UINT64>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;
			case DataType::FLOAT32:
				hash ^= std::hash<float>()(data.get<DataType::FLOAT32>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;
			case DataType::FLOAT64:
				hash ^= std::hash<double>()(data.get<DataType::FLOAT64>()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;
			case DataType::POINTER:
			{
				const auto &ptr = data.get<DataType::POINTER>();
				hash ^= std::hash<int>()(static_cast<int>(ptr.pointee_type)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				hash ^= std::hash<uint32_t>()(ptr.addr_space) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;
			}
			case DataType::ARRAY:
			{
				const auto &arr = data.get<DataType::ARRAY>();
				hash ^= std::hash<int>()(static_cast<int>(arr.elem_type)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				hash ^= std::hash<uint64_t>()(arr.count) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;
			}
			case DataType::STRUCT:
			{
				const auto &struct_data = data.get<DataType::STRUCT>();
				hash ^= std::hash<uint32_t>()(struct_data.size) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				hash ^= std::hash<uint32_t>()(struct_data.alignment) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				for (const auto &[field_name, field_type]: struct_data.fields)
				{
					hash ^= std::hash<std::string>()(field_name) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
					hash ^= std::hash<int>()(static_cast<int>(field_type)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				}
				break;
			}
			case DataType::FUNCTION:
			{
				const auto &func = data.get<DataType::FUNCTION>();
				hash ^= std::hash<int>()(static_cast<int>(func.return_type)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				hash ^= std::hash<bool>()(func.is_vararg) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

				/* hash parameter types */
				for (const auto &param_type: func.param_types)
					hash ^= std::hash<int>()(static_cast<int>(param_type)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;
			}

			case DataType::VECTOR:
			{
				const auto &[elem_type, count] = data.get<DataType::VECTOR>();
				hash ^= std::hash<int>()(static_cast<int>(elem_type)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				hash ^= std::hash<uint32_t>()(count) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
				break;
			}

			/* VOID is also noop as well */
			default:
				break;
		}

		return hash;
	}

	bool TypeRegistry::TypeEqual::operator()(const TypedData &a, const TypedData &b) const
	{
		if (a.type() != b.type())
			return false;

		switch (a.type())
		{
			case DataType::VOID:
				return true;
			case DataType::BOOL:
				return a.get<DataType::BOOL>() == b.get<DataType::BOOL>();
			case DataType::INT8:
				return a.get<DataType::INT8>() == b.get<DataType::INT8>();
			case DataType::INT16:
				return a.get<DataType::INT16>() == b.get<DataType::INT16>();
			case DataType::INT32:
				return a.get<DataType::INT32>() == b.get<DataType::INT32>();
			case DataType::INT64:
				return a.get<DataType::INT64>() == b.get<DataType::INT64>();
			case DataType::UINT8:
				return a.get<DataType::UINT8>() == b.get<DataType::UINT8>();
			case DataType::UINT16:
				return a.get<DataType::UINT16>() == b.get<DataType::UINT16>();
			case DataType::UINT32:
				return a.get<DataType::UINT32>() == b.get<DataType::UINT32>();
			case DataType::UINT64:
				return a.get<DataType::UINT64>() == b.get<DataType::UINT64>();
			case DataType::FLOAT32:
				return a.get<DataType::FLOAT32>() == b.get<DataType::FLOAT32>();
			case DataType::FLOAT64:
				return a.get<DataType::FLOAT64>() == b.get<DataType::FLOAT64>();
			case DataType::POINTER:
			{
				const auto &ptr_a = a.get<DataType::POINTER>();
				const auto &ptr_b = b.get<DataType::POINTER>();
				return ptr_a.pointee_type == ptr_b.pointee_type &&
				       ptr_a.addr_space == ptr_b.addr_space;
			}
			case DataType::ARRAY:
			{
				const auto &arr_a = a.get<DataType::ARRAY>();
				const auto &arr_b = b.get<DataType::ARRAY>();
				return arr_a.elem_type == arr_b.elem_type &&
				       arr_a.count == arr_b.count;
			}
			case DataType::STRUCT:
			{
				const auto &struct_a = a.get<DataType::STRUCT>();
				const auto &struct_b = b.get<DataType::STRUCT>();
				return struct_a.size == struct_b.size &&
				       struct_a.alignment == struct_b.alignment &&
				       struct_a.fields == struct_b.fields;
			}
			case DataType::FUNCTION:
			{
				const auto &func_a = a.get<DataType::FUNCTION>();
				const auto &func_b = b.get<DataType::FUNCTION>();
				return func_a.param_types == func_b.param_types &&
				       func_a.return_type == func_b.return_type &&
				       func_a.is_vararg == func_b.is_vararg;
			}
			case DataType::VECTOR:
			{
				const auto &vec_a = a.get<DataType::VECTOR>();
				const auto &vec_b = b.get<DataType::VECTOR>();
				return vec_a.elem_type == vec_b.elem_type &&
				       vec_a.count == vec_b.count;
			}
			default:
				return false;
		}
	}
}
