/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/type-registry.hpp>
#include <gtest/gtest.h>

class TypeRegistryFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		type_registry = std::make_unique<blm::TypeRegistry>();

		/* non-explictly `.set<T, U>() means a void type */
		blm::TypedData void_data;
		void_type = type_registry->register_type(std::move(void_data));

		blm::TypedData int32_data;
		int32_data.set<std::int32_t, blm::DataType::INT32>(0);
		int32_type = type_registry->register_type(std::move(int32_data));

		blm::TypedData float64_data;
		float64_data.set<double, blm::DataType::FLOAT64>(0.0);
		float64_type = type_registry->register_type(std::move(float64_data));

		blm::TypedData float32_data;
		float32_data.set<float, blm::DataType::FLOAT32>(0.0f);
		float32_type = type_registry->register_type(std::move(float32_data));

		blm::TypedData bool_data;
		bool_data.set<bool, blm::DataType::BOOL>(false);
		bool_type = type_registry->register_type(std::move(bool_data));
	}

	void TearDown() override
	{
		type_registry.reset();
	}

	std::unique_ptr<blm::TypeRegistry> type_registry;
	blm::DataType void_type = {};
	blm::DataType int32_type = {};
	blm::DataType float64_type = {};
	blm::DataType float32_type = {};
	blm::DataType bool_type = {};
};

TEST_F(TypeRegistryFixture, BasicTypes)
{
	const auto &void_type_data = type_registry->get_type(void_type);
	EXPECT_EQ(void_type_data.type(), blm::DataType::VOID);

	const auto &int32_type_data = type_registry->get_type(int32_type);
	EXPECT_EQ(int32_type_data.type(), blm::DataType::INT32);

	const auto &float64_type_data = type_registry->get_type(float64_type);
	EXPECT_EQ(float64_type_data.type(), blm::DataType::FLOAT64);
}

TEST_F(TypeRegistryFixture, PointerTypes)
{
	auto ptr_int32 = type_registry->create_pointer_type(int32_type);
	EXPECT_NE(ptr_int32, int32_type);

	const auto &ptr_data = type_registry->get_type(ptr_int32);
	EXPECT_EQ(ptr_data.type(), blm::DataType::POINTER);

	const auto &ptr_info = ptr_data.get<blm::DataType::POINTER>();
	EXPECT_EQ(ptr_info.pointee_type, int32_type);
	EXPECT_EQ(ptr_info.addr_space, 0u);

	auto ptr_int32_again = type_registry->create_pointer_type(int32_type);
	EXPECT_EQ(ptr_int32, ptr_int32_again);

	auto ptr_int32_as1 = type_registry->create_pointer_type(int32_type, 1);
	EXPECT_NE(ptr_int32, ptr_int32_as1);
}

TEST_F(TypeRegistryFixture, ArrayTypes)
{
	auto array_int32 = type_registry->create_array_type(int32_type, 10);
	EXPECT_NE(array_int32, int32_type);

	const auto &array_data = type_registry->get_type(array_int32);
	EXPECT_EQ(array_data.type(), blm::DataType::ARRAY);

	const auto &array_info = array_data.get<blm::DataType::ARRAY>();
	EXPECT_EQ(array_info.elem_type, int32_type);
	EXPECT_EQ(array_info.count, 10u);

	auto array_int32_again = type_registry->create_array_type(int32_type, 10);
	EXPECT_EQ(array_int32, array_int32_again);

	auto array_int32_20 = type_registry->create_array_type(int32_type, 20);
	EXPECT_NE(array_int32, array_int32_20);
}

TEST_F(TypeRegistryFixture, StructTypes)
{
	std::vector<std::pair<std::string, blm::DataType> > fields = {
		{ "x", int32_type },
		{ "y", float64_type }
	};

	auto struct_type = type_registry->create_struct_type(fields, 16, 8);

	const auto &struct_data = type_registry->get_type(struct_type);
	EXPECT_EQ(struct_data.type(), blm::DataType::STRUCT);

	const auto &struct_info = struct_data.get<blm::DataType::STRUCT>();
	EXPECT_EQ(struct_info.size, 16u);
	EXPECT_EQ(struct_info.alignment, 8u);
	EXPECT_EQ(struct_info.fields.size(), 2u);
	EXPECT_EQ(struct_info.fields[0].first, "x");
	EXPECT_EQ(struct_info.fields[0].second, int32_type);
	EXPECT_EQ(struct_info.fields[1].first, "y");
	EXPECT_EQ(struct_info.fields[1].second, float64_type);

	auto struct_type_again = type_registry->create_struct_type(fields, 16, 8);
	EXPECT_EQ(struct_type, struct_type_again);

	auto struct_type_diff_align = type_registry->create_struct_type(fields, 16, 4);
	EXPECT_NE(struct_type, struct_type_diff_align);
}

TEST_F(TypeRegistryFixture, FunctionTypes)
{
	/* create a function type: i32 (f32, bool) */
	const std::vector param_types = {
		float32_type,
		bool_type
	};

	auto func_type = type_registry->create_function_type(int32_type, param_types);

	const auto &func_data = type_registry->get_type(func_type);
	EXPECT_EQ(func_data.type(), blm::DataType::FUNCTION);

	const auto &func_info = func_data.get<blm::DataType::FUNCTION>();
	EXPECT_EQ(func_info.return_type, int32_type);
	EXPECT_EQ(func_info.param_types.size(), 2u);
	EXPECT_EQ(func_info.param_types[0], float32_type);
	EXPECT_EQ(func_info.param_types[1], bool_type);
	EXPECT_FALSE(func_info.is_vararg);

	auto func_type_again = type_registry->create_function_type(int32_type, param_types);
	EXPECT_EQ(func_type, func_type_again);

	auto vararg_func_type = type_registry->create_function_type(int32_type, param_types, true);
	EXPECT_NE(func_type, vararg_func_type);
}

TEST_F(TypeRegistryFixture, ComplexTypes)
{
	auto ptr_int32 = type_registry->create_pointer_type(int32_type);
	auto array_ptr_int32 = type_registry->create_array_type(ptr_int32, 5);

	const auto &array_data = type_registry->get_type(array_ptr_int32);
	const auto &array_info = array_data.get<blm::DataType::ARRAY>();
	EXPECT_EQ(array_info.elem_type, ptr_int32);
	EXPECT_EQ(array_info.count, 5u);

	std::vector<std::pair<std::string, blm::DataType> > fields = {
		{ "data", array_ptr_int32 }
	};

	auto struct_type = type_registry->create_struct_type(fields, 40, 8);

	const auto &struct_data = type_registry->get_type(struct_type);
	const auto &struct_info = struct_data.get<blm::DataType::STRUCT>();
	EXPECT_EQ(struct_info.fields[0].second, array_ptr_int32);
}

TEST_F(TypeRegistryFixture, ForwardDeclarations)
{
	auto placeholder = type_registry->reserve_type_id();

	/*
	 * struct placeholder
	 * {
	 *     placeholder *next;
	 * };
	 */
	std::vector<std::pair<std::string, blm::DataType> > fields = {
		{ "next", placeholder }
	};

	auto struct_type = type_registry->create_struct_type(fields, 8, 8);

	type_registry->complete_type(placeholder, struct_type);

	const auto &placeholder_data = type_registry->get_type(placeholder);
	EXPECT_EQ(placeholder_data.type(), blm::DataType::STRUCT);

	const auto &struct_info = placeholder_data.get<blm::DataType::STRUCT>();
	EXPECT_EQ(struct_info.fields[0].first, "next");
	EXPECT_EQ(struct_info.fields[0].second, placeholder);
}
