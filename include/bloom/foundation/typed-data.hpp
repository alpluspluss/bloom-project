/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <string>
#include <variant>
#include <vector>
#include <bloom/foundation/types.hpp>

namespace blm
{
	/**
	 * @brief Traits class to map DataType enum values to their corresponding C++ types
	 */
	template<DataType /* T */>
	struct DataTypeTraits {};

	/**
	 * @brief Specialization for `DataType::VOID`
	 */
	template<>
	struct DataTypeTraits<DataType::VOID>
	{
		struct type
		{
			bool operator==(const type&) const
			{
				return true;
			}
		};
	};

	/**
	 * @brief Specialization for `DataType::BOOL`
	 */
	template<>
	struct DataTypeTraits<DataType::BOOL>
	{
		using type = bool;
	};

	/**
	 * @brief Specialization for `DataType::INT8`
	 */
	template<>
	struct DataTypeTraits<DataType::INT8>
	{
		using type = std::int8_t;
	};

	/**
	 * @brief Specialization for `DataType::INT16`
	 */
	template<>
	struct DataTypeTraits<DataType::INT16>
	{
		using type = std::int16_t;
	};

	/**
	 * @brief Specialization for `DataType::INT32`
	 */
	template<>
	struct DataTypeTraits<DataType::INT32>
	{
		using type = std::int32_t;
	};

	/**
	 * @brief Specialization for `DataType::INT64`
	 */
	template<>
	struct DataTypeTraits<DataType::INT64>
	{
		using type = std::int64_t;
	};

	/**
	 * @brief Specialization for `DataType::UINT8`
	 */
	template<>
	struct DataTypeTraits<DataType::UINT8>
	{
		using type = std::uint8_t;
	};

	/**
	 * @brief Specialization for `DataType::UINT16`
	 */
	template<>
	struct DataTypeTraits<DataType::UINT16>
	{
		using type = std::uint16_t;
	};

	/**
	 * @brief Specialization for `DataType::UINT32`
	 */
	template<>
	struct DataTypeTraits<DataType::UINT32>
	{
		using type = std::uint32_t;
	};

	/**
	 * @brief Specialization for `DataType::UINT64`
	 */
	template<>
	struct DataTypeTraits<DataType::UINT64>
	{
		using type = std::uint64_t;
	};

	/**
	 * @brief Specialization for `DataType::FLOAT32`
	 */
	template<>
	struct DataTypeTraits<DataType::FLOAT32>
	{
		using type = float;
	};

	/**
	 * @brief Specialization for `DataType::FLOAT64`
	 */
	template<>
	struct DataTypeTraits<DataType::FLOAT64>
	{
		using type = double;
	};

	/**
	 * @brief Specialization for `DataType::POINTER`
	 */
	template<>
	struct DataTypeTraits<DataType::POINTER>
	{
		struct type
		{
			/** @brief The type of the data being pointed to */
			DataType pointee_type;
			/** @brief The address space of the pointer */
			std::uint32_t addr_space;
		};
	};

	/**
	 * @brief Specialization for `DataType::ARRAY`
	 */
	template<>
	struct DataTypeTraits<DataType::ARRAY>
	{
		struct type
		{
			/** @brief the type of the elements in the array */
			DataType elem_type;
			/** @brief the number of elements in the array */
			std::uint64_t count;
		};
	};

	/**
	 * @brief Specialization for `DataType::STRUCT`
	 */
	template<>
	struct DataTypeTraits<DataType::STRUCT>
	{
		struct type
		{
			/** @brief the size of the struct in bytes */
			std::uint32_t size;
			/** @brief the alignment of the struct in bytes */
			std::uint32_t alignment;
			/** @brief the fields of the struct, each field is a pair of name and type */
			std::vector<std::pair<std::string, DataType>> fields;
		};
	};

	/**
	 * @brief Specialization for `DataType::FUNCTION`
	 */
	template<>
	struct DataTypeTraits<DataType::FUNCTION>
	{
		struct type
		{
			/** @brief The types of the parameters of the function */
			std::vector<DataType> param_types;
			/** @brief The return type of the function */
			DataType return_type;
			/** @brief Whether the function is a vararg function */
			bool is_vararg; /* note: this maybe changed to a bitmask of flags in the future */
		};
	};

	/**
	 * @brief Specialization for `DataType::VECTOR`
	 */
	template<>
	struct DataTypeTraits<DataType::VECTOR>
	{
		struct type
		{
			/** @brief The element type of the vector */
			DataType elem_type;
			/** @brief The number of elements in the vector */
			std::uint32_t count;

			bool operator==(const type& other) const
			{
				return elem_type == other.elem_type && count == other.count;
			}
		};
	};

	template<>
	struct DataTypeTraits<DataType::STRING>
	{
		using type = std::string;
	};

	class TypedData
	{
	public:
		/**
		 * @brief Constructs a TypedData object with the type default to DataType::VOID
		 */
		TypedData();

		/**
		 * @brief Destructor for TypedData
		 */
		~TypedData();

		TypedData(const TypedData &other);

		TypedData(TypedData &&other) noexcept;

		TypedData &operator=(const TypedData &other);

		TypedData &operator=(TypedData &&other) noexcept;

		/**
		 * @brief Get the member `current_type`
		 */
		[[nodiscard]] DataType type() const;

		/**
		 * @brief Check if the type currently holding is the same as `T`
		 * @tparam T Type to check against
		 * @return
		 */
		template<typename T>
		[[nodiscard]] bool is_type() const
		{
			switch (current_type)
			{
#define BIR_TYPEDATA_CHECK(dt) \
case DataType::dt: return std::is_same_v<T, typename DataTypeTraits<DataType::dt>::type>;

				BIR_TYPEDATA_CHECK(BOOL)
				BIR_TYPEDATA_CHECK(INT8)
				BIR_TYPEDATA_CHECK(INT16)
				BIR_TYPEDATA_CHECK(INT32)
				BIR_TYPEDATA_CHECK(INT64)
				BIR_TYPEDATA_CHECK(UINT8)
				BIR_TYPEDATA_CHECK(UINT16)
				BIR_TYPEDATA_CHECK(UINT32)
				BIR_TYPEDATA_CHECK(UINT64)
				BIR_TYPEDATA_CHECK(FLOAT32)
				BIR_TYPEDATA_CHECK(FLOAT64)
				BIR_TYPEDATA_CHECK(POINTER)
				BIR_TYPEDATA_CHECK(ARRAY)
				BIR_TYPEDATA_CHECK(STRUCT)
				BIR_TYPEDATA_CHECK(FUNCTION)
				BIR_TYPEDATA_CHECK(VECTOR)
				BIR_TYPEDATA_CHECK(STRING)
	#undef BIR_TYPEDATA_CHECK

				default: return false;
			}
		}

		/**
		 * @brief Get the value of the current type
		 * @tparam T Type to get
		 * @return Reference to the value of the current type
		 */
		template<DataType T>
		typename DataTypeTraits<T>::type &get()
		{
			if (current_type != T)
				throw std::bad_variant_access();
			return *std::launder(reinterpret_cast<typename DataTypeTraits<T>::type*>(&storage));
		}

		/**
		 * @brief Get the value of the current type
		 * @tparam T Type to get
		 * @return Const reference to the value of the current type
		 */
		template<DataType T>
		const typename DataTypeTraits<T>::type &get() const
		{
			if (current_type != T)
				throw std::bad_variant_access();
			return *std::launder(reinterpret_cast<const typename DataTypeTraits<T>::type*>(&storage));
		}

		/**
		 * @brief Set the type to `U` and the value to `value`
		 * @tparam T C++ equivalent type of the value
		 * @tparam U `DataType` to set
		 * @param value Value to set
		 */
		template<typename T, DataType U>
			requires(std::is_same_v<std::decay_t<T>, typename DataTypeTraits<U>::type>)
		void set(T &&value)
		{
			destroy();
			current_type = U;
			new (&storage) typename DataTypeTraits<U>::type(std::forward<T>(value));
		}

		/**
		 * @brief Set the type to `U` and the value to `value` (const lvalue version)
		 * @tparam T C++ equivalent type of the value
		 * @tparam U `DataType` to set
		 * @param value Value to set
		 */
		template<typename T, DataType U>
			requires(std::is_same_v<std::decay_t<T>, typename DataTypeTraits<U>::type>)
		void set(const T &value)
		{
			destroy();
			current_type = U;
			new (&storage) typename DataTypeTraits<U>::type(value);
		}

	private:
		static constexpr std::size_t MaxSize = []
		{
			std::size_t max = 0;
#define BIR_TYPEDATA_TYPE(dt) \
				max = std::max(max, sizeof(typename DataTypeTraits<DataType::dt>::type));

			BIR_TYPEDATA_TYPE(BOOL)
			BIR_TYPEDATA_TYPE(INT8)
			BIR_TYPEDATA_TYPE(INT16)
			BIR_TYPEDATA_TYPE(INT32)
			BIR_TYPEDATA_TYPE(INT64)
			BIR_TYPEDATA_TYPE(UINT8)
			BIR_TYPEDATA_TYPE(UINT16)
			BIR_TYPEDATA_TYPE(UINT32)
			BIR_TYPEDATA_TYPE(UINT64)
			BIR_TYPEDATA_TYPE(FLOAT32)
			BIR_TYPEDATA_TYPE(FLOAT64)
			BIR_TYPEDATA_TYPE(POINTER)
			BIR_TYPEDATA_TYPE(ARRAY)
			BIR_TYPEDATA_TYPE(STRUCT)
			BIR_TYPEDATA_TYPE(FUNCTION)
			BIR_TYPEDATA_TYPE(VECTOR)
			BIR_TYPEDATA_TYPE(STRING)
#undef BIR_TYPEDATA_TYPE
			return max;
		}();

		static constexpr std::size_t MaxAlign = []
		{
			std::size_t max = 1;
#define BIR_TYPEDATA_ALIGN(dt) \
				max = std::max(max, alignof(typename DataTypeTraits<DataType::dt>::type));
			BIR_TYPEDATA_ALIGN(BOOL)
			BIR_TYPEDATA_ALIGN(INT8)
			BIR_TYPEDATA_ALIGN(INT16)
			BIR_TYPEDATA_ALIGN(INT32)
			BIR_TYPEDATA_ALIGN(INT64)
			BIR_TYPEDATA_ALIGN(UINT8)
			BIR_TYPEDATA_ALIGN(UINT16)
			BIR_TYPEDATA_ALIGN(UINT32)
			BIR_TYPEDATA_ALIGN(UINT64)
			BIR_TYPEDATA_ALIGN(FLOAT32)
			BIR_TYPEDATA_ALIGN(FLOAT64)
			BIR_TYPEDATA_ALIGN(POINTER)
			BIR_TYPEDATA_ALIGN(ARRAY)
			BIR_TYPEDATA_ALIGN(STRUCT)
			BIR_TYPEDATA_ALIGN(FUNCTION)
			BIR_TYPEDATA_ALIGN(VECTOR)
			BIR_TYPEDATA_ALIGN(STRING)

#undef BIR_TYPEDATA_ALIGN
			return max;
		}();

		using Storage = std::aligned_storage_t<MaxSize, MaxAlign>;
		Storage storage = {};
		DataType current_type = DataType::VOID;

		void destroy();

		void copy_from(const TypedData &other);

		void move_from(TypedData &&other);

		void construct_from_storage(const unsigned char *other_storage, bool move);
	};

	/**
 * @brief Specialized get implementation for VOID
 */
	template<>
	inline const DataTypeTraits<DataType::VOID>::type& TypedData::get<DataType::VOID>() const
	{
		if (current_type != DataType::VOID)
			throw std::bad_variant_access();

		static constexpr DataTypeTraits<DataType::VOID>::type void_value = {};
		return void_value;
	}

	template<>
	inline DataTypeTraits<DataType::VOID>::type& TypedData::get<DataType::VOID>()
	{
		if (current_type != DataType::VOID)
			throw std::bad_variant_access();

		static DataTypeTraits<DataType::VOID>::type void_value = {};
		return void_value;
	}
}
