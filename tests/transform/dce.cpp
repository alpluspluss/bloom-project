/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/transform/dce.hpp>
#include <gtest/gtest.h>

class DCEPassFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        context = std::make_unique<blm::Context>();
        module = std::make_unique<blm::Module>(*context, "test_module");
    }

    void TearDown() override
    {
        module.reset();
        context.reset();
    }

    std::unique_ptr<blm::Context> context;
    std::unique_ptr<blm::Module> module;
};

TEST_F(DCEPassFixture, NoDeadNodesNoChanges)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param = region->create_node<blm::Node>();
    param->ir_type = blm::NodeType::PARAM;
    param->type_kind = blm::DataType::INT32;

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(param);
    param->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(region->get_nodes().size(), 4);
}

TEST_F(DCEPassFixture, RemovesUnusedLiteral)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param = region->create_node<blm::Node>();
    param->ir_type = blm::NodeType::PARAM;
    param->type_kind = blm::DataType::INT32;

    auto* unused = region->create_node<blm::Node>();
    unused->ir_type = blm::NodeType::LIT;
    unused->type_kind = blm::DataType::INT32;
    unused->data.set<int32_t, blm::DataType::INT32>(42);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(param);
    param->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(region->get_nodes().size(), 4);
    EXPECT_EQ(pass_ctx.get_stat("dce.removed_nodes"), 1);
}

TEST_F(DCEPassFixture, KeepsUsedOperations)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param = region->create_node<blm::Node>();
    param->ir_type = blm::NodeType::PARAM;
    param->type_kind = blm::DataType::INT32;

    auto* lit = region->create_node<blm::Node>();
    lit->ir_type = blm::NodeType::LIT;
    lit->type_kind = blm::DataType::INT32;
    lit->data.set<int32_t, blm::DataType::INT32>(10);

    auto* add = region->create_node<blm::Node>();
    add->ir_type = blm::NodeType::ADD;
    add->type_kind = blm::DataType::INT32;
    add->inputs.push_back(param);
    add->inputs.push_back(lit);
    param->users.push_back(add);
    lit->users.push_back(add);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(region->get_nodes().size(), 6);
}

TEST_F(DCEPassFixture, RemovesDeadArithmeticChain)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param = region->create_node<blm::Node>();
    param->ir_type = blm::NodeType::PARAM;
    param->type_kind = blm::DataType::INT32;

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(param);
    param->users.push_back(ret);

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(10);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(20);

    auto* add = region->create_node<blm::Node>();
    add->ir_type = blm::NodeType::ADD;
    add->type_kind = blm::DataType::INT32;
    add->inputs.push_back(lit1);
    add->inputs.push_back(lit2);
    lit1->users.push_back(add);
    lit2->users.push_back(add);

    auto* mul = region->create_node<blm::Node>();
    mul->ir_type = blm::NodeType::MUL;
    mul->type_kind = blm::DataType::INT32;
    mul->inputs.push_back(add);
    mul->inputs.push_back(lit1);
    add->users.push_back(mul);
    lit1->users.push_back(mul);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("dce.removed_nodes"), 4); /* lit1, lit2, add, mul */
}

TEST_F(DCEPassFixture, KeepsSideEffects)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param = region->create_node<blm::Node>();
    param->ir_type = blm::NodeType::PARAM;
    param->type_kind = blm::DataType::INT32;

    auto* lit = region->create_node<blm::Node>();
    lit->ir_type = blm::NodeType::LIT;
    lit->type_kind = blm::DataType::INT32;
    lit->data.set<int32_t, blm::DataType::INT32>(42);

    auto* addr = region->create_node<blm::Node>();
    addr->ir_type = blm::NodeType::ADDR_OF;
    addr->type_kind = blm::DataType::POINTER;

    auto* store = region->create_node<blm::Node>();
    store->ir_type = blm::NodeType::STORE;
    store->inputs.push_back(addr);
    store->inputs.push_back(lit);
    addr->users.push_back(store);
    lit->users.push_back(store);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(param);
    param->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(region->get_nodes().size(), 7);
}

TEST_F(DCEPassFixture, RemovesUnusedNodes)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param1 = region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;
    param1->type_kind = blm::DataType::INT32;

    auto* param2 = region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;
    param2->type_kind = blm::DataType::INT32;

    auto* add = region->create_node<blm::Node>();
    add->ir_type = blm::NodeType::ADD;
    add->type_kind = blm::DataType::INT32;
    add->inputs.push_back(param1);
    add->inputs.push_back(param2);
    param1->users.push_back(add);
    param2->users.push_back(add);

    auto* sub = region->create_node<blm::Node>();
    sub->ir_type = blm::NodeType::SUB;
    sub->type_kind = blm::DataType::INT32;
    sub->inputs.push_back(param1);
    sub->inputs.push_back(param2);
    param1->users.push_back(sub);
    param2->users.push_back(sub);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(param1);
    param1->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("dce.removed_nodes"), 2);
}

TEST_F(DCEPassFixture, PreservesNoOptimizeNodes)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param = region->create_node<blm::Node>();
    param->ir_type = blm::NodeType::PARAM;
    param->type_kind = blm::DataType::INT32;

    auto* lit = region->create_node<blm::Node>();
    lit->ir_type = blm::NodeType::LIT;
    lit->type_kind = blm::DataType::INT32;
    lit->data.set<int32_t, blm::DataType::INT32>(42);

    auto* volatile_add = region->create_node<blm::Node>();
    volatile_add->ir_type = blm::NodeType::ADD;
    volatile_add->type_kind = blm::DataType::INT32;
    volatile_add->props = blm::NodeProps::NO_OPTIMIZE;
    volatile_add->inputs.push_back(param);
    volatile_add->inputs.push_back(lit);
    param->users.push_back(volatile_add);
    lit->users.push_back(volatile_add);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(param);
    param->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(region->get_nodes().size(), 6);
}

TEST_F(DCEPassFixture, SupportsNestedRegions)
{
    auto* function_region = module->create_region("test_function");

    auto* entry = function_region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* param = function_region->create_node<blm::Node>();
    param->ir_type = blm::NodeType::PARAM;
    param->type_kind = blm::DataType::INT32;

    auto* inner_region = module->create_region("inner", function_region);

    auto* lit = inner_region->create_node<blm::Node>();
    lit->ir_type = blm::NodeType::LIT;
    lit->type_kind = blm::DataType::INT32;
    lit->data.set<int32_t, blm::DataType::INT32>(42);

    auto* ret = function_region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(param);
    param->users.push_back(ret);

    auto* func = function_region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::DCEPass dce;
    bool changed = dce.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("dce.removed_nodes"), 1);
    EXPECT_TRUE(inner_region->get_nodes().empty());
}
