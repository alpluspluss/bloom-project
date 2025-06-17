/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/dbinfo.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <gtest/gtest.h>

class DebugInfoFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        context = std::make_unique<blm::Context>();
        module = std::make_unique<blm::Module>(*context, "test_module");
        region = module->create_region("test_region");
        debug_info = std::make_unique<blm::DebugInfo>(*region);
    }

    void TearDown() override
    {
        debug_info.reset();
        module.reset();
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    std::unique_ptr<blm::Module> module;
    blm::Region* region = nullptr;
    std::unique_ptr<blm::DebugInfo> debug_info;
};

TEST_F(DebugInfoFixture, AddSourceFile)
{
    const auto file_id = debug_info->add_source_file("test.cpp");
    EXPECT_NE(file_id, 0);

    const auto path = debug_info->get_source_file(file_id);
    EXPECT_EQ(path, "test.cpp");
}

TEST_F(DebugInfoFixture, LocationTracking)
{
    const auto file_id = debug_info->add_source_file("test.cpp");

    auto* node = region->create_node<blm::Node>();

    debug_info->set_node_location(node, file_id, 42, 10);

    const auto loc_opt = debug_info->get_node_location(node);
    ASSERT_TRUE(loc_opt.has_value());

    auto& loc = loc_opt.value();
    EXPECT_EQ(loc.file_id, file_id);
    EXPECT_EQ(loc.line, 42);
    EXPECT_EQ(loc.column, 10);

    auto nodes = debug_info->find_nodes_at_location(file_id, 42, 10);
    ASSERT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes[0], node);

    nodes = debug_info->find_nodes_at_location(file_id, 1, 1);
    EXPECT_TRUE(nodes.empty());
}

TEST_F(DebugInfoFixture, VariableInfo)
{
    auto* var_node = region->create_node<blm::Node>();
    var_node->ir_type = blm::NodeType::PARAM;

    debug_info->add_variable(var_node, "count", "int32", true, -8);

    auto [name, type, is_param, offset] = debug_info->get_variable_info(var_node);

    EXPECT_EQ(name, "count");
    EXPECT_EQ(type, "int32");
    EXPECT_TRUE(is_param);
    EXPECT_EQ(offset, -8);

    auto* non_existent = reinterpret_cast<blm::Node*>(0x123);
    auto [ne_name, ne_type, ne_is_param, ne_offset] = debug_info->get_variable_info(non_existent);

    EXPECT_EQ(ne_name, "");
    EXPECT_EQ(ne_type, "");
    EXPECT_FALSE(ne_is_param);
    EXPECT_EQ(ne_offset, 0);
}

TEST_F(DebugInfoFixture, FunctionInfo)
{
    auto* func_node = region->create_node<blm::Node>();
    func_node->ir_type = blm::NodeType::FUNCTION;

    auto* param1 = region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;

    auto* param2 = region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;

    auto* local_var = region->create_node<blm::Node>();
    local_var->ir_type = blm::NodeType::LIT;

    debug_info->add_function(func_node, "test_func");
    debug_info->add_parameter_to_function(func_node, param1);
    debug_info->add_parameter_to_function(func_node, param2);
    debug_info->add_local_var_to_function(func_node, local_var);

    const auto params = debug_info->get_function_parameters(func_node);
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], param1);
    EXPECT_EQ(params[1], param2);

    const auto locals = debug_info->get_function_local_vars(func_node);
    ASSERT_EQ(locals.size(), 1);
    EXPECT_EQ(locals[0], local_var);

    auto* non_existent = reinterpret_cast<blm::Node*>(0x123);
    auto ne_params = debug_info->get_function_parameters(non_existent);
    EXPECT_TRUE(ne_params.empty());

    auto ne_locals = debug_info->get_function_local_vars(non_existent);
    EXPECT_TRUE(ne_locals.empty());
}

TEST_F(DebugInfoFixture, TypeInfo)
{
    auto type_id = debug_info->add_type("Vector3", 12, 4);

    auto [name, size, alignment] = debug_info->get_type_info(type_id);

    EXPECT_EQ(name, "Vector3");
    EXPECT_EQ(size, 12);
    EXPECT_EQ(alignment, 4);

    auto invalid_id = static_cast<blm::StringTable::StringId>(9999);
    auto [ne_name, ne_size, ne_alignment] = debug_info->get_type_info(invalid_id);

    EXPECT_EQ(ne_size, 0);
    EXPECT_EQ(ne_alignment, 0);
}

TEST_F(DebugInfoFixture, RegionAccessor)
{
    blm::Region& accessed_region = debug_info->get_region();
    EXPECT_EQ(&accessed_region, region);
}

TEST_F(DebugInfoFixture, NullHandling)
{
    debug_info->set_node_location(nullptr, 0, 0, 0);
    auto loc = debug_info->get_node_location(nullptr);
    EXPECT_FALSE(loc.has_value());

    debug_info->add_variable(nullptr, "", "", false, 0);
    debug_info->add_function(nullptr, "");
    debug_info->add_parameter_to_function(nullptr, nullptr);
    debug_info->add_local_var_to_function(nullptr, nullptr);

    auto params = debug_info->get_function_parameters(nullptr);
    EXPECT_TRUE(params.empty());

    auto locals = debug_info->get_function_local_vars(nullptr);
    EXPECT_TRUE(locals.empty());
}

TEST_F(DebugInfoFixture, SourceLocationDefaultConstructor)
{
    constexpr blm::SourceLocation loc = {};
    EXPECT_EQ(loc.file_id, 0);
    EXPECT_EQ(loc.line, 0);
    EXPECT_EQ(loc.column, 0);
}

TEST_F(DebugInfoFixture, SourceLocationComparison)
{
    const auto file_id = debug_info->add_source_file("test.cpp");

    blm::SourceLocation loc1(file_id, 10, 5);
    blm::SourceLocation loc2(file_id, 10, 5);
    blm::SourceLocation loc3(file_id, 10, 6);
    blm::SourceLocation loc4(file_id, 11, 5);

    EXPECT_TRUE(loc1 == loc2);
    EXPECT_FALSE(loc1 == loc3);
    EXPECT_FALSE(loc1 == loc4);

    EXPECT_TRUE(loc1 <= loc2);
    EXPECT_TRUE(loc1 < loc3);
    EXPECT_TRUE(loc1 < loc4);
    EXPECT_TRUE(loc3 < loc4);
}
