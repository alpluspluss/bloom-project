/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/typed-data.hpp>
#include <bloom/foundation/types.hpp>

namespace blm
{
	TypedData::TypedData() = default;

	TypedData::~TypedData()
	{
		destroy();
	}

	TypedData::TypedData(const TypedData &other)
	{
		copy_from(other);
	}

	TypedData::TypedData(TypedData &&other) noexcept
	{
		move_from(std::move(other));
	}

	TypedData &TypedData::operator=(const TypedData &other)
	{
		if (this != &other)
			copy_from(other);
		return *this;
	}

	TypedData &TypedData::operator=(TypedData &&other) noexcept
	{
		if (this != &other)
			move_from(std::move(other));
		return *this;
	}

	DataType TypedData::type() const
	{
		return current_type;
	}

	void TypedData::destroy()
	{
		switch (current_type)
		{
#define BIR_TYPEDATA_DESTROY(dt) \
	case DataType::dt: \
		std::destroy_at(reinterpret_cast<DataTypeTraits<DataType::dt>::type*>(&storage)); \
		break;

			BIR_TYPEDATA_DESTROY(BOOL)
			BIR_TYPEDATA_DESTROY(INT8)
			BIR_TYPEDATA_DESTROY(INT16)
			BIR_TYPEDATA_DESTROY(INT32)
			BIR_TYPEDATA_DESTROY(INT64)
			BIR_TYPEDATA_DESTROY(UINT8)
			BIR_TYPEDATA_DESTROY(UINT16)
			BIR_TYPEDATA_DESTROY(UINT32)
			BIR_TYPEDATA_DESTROY(UINT64)
			BIR_TYPEDATA_DESTROY(FLOAT32)
			BIR_TYPEDATA_DESTROY(FLOAT64)
			BIR_TYPEDATA_DESTROY(POINTER)
			BIR_TYPEDATA_DESTROY(ARRAY)
			BIR_TYPEDATA_DESTROY(STRUCT)
			BIR_TYPEDATA_DESTROY(FUNCTION)
#undef BIR_TYPEDATA_DESTROY

			default:
				break;
		}
		current_type = DataType::VOID;
	}

	void TypedData::copy_from(const TypedData &other)
	{
		destroy();
		current_type = other.current_type;
		construct_from_storage(reinterpret_cast<const unsigned char *>(&other.storage), false);
	}

	void TypedData::move_from(TypedData &&other)
	{
		destroy();
		current_type = other.current_type;
		construct_from_storage(reinterpret_cast<unsigned char *>(&other.storage), true);
		other.current_type = DataType::VOID;
	}

	void TypedData::construct_from_storage(const unsigned char *other_storage, bool move)
	{
#define BIR_TYPEDATA_CONSTRUCT(dt) \
	case DataType::dt: \
		if (move) \
			new (&storage) typename DataTypeTraits<DataType::dt>::type( \
				std::move(*std::launder(reinterpret_cast<typename DataTypeTraits<DataType::dt>::type*>(const_cast<unsigned char*>(other_storage))))); \
		else \
			new (&storage) typename DataTypeTraits<DataType::dt>::type( \
				*std::launder(reinterpret_cast<const typename DataTypeTraits<DataType::dt>::type*>(other_storage))); \
		break;

		switch (current_type)
		{
			BIR_TYPEDATA_CONSTRUCT(BOOL)
			BIR_TYPEDATA_CONSTRUCT(INT8)
			BIR_TYPEDATA_CONSTRUCT(INT16)
			BIR_TYPEDATA_CONSTRUCT(INT32)
			BIR_TYPEDATA_CONSTRUCT(INT64)
			BIR_TYPEDATA_CONSTRUCT(UINT8)
			BIR_TYPEDATA_CONSTRUCT(UINT16)
			BIR_TYPEDATA_CONSTRUCT(UINT32)
			BIR_TYPEDATA_CONSTRUCT(UINT64)
			BIR_TYPEDATA_CONSTRUCT(FLOAT32)
			BIR_TYPEDATA_CONSTRUCT(FLOAT64)
			BIR_TYPEDATA_CONSTRUCT(POINTER)
			BIR_TYPEDATA_CONSTRUCT(ARRAY)
			BIR_TYPEDATA_CONSTRUCT(STRUCT)
			BIR_TYPEDATA_CONSTRUCT(FUNCTION)
			default:
				break;
		}

#undef BIR_TYPEDATA_CONSTRUCT
	}
}
