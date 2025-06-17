/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/transform/constfold.hpp>
#include <gtest/gtest.h>

class ConstantFoldingPassFixture : public ::testing::Test
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

TEST_F(ConstantFoldingPassFixture, NoConstantsNoChanges)
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

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(region->get_nodes().size(), 6);
}

TEST_F(ConstantFoldingPassFixture, FoldAddition)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

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

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 1);

    /* add node should be replaced with a literal 30 */
    EXPECT_EQ(ret->inputs.size(), 1);
    EXPECT_EQ(ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret->inputs[0]->data.get<blm::DataType::INT32>(), 30);
}

TEST_F(ConstantFoldingPassFixture, FoldSubtraction)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(30);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(20);

    auto* sub = region->create_node<blm::Node>();
    sub->ir_type = blm::NodeType::SUB;
    sub->type_kind = blm::DataType::INT32;
    sub->inputs.push_back(lit1);
    sub->inputs.push_back(lit2);
    lit1->users.push_back(sub);
    lit2->users.push_back(sub);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(sub);
    sub->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 1);

    /* SUB node should be replaced with a literal 10 */
    EXPECT_EQ(ret->inputs.size(), 1);
    EXPECT_EQ(ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret->inputs[0]->data.get<blm::DataType::INT32>(), 10);
}

TEST_F(ConstantFoldingPassFixture, FoldMultiplication)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(4);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(5);

    auto* mul = region->create_node<blm::Node>();
    mul->ir_type = blm::NodeType::MUL;
    mul->type_kind = blm::DataType::INT32;
    mul->inputs.push_back(lit1);
    mul->inputs.push_back(lit2);
    lit1->users.push_back(mul);
    lit2->users.push_back(mul);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(mul);
    mul->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 1);

    /* mul node should be replaced with a literal 20 */
    EXPECT_EQ(ret->inputs.size(), 1);
    EXPECT_EQ(ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret->inputs[0]->data.get<blm::DataType::INT32>(), 20);
}

TEST_F(ConstantFoldingPassFixture, FoldDivision)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(20);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(5);

    auto* div = region->create_node<blm::Node>();
    div->ir_type = blm::NodeType::DIV;
    div->type_kind = blm::DataType::INT32;
    div->inputs.push_back(lit1);
    div->inputs.push_back(lit2);
    lit1->users.push_back(div);
    lit2->users.push_back(div);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(div);
    div->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 1);

    EXPECT_EQ(ret->inputs.size(), 1);
    EXPECT_EQ(ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret->inputs[0]->data.get<blm::DataType::INT32>(), 4);
}

TEST_F(ConstantFoldingPassFixture, DivisionByZeroNotFolded)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(20);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(0);

    auto* div = region->create_node<blm::Node>();
    div->ir_type = blm::NodeType::DIV;
    div->type_kind = blm::DataType::INT32;
    div->inputs.push_back(lit1);
    div->inputs.push_back(lit2);
    lit1->users.push_back(div);
    lit2->users.push_back(div);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(div);
    div->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    /* div by 0 no fold */
    EXPECT_FALSE(changed);
    EXPECT_EQ(ret->inputs[0], div);
}

TEST_F(ConstantFoldingPassFixture, FoldFloatingPointOperations)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::FLOAT32;
    lit1->data.set<float, blm::DataType::FLOAT32>(10.5f);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::FLOAT32;
    lit2->data.set<float, blm::DataType::FLOAT32>(2.5f);

    auto* add = region->create_node<blm::Node>();
    add->ir_type = blm::NodeType::ADD;
    add->type_kind = blm::DataType::FLOAT32;
    add->inputs.push_back(lit1);
    add->inputs.push_back(lit2);
    lit1->users.push_back(add);
    lit2->users.push_back(add);

    auto* mul = region->create_node<blm::Node>();
    mul->ir_type = blm::NodeType::MUL;
    mul->type_kind = blm::DataType::FLOAT32;
    mul->inputs.push_back(lit1);
    mul->inputs.push_back(lit2);
    lit1->users.push_back(mul);
    lit2->users.push_back(mul);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto* ret2 = region->create_node<blm::Node>();
    ret2->ir_type = blm::NodeType::RET;
    ret2->inputs.push_back(mul);
    mul->users.push_back(ret2);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 2);

    EXPECT_EQ(ret->inputs.size(), 1);
    EXPECT_EQ(ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret->inputs[0]->type_kind, blm::DataType::FLOAT32);
    EXPECT_FLOAT_EQ(ret->inputs[0]->data.get<blm::DataType::FLOAT32>(), 13.0f);

    EXPECT_EQ(ret2->inputs.size(), 1);
    EXPECT_EQ(ret2->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret2->inputs[0]->type_kind, blm::DataType::FLOAT32);
    EXPECT_FLOAT_EQ(ret2->inputs[0]->data.get<blm::DataType::FLOAT32>(), 26.25f);
}

TEST_F(ConstantFoldingPassFixture, FoldComparisons)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(10);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(20);

    auto* gt = region->create_node<blm::Node>();
    gt->ir_type = blm::NodeType::GT;
    gt->type_kind = blm::DataType::BOOL;
    gt->inputs.push_back(lit1);
    gt->inputs.push_back(lit2);
    lit1->users.push_back(gt);
    lit2->users.push_back(gt);

    auto* lt = region->create_node<blm::Node>();
    lt->ir_type = blm::NodeType::LT;
    lt->type_kind = blm::DataType::BOOL;
    lt->inputs.push_back(lit1);
    lt->inputs.push_back(lit2);
    lit1->users.push_back(lt);
    lit2->users.push_back(lt);

    auto* eq = region->create_node<blm::Node>();
    eq->ir_type = blm::NodeType::EQ;
    eq->type_kind = blm::DataType::BOOL;
    eq->inputs.push_back(lit1);
    eq->inputs.push_back(lit2);
    lit1->users.push_back(eq);
    lit2->users.push_back(eq);

    auto* ret1 = region->create_node<blm::Node>();
    ret1->ir_type = blm::NodeType::RET;
    ret1->inputs.push_back(gt);
    gt->users.push_back(ret1);

    auto* ret2 = region->create_node<blm::Node>();
    ret2->ir_type = blm::NodeType::RET;
    ret2->inputs.push_back(lt);
    lt->users.push_back(ret2);

    auto* ret3 = region->create_node<blm::Node>();
    ret3->ir_type = blm::NodeType::RET;
    ret3->inputs.push_back(eq);
    eq->users.push_back(ret3);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 3);

    EXPECT_EQ(ret1->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret1->inputs[0]->type_kind, blm::DataType::BOOL);
    EXPECT_FALSE(ret1->inputs[0]->data.get<blm::DataType::BOOL>());

    EXPECT_EQ(ret2->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret2->inputs[0]->type_kind, blm::DataType::BOOL);
    EXPECT_TRUE(ret2->inputs[0]->data.get<blm::DataType::BOOL>());

    EXPECT_EQ(ret3->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret3->inputs[0]->type_kind, blm::DataType::BOOL);
    EXPECT_FALSE(ret3->inputs[0]->data.get<blm::DataType::BOOL>());
}

TEST_F(ConstantFoldingPassFixture, FoldBitwiseOperations)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(0b1100);  /* 12 in binary */

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(0b1010);  /* 10 in binary */

    auto* band = region->create_node<blm::Node>();
    band->ir_type = blm::NodeType::BAND;
    band->type_kind = blm::DataType::INT32;
    band->inputs.push_back(lit1);
    band->inputs.push_back(lit2);
    lit1->users.push_back(band);
    lit2->users.push_back(band);

    auto* bor = region->create_node<blm::Node>();
    bor->ir_type = blm::NodeType::BOR;
    bor->type_kind = blm::DataType::INT32;
    bor->inputs.push_back(lit1);
    bor->inputs.push_back(lit2);
    lit1->users.push_back(bor);
    lit2->users.push_back(bor);

    auto* bxor = region->create_node<blm::Node>();
    bxor->ir_type = blm::NodeType::BXOR;
    bxor->type_kind = blm::DataType::INT32;
    bxor->inputs.push_back(lit1);
    bxor->inputs.push_back(lit2);
    lit1->users.push_back(bxor);
    lit2->users.push_back(bxor);

    auto* bnot = region->create_node<blm::Node>();
    bnot->ir_type = blm::NodeType::BNOT;
    bnot->type_kind = blm::DataType::INT32;
    bnot->inputs.push_back(lit1);
    lit1->users.push_back(bnot);

    auto* ret1 = region->create_node<blm::Node>();
    ret1->ir_type = blm::NodeType::RET;
    ret1->inputs.push_back(band);
    band->users.push_back(ret1);

    auto* ret2 = region->create_node<blm::Node>();
    ret2->ir_type = blm::NodeType::RET;
    ret2->inputs.push_back(bor);
    bor->users.push_back(ret2);

    auto* ret3 = region->create_node<blm::Node>();
    ret3->ir_type = blm::NodeType::RET;
    ret3->inputs.push_back(bxor);
    bxor->users.push_back(ret3);

    auto* ret4 = region->create_node<blm::Node>();
    ret4->ir_type = blm::NodeType::RET;
    ret4->inputs.push_back(bnot);
    bnot->users.push_back(ret4);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 4);

    /* BAND should fold to 8 */
    EXPECT_EQ(ret1->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret1->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret1->inputs[0]->data.get<blm::DataType::INT32>(), 0b1000);

    /* BOR should fold  to 14 */
    EXPECT_EQ(ret2->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret2->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret2->inputs[0]->data.get<blm::DataType::INT32>(), 0b1110);

    /* BXOR should fold to 6 */
    EXPECT_EQ(ret3->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret3->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret3->inputs[0]->data.get<blm::DataType::INT32>(), 0b0110);

    /* BNOT should fold to ~0b1100 */
    EXPECT_EQ(ret4->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret4->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret4->inputs[0]->data.get<blm::DataType::INT32>(), ~0b1100);
}

TEST_F(ConstantFoldingPassFixture, FoldComplexConstantExpression)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(5);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(10);

    auto* lit3 = region->create_node<blm::Node>();
    lit3->ir_type = blm::NodeType::LIT;
    lit3->type_kind = blm::DataType::INT32;
    lit3->data.set<int32_t, blm::DataType::INT32>(2);

    /* compute (5 + 10) * 2 */
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
    mul->inputs.push_back(lit3);
    add->users.push_back(mul);
    lit3->users.push_back(mul);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(mul);
    mul->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 2);

    EXPECT_EQ(ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret->inputs[0]->data.get<blm::DataType::INT32>(), 30);
}

TEST_F(ConstantFoldingPassFixture, NoOptimizeNodesNotFolded)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

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
    add->props = blm::NodeProps::NO_OPTIMIZE;
    lit1->users.push_back(add);
    lit2->users.push_back(add);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(ret->inputs[0], add);
}

TEST_F(ConstantFoldingPassFixture, FoldBitwiseShiftOperations)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(8);

    auto* lit2 = region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(2);

    auto* bshl = region->create_node<blm::Node>();
    bshl->ir_type = blm::NodeType::BSHL;
    bshl->type_kind = blm::DataType::INT32;
    bshl->inputs.push_back(lit1);
    bshl->inputs.push_back(lit2);
    lit1->users.push_back(bshl);
    lit2->users.push_back(bshl);

    auto* bshr = region->create_node<blm::Node>();
    bshr->ir_type = blm::NodeType::BSHR;
    bshr->type_kind = blm::DataType::INT32;
    bshr->inputs.push_back(lit1);
    bshr->inputs.push_back(lit2);
    lit1->users.push_back(bshr);
    lit2->users.push_back(bshr);

    auto* ret1 = region->create_node<blm::Node>();
    ret1->ir_type = blm::NodeType::RET;
    ret1->inputs.push_back(bshl);
    bshl->users.push_back(ret1);

    auto* ret2 = region->create_node<blm::Node>();
    ret2->ir_type = blm::NodeType::RET;
    ret2->inputs.push_back(bshr);
    bshr->users.push_back(ret2);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 2);

    EXPECT_EQ(ret1->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret1->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret1->inputs[0]->data.get<blm::DataType::INT32>(), 32);

    EXPECT_EQ(ret2->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(ret2->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(ret2->inputs[0]->data.get<blm::DataType::INT32>(), 2);
}

TEST_F(ConstantFoldingPassFixture, RegionHierarchy)
{
    auto* outer_region = module->create_region("outer_function");

    auto* entry = outer_region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* lit1 = outer_region->create_node<blm::Node>();
    lit1->ir_type = blm::NodeType::LIT;
    lit1->type_kind = blm::DataType::INT32;
    lit1->data.set<int32_t, blm::DataType::INT32>(10);

    auto* lit2 = outer_region->create_node<blm::Node>();
    lit2->ir_type = blm::NodeType::LIT;
    lit2->type_kind = blm::DataType::INT32;
    lit2->data.set<int32_t, blm::DataType::INT32>(20);

    auto* outer_add = outer_region->create_node<blm::Node>();
    outer_add->ir_type = blm::NodeType::ADD;
    outer_add->type_kind = blm::DataType::INT32;
    outer_add->inputs.push_back(lit1);
    outer_add->inputs.push_back(lit2);
    lit1->users.push_back(outer_add);
    lit2->users.push_back(outer_add);

    auto* inner_region = module->create_region("inner", outer_region);

    auto* lit3 = inner_region->create_node<blm::Node>();
    lit3->ir_type = blm::NodeType::LIT;
    lit3->type_kind = blm::DataType::INT32;
    lit3->data.set<int32_t, blm::DataType::INT32>(5);

    auto* lit4 = inner_region->create_node<blm::Node>();
    lit4->ir_type = blm::NodeType::LIT;
    lit4->type_kind = blm::DataType::INT32;
    lit4->data.set<int32_t, blm::DataType::INT32>(15);

    auto* inner_add = inner_region->create_node<blm::Node>();
    inner_add->ir_type = blm::NodeType::ADD;
    inner_add->type_kind = blm::DataType::INT32;
    inner_add->inputs.push_back(lit3);
    inner_add->inputs.push_back(lit4);
    lit3->users.push_back(inner_add);
    lit4->users.push_back(inner_add);

    auto* outer_ret = outer_region->create_node<blm::Node>();
    outer_ret->ir_type = blm::NodeType::RET;
    outer_ret->inputs.push_back(outer_add);
    outer_add->users.push_back(outer_ret);

    auto* inner_ret = inner_region->create_node<blm::Node>();
    inner_ret->ir_type = blm::NodeType::RET;
    inner_ret->inputs.push_back(inner_add);
    inner_add->users.push_back(inner_ret);

    auto* func = outer_region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("outer_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_TRUE(changed);
    EXPECT_EQ(pass_ctx.get_stat("constant_folding.folded_nodes"), 2);

    EXPECT_EQ(outer_ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(outer_ret->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(outer_ret->inputs[0]->data.get<blm::DataType::INT32>(), 30);

    EXPECT_EQ(inner_ret->inputs[0]->ir_type, blm::NodeType::LIT);
    EXPECT_EQ(inner_ret->inputs[0]->type_kind, blm::DataType::INT32);
    EXPECT_EQ(inner_ret->inputs[0]->data.get<blm::DataType::INT32>(), 20);
}

TEST_F(ConstantFoldingPassFixture, MixedConstantAndVariableNotFolded)
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
    blm::ConstantFoldingPass const_fold;
    const bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(ret->inputs[0], add);
}

TEST_F(ConstantFoldingPassFixture, IntAndFloatMixedTypesNotFolded)
{
    auto* region = module->create_region("test_function");

    auto* entry = region->create_node<blm::Node>();
    entry->ir_type = blm::NodeType::ENTRY;

    auto* int_lit = region->create_node<blm::Node>();
    int_lit->ir_type = blm::NodeType::LIT;
    int_lit->type_kind = blm::DataType::INT32;
    int_lit->data.set<int32_t, blm::DataType::INT32>(10);

    auto* float_lit = region->create_node<blm::Node>();
    float_lit->ir_type = blm::NodeType::LIT;
    float_lit->type_kind = blm::DataType::FLOAT32;
    float_lit->data.set<float, blm::DataType::FLOAT32>(20.0f);

    auto* add = region->create_node<blm::Node>();
    add->ir_type = blm::NodeType::ADD;
    add->type_kind = blm::DataType::FLOAT32;
    add->inputs.push_back(int_lit);
    add->inputs.push_back(float_lit);
    int_lit->users.push_back(add);
    float_lit->users.push_back(add);

    auto* ret = region->create_node<blm::Node>();
    ret->ir_type = blm::NodeType::RET;
    ret->inputs.push_back(add);
    add->users.push_back(ret);

    auto* func = region->create_node<blm::Node>();
    func->ir_type = blm::NodeType::FUNCTION;
    func->str_id = context->intern_string("test_function");
    module->add_function(func);

    blm::PassContext pass_ctx(*module, 1);
    blm::ConstantFoldingPass const_fold;
    const bool changed = const_fold.run(*module, pass_ctx);

    EXPECT_FALSE(changed);
    EXPECT_EQ(ret->inputs[0], add);
}
