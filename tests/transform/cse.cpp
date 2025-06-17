/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/print.hpp>
#include <bloom/transform/cse.hpp>
#include <gtest/gtest.h>

class CSEPassFixture : public ::testing::Test
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

TEST_F(CSEPassFixture, NoCommonExpressionsNoChanges)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *param1 = region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;
    param1->type_kind = blm::DataType::INT32;

    auto *param2 = region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;
    param2->type_kind = blm::DataType::INT32;

    auto *add = region->create_node<blm::Node>();
    add->ir_type = blm::NodeType::ADD;
    add->type_kind = blm::DataType::INT32;
    add->inputs.push_back(param1);
    add->inputs.push_back(param2);
    param1->users.push_back(add);
    param2->users.push_back(add);

    auto *mul = region->create_node<blm::Node>();
    mul->ir_type = blm::NodeType::MUL;
    mul->type_kind = blm::DataType::INT32;
    mul->inputs.push_back(param1);
    mul->inputs.push_back(param2);
    param1->users.push_back(mul);
    param2->users.push_back(mul);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(region->get_nodes().size(), 7);
}

TEST_F(CSEPassFixture, EliminatesIdenticalExpressions)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *param1 = region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;
    param1->type_kind = blm::DataType::INT32;

    auto *param2 = region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;
    param2->type_kind = blm::DataType::INT32;

    auto *add1 = region->create_node<blm::Node>();
    add1->ir_type = blm::NodeType::ADD;
    add1->type_kind = blm::DataType::INT32;
    add1->inputs.push_back(param1);
    add1->inputs.push_back(param2);
    param1->users.push_back(add1);
    param2->users.push_back(add1);

    auto *add2 = region->create_node<blm::Node>();
    add2->ir_type = blm::NodeType::ADD;
    add2->type_kind = blm::DataType::INT32;
    add2->inputs.push_back(param1);
    add2->inputs.push_back(param2);
    param1->users.push_back(add2);
    param2->users.push_back(add2);

    auto *mul = region->create_node<blm::Node>();
    mul->ir_type = blm::NodeType::MUL;
    mul->type_kind = blm::DataType::INT32;
    mul->inputs.push_back(add1);
    mul->inputs.push_back(add2);
    add1->users.push_back(mul);
    add2->users.push_back(mul);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(mul);
    mul->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("cse.eliminated_expressions"), 2);

    /* add2 was eliminated and replaced with add1 */
    for (blm::Node *user: mul->inputs)
    {
        EXPECT_EQ(user, add1);
    }
}

TEST_F(CSEPassFixture, HandlesCommutativeOperations)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *param1 = region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;
    param1->type_kind = blm::DataType::INT32;

    auto *param2 = region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;
    param2->type_kind = blm::DataType::INT32;

    auto *add1 = region->create_node<blm::Node>();
    add1->ir_type = blm::NodeType::ADD;
    add1->type_kind = blm::DataType::INT32;
    add1->inputs.push_back(param1);
    add1->inputs.push_back(param2);
    param1->users.push_back(add1);
    param2->users.push_back(add1);

    auto *add2 = region->create_node<blm::Node>();
    add2->ir_type = blm::NodeType::ADD;
    add2->type_kind = blm::DataType::INT32;
    add2->inputs.push_back(param2); /* reversed order compared to add1 */
    add2->inputs.push_back(param1);
    param2->users.push_back(add2);
    param1->users.push_back(add2);

    auto *mul = region->create_node<blm::Node>();
    mul->ir_type = blm::NodeType::MUL;
    mul->type_kind = blm::DataType::INT32;
    mul->inputs.push_back(add1);
    mul->inputs.push_back(add2);
    add1->users.push_back(mul);
    add2->users.push_back(mul);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(mul);
    mul->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("cse.eliminated_expressions"), 2);

    /* add2 was eliminated and replaced with add1 */
    for (blm::Node *user: mul->inputs)
    {
        EXPECT_EQ(user, add1);
    }
}

TEST_F(CSEPassFixture, IdenticalLiterals)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(42);

    auto *lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(42);

    auto *add = region->create_node<blm::Node>();
    add->ir_type = blm::NodeType::ADD;
    add->type_kind = blm::DataType::INT32;
    add->inputs.push_back(lit1);
    add->inputs.push_back(lit2);
    lit1->users.push_back(add);
    lit2->users.push_back(add);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("cse.eliminated_expressions"), 2);

    /* both inputs of add are now the same node */
    EXPECT_EQ(add->inputs[0], add->inputs[1]);
}

TEST_F(CSEPassFixture, DifferentTypesNotEliminated)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *param_i32 = region->create_node<blm::Node>();
    param_i32->ir_type = blm::NodeType::PARAM;
    param_i32->type_kind = blm::DataType::INT32;

    auto *param_i64 = region->create_node<blm::Node>();
    param_i64->ir_type = blm::NodeType::PARAM;
    param_i64->type_kind = blm::DataType::INT64;

    auto *lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(10);

    auto *lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT64;
    lit2->data.set<int64_t, blm::DataType::INT64>(10);

    auto *add1 = region->create_node<blm::Node>();
    add1->ir_type = blm::NodeType::ADD;
    add1->type_kind = blm::DataType::INT32;
    add1->inputs.push_back(param_i32);
    add1->inputs.push_back(lit1);
    param_i32->users.push_back(add1);
    lit1->users.push_back(add1);

    auto *add2 = region->create_node<blm::Node>();
    add2->ir_type = blm::NodeType::ADD;
    add2->type_kind = blm::DataType::INT64;
    add2->inputs.push_back(param_i64);
    add2->inputs.push_back(lit2);
    param_i64->users.push_back(add2);
    lit2->users.push_back(add2);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add1);
    add1->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    const bool changed = cse.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
}

TEST_F(CSEPassFixture, NoOptimizeNodesNotEliminated)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *param1 = region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;
    param1->type_kind = blm::DataType::INT32;

    auto *param2 = region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;
    param2->type_kind = blm::DataType::INT32;

    auto *add1 = region->create_node<blm::Node>();
    add1->ir_type = blm::NodeType::ADD;
    add1->type_kind = blm::DataType::INT32;
    add1->inputs.push_back(param1);
    add1->inputs.push_back(param2);
    param1->users.push_back(add1);
    param2->users.push_back(add1);

    auto *add2 = region->create_node<blm::Node>();
    add2->ir_type = blm::NodeType::ADD;
    add2->type_kind = blm::DataType::INT32;
    add2->inputs.push_back(param1);
    add2->inputs.push_back(param2);
    add2->props = blm::NodeProps::NO_OPTIMIZE;
    param1->users.push_back(add2);
    param2->users.push_back(add2);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add1);
    add1->users.push_back(ret);

    auto *sub = region->create_node<blm::Node>();
    sub->ir_type = blm::NodeType::SUB;
    sub->type_kind = blm::DataType::INT32;
    sub->inputs.push_back(add2);
    sub->inputs.push_back(param1);
    add2->users.push_back(sub);
    param1->users.push_back(sub);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    const bool changed = cse.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
}

TEST_F(CSEPassFixture, ComplexExpressionsEliminated)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *param1 = region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;
    param1->type_kind = blm::DataType::INT32;

    auto *param2 = region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;
    param2->type_kind = blm::DataType::INT32;

    /* (param1 + param2) * param1 */
    auto *add1 = region->create_node<blm::Node>();
    add1->ir_type = blm::NodeType::ADD;
    add1->type_kind = blm::DataType::INT32;
    add1->inputs.push_back(param1);
    add1->inputs.push_back(param2);
    param1->users.push_back(add1);
    param2->users.push_back(add1);

    auto *mul1 = region->create_node<blm::Node>();
    mul1->ir_type = blm::NodeType::MUL;
    mul1->type_kind = blm::DataType::INT32;
    mul1->inputs.push_back(add1);
    mul1->inputs.push_back(param1);
    add1->users.push_back(mul1);
    param1->users.push_back(mul1);

    /* (param1 + param2) * param1 */
    auto *add2 = region->create_node<blm::Node>();
    add2->ir_type = blm::NodeType::ADD;
    add2->type_kind = blm::DataType::INT32;
    add2->inputs.push_back(param1);
    add2->inputs.push_back(param2);
    param1->users.push_back(add2);
    param2->users.push_back(add2);

    auto *mul2 = region->create_node<blm::Node>();
    mul2->ir_type = blm::NodeType::MUL;
    mul2->type_kind = blm::DataType::INT32;
    mul2->inputs.push_back(add2);
    mul2->inputs.push_back(param1);
    add2->users.push_back(mul2);
    param1->users.push_back(mul2);

    /* use both */
    auto *add3 = region->create_node<blm::Node>();
    add3->ir_type = blm::NodeType::ADD;
    add3->type_kind = blm::DataType::INT32;
    add3->inputs.push_back(mul1);
    add3->inputs.push_back(mul2);
    mul1->users.push_back(add3);
    mul2->users.push_back(add3);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add3);
    add3->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("cse.eliminated_expressions"), 4); /* both add2 and mul2 should be eliminated */

    /* verify that add3 now has mul1 for both inputs */
    EXPECT_EQ(add3->inputs[0], mul1);
    EXPECT_EQ(add3->inputs[1], mul1);
}

TEST_F(CSEPassFixture, RegionHierarchyRespected)
{
    auto *outer_region = module->create_region("outer_function");

    auto *entry = outer_region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *param1 = outer_region->create_node<blm::Node>();
    param1->ir_type = blm::NodeType::PARAM;
    param1->type_kind = blm::DataType::INT32;

    auto *param2 = outer_region->create_node<blm::Node>();
    param2->ir_type = blm::NodeType::PARAM;
    param2->type_kind = blm::DataType::INT32;

    auto *outer_add = outer_region->create_node<blm::Node>();
    outer_add->ir_type = blm::NodeType::ADD;
    outer_add->type_kind = blm::DataType::INT32;
    outer_add->inputs.push_back(param1);
    outer_add->inputs.push_back(param2);
    param1->users.push_back(outer_add);
    param2->users.push_back(outer_add);

    auto *inner_region = module->create_region("inner", outer_region);

    auto *inner_add = inner_region->create_node<blm::Node>();
    inner_add->ir_type = blm::NodeType::ADD;
    inner_add->type_kind = blm::DataType::INT32;
    inner_add->inputs.push_back(param1);
    inner_add->inputs.push_back(param2);
    param1->users.push_back(inner_add);
    param2->users.push_back(inner_add);

    auto *ret = outer_region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(outer_add);
    outer_add->users.push_back(ret);

    auto *inner_ret = inner_region->create_node<blm::Node>();
    inner_ret->ir_type = blm::NodeType::RET;
    inner_ret->inputs.push_back(inner_add);
    inner_add->users.push_back(inner_ret);

    auto *func = outer_region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("outer_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("cse.eliminated_expressions"), 2);

    EXPECT_EQ(inner_ret->inputs[0], outer_add);
}

TEST_F(CSEPassFixture, FloatingPointLiterals)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::FLOAT64;
    lit1->data.set<double, blm::DataType::FLOAT64>(3.14159);

    auto *lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::FLOAT64;
    lit2->data.set<double, blm::DataType::FLOAT64>(3.14159);

    auto *lit3 = region->create_node<blm::Node>();
    lit3->ir_type = blm::NodeType::LIT;
    lit3->type_kind = blm::DataType::FLOAT64;
    lit3->data.set<double, blm::DataType::FLOAT64>(2.71828);

    auto *add1 = region->create_node<blm::Node>();
    add1->ir_type = blm::NodeType::ADD;
    add1->type_kind = blm::DataType::FLOAT64;
    add1->inputs.push_back(lit1);
    add1->inputs.push_back(lit3);
    lit1->users.push_back(add1);
    lit3->users.push_back(add1);

    auto *add2 = region->create_node<blm::Node>();
    add2->ir_type = blm::NodeType::ADD;
    add2->type_kind = blm::DataType::FLOAT64;
    add2->inputs.push_back(lit2);
    add2->inputs.push_back(lit3);
    lit2->users.push_back(add2);
    lit3->users.push_back(add2);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add1);
    add1->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("cse.eliminated_expressions"), 4); /* both lit2 and add2 should be eliminated */
}

TEST_F(CSEPassFixture, BooleanLiterals)
{
    auto *region = module->create_region("test_function");

    auto *entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto *true1 = region->create_node<blm::Node>();
    true1->ir_type = blm::NodeType::LIT;
    true1->type_kind = blm::DataType::BOOL;
    true1->data.set<bool, blm::DataType::BOOL>(true);

    auto *true2 = region->create_node<blm::Node>();
    true2->ir_type = blm::NodeType::LIT;
    true2->type_kind = blm::DataType::BOOL;
    true2->data.set<bool, blm::DataType::BOOL>(true);

    auto *false1 = region->create_node<blm::Node>();
    false1->ir_type = blm::NodeType::LIT;
    false1->type_kind = blm::DataType::BOOL;
    false1->data.set<bool, blm::DataType::BOOL>(false);

    auto *and1 = region->create_node<blm::Node>();
    and1->ir_type = blm::NodeType::BAND;
    and1->type_kind = blm::DataType::BOOL;
    and1->inputs.push_back(true1);
    and1->inputs.push_back(false1);
    true1->users.push_back(and1);
    false1->users.push_back(and1);

    auto *and2 = region->create_node<blm::Node>();
    and2->ir_type = blm::NodeType::BAND;
    and2->type_kind = blm::DataType::BOOL;
    and2->inputs.push_back(true2);
    and2->inputs.push_back(false1);
    true2->users.push_back(and2);
    false1->users.push_back(and2);

    auto *ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(and1);
    and1->users.push_back(ret);

    auto *func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::CSEPass cse;
    bool changed = cse.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("cse.eliminated_expressions"), 4); /* both true2 and and2 should be eliminated */
}
