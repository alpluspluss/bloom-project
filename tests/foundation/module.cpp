/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <gtest/gtest.h>

class ModuleFixture : public ::testing::Test
{
protected:
    void SetUp() override {
        context = std::make_unique<blm::Context>();
        module = std::make_unique<blm::Module>(*context, "test_module");
    }

    void TearDown() override {
        module.reset();
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    std::unique_ptr<blm::Module> module;
};

TEST_F(ModuleFixture, BasicProperties)
{
    EXPECT_EQ(module->get_name(), "test_module");
    EXPECT_NE(module->get_root_region(), nullptr);
}

TEST_F(ModuleFixture, RegionCreation)
{
    auto* region1 = module->create_region("region1");
    auto* region2 = module->create_region("region2", region1);

    EXPECT_NE(region1, nullptr);
    EXPECT_NE(region2, nullptr);
    EXPECT_EQ(region1->get_name(), "region1");
    EXPECT_EQ(region2->get_name(), "region2");
    EXPECT_EQ(region2->get_parent(), region1);

    const auto& children = region1->get_children();
    EXPECT_EQ(children.size(), 1);
    EXPECT_EQ(children[0], region2);
}

TEST_F(ModuleFixture, RegionHierarchy)
{
    auto* region1 = module->create_region("region1");
    auto* region2 = module->create_region("region2", region1);
    auto* region3 = module->create_region("region3", region1);
    auto* region4 = module->create_region("region4", region2);

    EXPECT_EQ(region1->get_parent(), module->get_root_region());
    EXPECT_EQ(region2->get_parent(), region1);
    EXPECT_EQ(region3->get_parent(), region1);
    EXPECT_EQ(region4->get_parent(), region2);

    EXPECT_EQ(region1->get_children().size(), 2);
    EXPECT_EQ(region2->get_children().size(), 1);
    EXPECT_EQ(region3->get_children().size(), 0);
    EXPECT_EQ(region4->get_children().size(), 0);
}

TEST_F(ModuleFixture, FunctionManagement)
{
    auto* node = module->get_root_region()->create_node<blm::Node>();
    node->ir_type = blm::NodeType::FUNCTION;
    node->str_id = context->intern_string("test_function");

    module->add_function(node);

    EXPECT_EQ(module->get_functions().size(), 1);
    EXPECT_EQ(module->get_functions()[0], node);

    auto* found = module->find_function("test_function");
    EXPECT_EQ(found, node);

    auto* not_found = module->find_function("nonexistent_function");
    EXPECT_EQ(not_found, nullptr);
}

TEST_F(ModuleFixture, MultipleFunctions)
{
    auto* func1 = module->get_root_region()->create_node<blm::Node>();
    func1->ir_type = blm::NodeType::FUNCTION;
    func1->str_id = context->intern_string("func1");

    auto* func2 = module->get_root_region()->create_node<blm::Node>();
    func2->ir_type = blm::NodeType::FUNCTION;
    func2->str_id = context->intern_string("func2");

    module->add_function(func1);
    module->add_function(func2);

    EXPECT_EQ(module->get_functions().size(), 2);
    EXPECT_EQ(module->find_function("func1"), func1);
    EXPECT_EQ(module->find_function("func2"), func2);
}

TEST_F(ModuleFixture, InvalidFunctionNodes)
{
    auto* not_a_func = module->get_root_region()->create_node<blm::Node>();
    not_a_func->ir_type = blm::NodeType::ADD; /* not a function */

    module->add_function(not_a_func);
    EXPECT_EQ(module->get_functions().size(), 0); /* noop */

    module->add_function(nullptr);
    EXPECT_EQ(module->get_functions().size(), 0); /* noop */
}
