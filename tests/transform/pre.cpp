/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/transform/pre.hpp>
#include <gtest/gtest.h>

class PREPassFixture : public ::testing::Test
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

TEST_F(PREPassFixture, NoRedundantExpressionsNoChanges)
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
	blm::PREPass pre;
	bool changed = pre.run(*module, pass_ctx);

	EXPECT_FALSE(changed);
	EXPECT_EQ(pass_ctx.get_stat("pre.hoisted_expressions"), 0);
}

TEST_F(PREPassFixture, BasicRedundantExpressionElimination)
{
	auto* parent_region = module->create_region("test_function");

	auto* entry = parent_region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto* param1 = parent_region->create_node<blm::Node>();
	param1->ir_type = blm::NodeType::PARAM;
	param1->type_kind = blm::DataType::INT32;

	auto* param2 = parent_region->create_node<blm::Node>();
	param2->ir_type = blm::NodeType::PARAM;
	param2->type_kind = blm::DataType::INT32;

	auto* child1 = module->create_region("child1", parent_region);

	auto* add1 = child1->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->inputs.push_back(param1);
	add1->inputs.push_back(param2);
	param1->users.push_back(add1);
	param2->users.push_back(add1);

	auto* mul1 = child1->create_node<blm::Node>();
	mul1->ir_type = blm::NodeType::MUL;
	mul1->type_kind = blm::DataType::INT32;
	mul1->inputs.push_back(add1);
	mul1->inputs.push_back(param1);
	add1->users.push_back(mul1);
	param1->users.push_back(mul1);

	auto* child2 = module->create_region("child2", parent_region);

	auto* add2 = child2->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(param1);
	add2->inputs.push_back(param2);
	param1->users.push_back(add2);
	param2->users.push_back(add2);

	auto* mul2 = child2->create_node<blm::Node>();
	mul2->ir_type = blm::NodeType::MUL;
	mul2->type_kind = blm::DataType::INT32;
	mul2->inputs.push_back(add2);
	mul2->inputs.push_back(param1);
	add2->users.push_back(mul2);
	param1->users.push_back(mul2);

	auto* add3 = parent_region->create_node<blm::Node>();
	add3->ir_type = blm::NodeType::ADD;
	add3->type_kind = blm::DataType::INT32;
	add3->inputs.push_back(mul1);
	add3->inputs.push_back(mul2);
	mul1->users.push_back(add3);
	mul2->users.push_back(add3);

	auto* ret = parent_region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add3);
	add3->users.push_back(ret);

	auto* func = parent_region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::PREPass pre;
	bool changed = pre.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_GE(pass_ctx.get_stat("pre.hoisted_expressions"), 1);

	bool found_hoisted_add = false;
	for (blm::Node* node : parent_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::ADD &&
			node->inputs.size() == 2 &&
			node->inputs[0] == param1 &&
			node->inputs[1] == param2)
		{
			found_hoisted_add = true;
			break;
		}
	}
	EXPECT_TRUE(found_hoisted_add);
	EXPECT_EQ(mul1->inputs[0], mul2->inputs[0]);
}

TEST_F(PREPassFixture, CommutativeOperationsRecognized)
{
	auto* parent_region = module->create_region("test_function");

	auto* entry = parent_region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto* param1 = parent_region->create_node<blm::Node>();
	param1->ir_type = blm::NodeType::PARAM;
	param1->type_kind = blm::DataType::INT32;

	auto* param2 = parent_region->create_node<blm::Node>();
	param2->ir_type = blm::NodeType::PARAM;
	param2->type_kind = blm::DataType::INT32;

	auto* child1 = module->create_region("child1", parent_region);

	auto* add1 = child1->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->inputs.push_back(param1);
	add1->inputs.push_back(param2);
	param1->users.push_back(add1);
	param2->users.push_back(add1);

	auto* child2 = module->create_region("child2", parent_region);

	auto* add2 = child2->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(param2); /* in reversed order compared to add1 */
	add2->inputs.push_back(param1);
	param2->users.push_back(add2);
	param1->users.push_back(add2);

	auto* add3 = parent_region->create_node<blm::Node>();
	add3->ir_type = blm::NodeType::ADD;
	add3->type_kind = blm::DataType::INT32;
	add3->inputs.push_back(add1);
	add3->inputs.push_back(add2);
	add1->users.push_back(add3);
	add2->users.push_back(add3);

	auto* ret = parent_region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add3);
	add3->users.push_back(ret);

	auto* func = parent_region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::PREPass pre;
	bool changed = pre.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_GE(pass_ctx.get_stat("pre.hoisted_expressions"), 1);

	/* note: both add nodes should be replaced by a hoisted add in the parent region */
	bool found_hoisted_add = false;
	for (blm::Node* node : parent_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::ADD &&
			((node->inputs[0] == param1 && node->inputs[1] == param2) ||
			(node->inputs[0] == param2 && node->inputs[1] == param1)))
		{
			found_hoisted_add = true;
			break;
		}
	}
	EXPECT_TRUE(found_hoisted_add);
	EXPECT_EQ(add3->inputs[0], add3->inputs[1]);
}

TEST_F(PREPassFixture, NonCommutaticeOperationsNotConfused)
{
	auto* parent_region = module->create_region("test_function");

	auto* entry = parent_region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto* param1 = parent_region->create_node<blm::Node>();
	param1->ir_type = blm::NodeType::PARAM;
	param1->type_kind = blm::DataType::INT32;

	auto* param2 = parent_region->create_node<blm::Node>();
	param2->ir_type = blm::NodeType::PARAM;
	param2->type_kind = blm::DataType::INT32;

	auto* child1 = module->create_region("child1", parent_region);

	auto* sub1 = child1->create_node<blm::Node>();
	sub1->ir_type = blm::NodeType::SUB; /* sub is not commutative */
	sub1->type_kind = blm::DataType::INT32;
	sub1->inputs.push_back(param1);
	sub1->inputs.push_back(param2);
	param1->users.push_back(sub1);
	param2->users.push_back(sub1);

	/* second child region with reversed operands */
	auto* child2 = module->create_region("child2", parent_region);

	auto* sub2 = child2->create_node<blm::Node>();
	sub2->ir_type = blm::NodeType::SUB;
	sub2->type_kind = blm::DataType::INT32;
	sub2->inputs.push_back(param2);
	sub2->inputs.push_back(param1);
	param2->users.push_back(sub2);
	param1->users.push_back(sub2);

	auto* add = parent_region->create_node<blm::Node>();
	add->ir_type = blm::NodeType::ADD;
	add->type_kind = blm::DataType::INT32;
	add->inputs.push_back(sub1);
	add->inputs.push_back(sub2);
	sub1->users.push_back(add);
	sub2->users.push_back(add);

	auto* ret = parent_region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add);
	add->users.push_back(ret);

	auto* func = parent_region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::PREPass pre;
	bool changed = pre.run(*module, pass_ctx);

	/* since subtraction is not commutative, these are not redundant expressions */
	EXPECT_FALSE(changed);
	EXPECT_EQ(pass_ctx.get_stat("pre.hoisted_expressions"), 0);

	/* both subtract operations should remain distinct */
	EXPECT_NE(add->inputs[0], add->inputs[1]);
	EXPECT_EQ(sub1->inputs[0], param1);
	EXPECT_EQ(sub1->inputs[1], param2);
	EXPECT_EQ(sub2->inputs[0], param2);
	EXPECT_EQ(sub2->inputs[1], param1);
}

TEST_F(PREPassFixture, NoOptimizeNodesRespected)
{
	auto* parent_region = module->create_region("test_function");

	auto* entry = parent_region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto* param1 = parent_region->create_node<blm::Node>();
	param1->ir_type = blm::NodeType::PARAM;
	param1->type_kind = blm::DataType::INT32;

	auto* param2 = parent_region->create_node<blm::Node>();
	param2->ir_type = blm::NodeType::PARAM;
	param2->type_kind = blm::DataType::INT32;

	auto* child1 = module->create_region("child1", parent_region);

	auto* add1 = child1->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->props = blm::NodeProps::NO_OPTIMIZE;
	add1->inputs.push_back(param1);
	add1->inputs.push_back(param2);
	param1->users.push_back(add1);
	param2->users.push_back(add1);

	auto* child2 = module->create_region("child2", parent_region);

	auto* add2 = child2->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(param1);
	add2->inputs.push_back(param2);
	param1->users.push_back(add2);
	param2->users.push_back(add2);

	auto* add3 = parent_region->create_node<blm::Node>();
	add3->ir_type = blm::NodeType::ADD;
	add3->type_kind = blm::DataType::INT32;
	add3->inputs.push_back(add1);
	add3->inputs.push_back(add2);
	add1->users.push_back(add3);
	add2->users.push_back(add3);

	auto* ret = parent_region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add3);
	add3->users.push_back(ret);

	auto* func = parent_region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::PREPass pre;
	bool changed = pre.run(*module, pass_ctx);

	/* add2 node should be hoisted but add1 should be preserved due to NO_OPTIMIZE */
	EXPECT_FALSE(changed);
	EXPECT_EQ(pass_ctx.get_stat("pre.hoisted_expressions"), 0);
	EXPECT_NE(add3->inputs[0], add3->inputs[1]);
}

TEST_F(PREPassFixture, RedundantConstantExpressions)
{
	auto* parent_region = module->create_region("test_function");

	auto* entry = parent_region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto* param = parent_region->create_node<blm::Node>();
	param->ir_type = blm::NodeType::PARAM;
	param->type_kind = blm::DataType::INT32;

	auto* lit1 = parent_region->create_node<blm::Node>();
	lit1->ir_type = blm::NodeType::LIT;
	lit1->type_kind = blm::DataType::INT32;
	lit1->data.set<std::int32_t, blm::DataType::INT32>(10);

	auto* lit2 = parent_region->create_node<blm::Node>();
	lit2->ir_type = blm::NodeType::LIT;
	lit2->type_kind = blm::DataType::INT32;
	lit2->data.set<std::int32_t, blm::DataType::INT32>(20);

	auto* child1 = module->create_region("child1", parent_region);

	auto* add1 = child1->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->inputs.push_back(lit1);
	add1->inputs.push_back(lit2);
	lit1->users.push_back(add1);
	lit2->users.push_back(add1);

	auto* mul1 = child1->create_node<blm::Node>();
	mul1->ir_type = blm::NodeType::MUL;
	mul1->type_kind = blm::DataType::INT32;
	mul1->inputs.push_back(add1);
	mul1->inputs.push_back(param);
	add1->users.push_back(mul1);
	param->users.push_back(mul1);

	auto* child2 = module->create_region("child2", parent_region);

	auto* add2 = child2->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(lit1);
	add2->inputs.push_back(lit2);
	lit1->users.push_back(add2);
	lit2->users.push_back(add2);

	auto* mul2 = child2->create_node<blm::Node>();
	mul2->ir_type = blm::NodeType::MUL;
	mul2->type_kind = blm::DataType::INT32;
	mul2->inputs.push_back(add2);
	mul2->inputs.push_back(param);
	add2->users.push_back(mul2);
	param->users.push_back(mul2);

	auto* add3 = parent_region->create_node<blm::Node>();
	add3->ir_type = blm::NodeType::ADD;
	add3->type_kind = blm::DataType::INT32;
	add3->inputs.push_back(mul1);
	add3->inputs.push_back(mul2);
	mul1->users.push_back(add3);
	mul2->users.push_back(add3);

	auto* ret = parent_region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add3);
	add3->users.push_back(ret);

	auto* func = parent_region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::PREPass pre;
	bool changed = pre.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_GE(pass_ctx.get_stat("pre.hoisted_expressions"), 1);

	bool found_hoisted_add = false;
	for (blm::Node* node : parent_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::ADD &&
			node->inputs.size() == 2 &&
			((node->inputs[0] == lit1 && node->inputs[1] == lit2) ||
			(node->inputs[0] == lit2 && node->inputs[1] == lit1)))
		{
			found_hoisted_add = true;
			break;
		}
	}
	EXPECT_TRUE(found_hoisted_add);
	EXPECT_EQ(mul1->inputs[0], mul2->inputs[0]);
}
