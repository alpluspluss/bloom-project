/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/region.hpp>
#include <gtest/gtest.h>

class ContextFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		context = std::make_unique<blm::Context>();
	}

	void TearDown() override
	{
		context.reset();
	}

	std::unique_ptr<blm::Context> context;
};

TEST_F(ContextFixture, ModuleCreation)
{
	auto *module = context->create_module("test_module");
	EXPECT_NE(module, nullptr);
	EXPECT_EQ(module->get_name(), "test_module");

	auto *found_module = context->find_module("test_module");
	EXPECT_EQ(found_module, module);

	auto *not_found = context->find_module("nonexistent_module");
	EXPECT_EQ(not_found, nullptr);
}

TEST_F(ContextFixture, StringInterning)
{
	const auto id1 = context->intern_string("test");
	const auto id2 = context->intern_string("test");
	const auto id3 = context->intern_string("different");

	EXPECT_EQ(id1, id2);
	EXPECT_NE(id1, id3);

	EXPECT_EQ(context->get_string(id1), "test");
	EXPECT_EQ(context->get_string(id3), "different");
}

TEST_F(ContextFixture, NodeCreation)
{
	auto *node = context->create<blm::Node>();
	EXPECT_NE(node, nullptr);

	node->ir_type = blm::NodeType::LIT;
	node->type_kind = blm::DataType::INT32;

	EXPECT_EQ(node->ir_type, blm::NodeType::LIT);
	EXPECT_EQ(node->type_kind, blm::DataType::INT32);
}

TEST_F(ContextFixture, TypeRegistryBasics)
{
	blm::TypedData int32_data;
	int32_data.set<std::int32_t, blm::DataType::INT32>(0);
	const auto int32_type = context->register_type(std::move(int32_data));

	blm::TypedData float64_data;
	float64_data.set<double, blm::DataType::FLOAT64>(0.0);
	const auto float64_type = context->register_type(std::move(float64_data));

	const auto &int32_type_data = context->get_type(int32_type);
	EXPECT_EQ(int32_type_data.type(), blm::DataType::INT32);

	const auto &float64_type_data = context->get_type(float64_type);
	EXPECT_EQ(float64_type_data.type(), blm::DataType::FLOAT64);
}

TEST_F(ContextFixture, ComplexTypes)
{
	blm::TypedData int32_data;
	int32_data.set<std::int32_t, blm::DataType::INT32>(0);
	auto int32_type = context->register_type(std::move(int32_data));

	auto ptr_int32 = context->create_pointer_type(int32_type);
	EXPECT_NE(ptr_int32, int32_type);

	const auto &ptr_data = context->get_type(ptr_int32);
	EXPECT_EQ(ptr_data.type(), blm::DataType::POINTER);

	const auto &ptr_info = ptr_data.get<blm::DataType::POINTER>();
	EXPECT_EQ(ptr_info.pointee_type, int32_type);
	EXPECT_EQ(ptr_info.addr_space, 0u);

	auto array_type = context->create_array_type(int32_type, 10);

	const auto &array_data = context->get_type(array_type);
	EXPECT_EQ(array_data.type(), blm::DataType::ARRAY);

	const auto &array_info = array_data.get<blm::DataType::ARRAY>();
	EXPECT_EQ(array_info.elem_type, int32_type);
	EXPECT_EQ(array_info.count, 10u);

	std::vector<std::pair<std::string, blm::DataType> > fields = {
		{ "x", int32_type },
		{ "y", ptr_int32 }
	};

	auto struct_type = context->create_struct_type(fields, 16, 8);

	const auto &struct_data = context->get_type(struct_type);
	EXPECT_EQ(struct_data.type(), blm::DataType::STRUCT);

	const auto &struct_info = struct_data.get<blm::DataType::STRUCT>();
	EXPECT_EQ(struct_info.size, 16u);
	EXPECT_EQ(struct_info.alignment, 8u);
	EXPECT_EQ(struct_info.fields.size(), 2u);
	EXPECT_EQ(struct_info.fields[0].first, "x");
	EXPECT_EQ(struct_info.fields[0].second, int32_type);
	EXPECT_EQ(struct_info.fields[1].first, "y");
	EXPECT_EQ(struct_info.fields[1].second, ptr_int32);

	std::vector<blm::DataType> param_types = { int32_type, ptr_int32 };
	auto func_type = context->create_function_type(int32_type, param_types);

	const auto &func_data = context->get_type(func_type);
	EXPECT_EQ(func_data.type(), blm::DataType::FUNCTION);

	const auto &func_info = func_data.get<blm::DataType::FUNCTION>();
	EXPECT_EQ(func_info.return_type, int32_type);
	EXPECT_EQ(func_info.param_types.size(), 2u);
	EXPECT_EQ(func_info.param_types[0], int32_type);
	EXPECT_EQ(func_info.param_types[1], ptr_int32);
	EXPECT_FALSE(func_info.is_vararg);
}

TEST_F(ContextFixture, IntegratedModuleAndTypes)
{
	const auto *module = context->create_module("test_module");

	blm::TypedData int32_data;
	int32_data.set<std::int32_t, blm::DataType::INT32>(0);
	const auto int32_type = context->register_type(std::move(int32_data));

	auto *node = module->get_root_region()->create_node<blm::Node>();
	node->ir_type = blm::NodeType::LIT;
	node->type_kind = int32_type;

	EXPECT_EQ(node->type_kind, int32_type);

	const auto &type_data = context->get_type(node->type_kind);
	EXPECT_EQ(type_data.type(), blm::DataType::INT32);
}
