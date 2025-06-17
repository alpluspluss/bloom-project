/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/transform/reassociate.hpp>
#include <gtest/gtest.h>

class ReassociatePassFixture : public ::testing::Test
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

TEST_F(ReassociatePassFixture, NoReassociableExpressionsNoChanges)
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

	/* param1 - param2; subtraction is not reassociable */
	auto *sub = region->create_node<blm::Node>();
	sub->ir_type = blm::NodeType::SUB;
	sub->type_kind = blm::DataType::INT32;
	sub->inputs.push_back(param1);
	sub->inputs.push_back(param2);
	param1->users.push_back(sub);
	param2->users.push_back(sub);

	auto *ret = region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(sub);
	sub->users.push_back(ret);

	auto *func = region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::ReassociatePass reassoc;
	bool changed = reassoc.run(*module, pass_ctx);

	EXPECT_FALSE(changed);
	EXPECT_EQ(pass_ctx.get_stat("reassociate.count"), 0);
}

TEST_F(ReassociatePassFixture, ReassociateSimpleAddition)
{
	auto *region = module->create_region("test_function");

	auto *entry = region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto *param = region->create_node<blm::Node>();
	param->ir_type = blm::NodeType::PARAM;
	param->type_kind = blm::DataType::INT32;

	auto *lit1 = region->create_node<blm::Node>();
	lit1->ir_type = blm::NodeType::LIT;
	lit1->type_kind = blm::DataType::INT32;
	lit1->data.set<int32_t, blm::DataType::INT32>(10);

	auto *lit2 = region->create_node<blm::Node>();
	lit2->ir_type = blm::NodeType::LIT;
	lit2->type_kind = blm::DataType::INT32;
	lit2->data.set<int32_t, blm::DataType::INT32>(20);

	/* (param + 10) + 20 */
	auto *add1 = region->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->inputs.push_back(param);
	add1->inputs.push_back(lit1);
	param->users.push_back(add1);
	lit1->users.push_back(add1);

	auto *add2 = region->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(add1);
	add2->inputs.push_back(lit2);
	add1->users.push_back(add2);
	lit2->users.push_back(add2);

	auto *ret = region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add2);
	add2->users.push_back(ret);

	auto *func = region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::ReassociatePass reassoc;
	bool changed = reassoc.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_EQ(pass_ctx.get_stat("reassociate.count"), 1);

	for (blm::Node *node: ret->inputs)
	{
		EXPECT_EQ(node->ir_type, blm::NodeType::ADD);

		bool found_param = false;
		bool found_const_tree = false;

		for (blm::Node *input: node->inputs)
		{
			if (input == param)
			{
				found_param = true;
			}
			else if (input->ir_type == blm::NodeType::ADD)
			{
				/* constant tree (10 + 20) */
				found_const_tree = true;
				EXPECT_EQ(input->inputs.size(), 2);
				EXPECT_EQ(input->inputs[0]->ir_type, blm::NodeType::LIT);
				EXPECT_EQ(input->inputs[1]->ir_type, blm::NodeType::LIT);
			}
		}

		EXPECT_TRUE(found_param);
		EXPECT_TRUE(found_const_tree);
	}
}

TEST_F(ReassociatePassFixture, ReassociateNestedAddition)
{
	auto *region = module->create_region("test_function");

	auto *entry = region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto *param = region->create_node<blm::Node>();
	param->ir_type = blm::NodeType::PARAM;
	param->type_kind = blm::DataType::INT32;

	auto *lit1 = region->create_node<blm::Node>();
	lit1->ir_type = blm::NodeType::LIT;
	lit1->type_kind = blm::DataType::INT32;
	lit1->data.set<int32_t, blm::DataType::INT32>(10);

	auto *lit2 = region->create_node<blm::Node>();
	lit2->ir_type = blm::NodeType::LIT;
	lit2->type_kind = blm::DataType::INT32;
	lit2->data.set<int32_t, blm::DataType::INT32>(20);

	auto *lit3 = region->create_node<blm::Node>();
	lit3->ir_type = blm::NodeType::LIT;
	lit3->type_kind = blm::DataType::INT32;
	lit3->data.set<int32_t, blm::DataType::INT32>(30);

	/* ((param + 10) + 20) + 30 */
	auto *add1 = region->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->inputs.push_back(param);
	add1->inputs.push_back(lit1);
	param->users.push_back(add1);
	lit1->users.push_back(add1);

	auto *add2 = region->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(add1);
	add2->inputs.push_back(lit2);
	add1->users.push_back(add2);
	lit2->users.push_back(add2);

	auto *add3 = region->create_node<blm::Node>();
	add3->ir_type = blm::NodeType::ADD;
	add3->type_kind = blm::DataType::INT32;
	add3->inputs.push_back(add2);
	add3->inputs.push_back(lit3);
	add2->users.push_back(add3);
	lit3->users.push_back(add3);

	auto *ret = region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add3);
	add3->users.push_back(ret);

	auto *func = region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::ReassociatePass reassoc;
	bool changed = reassoc.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_EQ(pass_ctx.get_stat("reassociate.count"), 2);

	ASSERT_EQ(ret->inputs.size(), 1);
	blm::Node *new_root = ret->inputs[0];
	EXPECT_EQ(new_root->ir_type, blm::NodeType::ADD);
	EXPECT_EQ(new_root->inputs.size(), 2);

	bool param_found = false;
	bool const_tree_found = false;

	for (blm::Node *input: new_root->inputs)
	{
		if (input == param)
		{
			param_found = true;
		}
		else if (input->ir_type == blm::NodeType::ADD)
		{
			const_tree_found = true;
			/* This should be a balanced tree - all inputs should be LIT or ADD nodes */
			for (blm::Node *subinput: input->inputs)
			{
				EXPECT_TRUE(subinput->ir_type == blm::NodeType::LIT ||
					subinput->ir_type == blm::NodeType::ADD);
			}
		}
	}

	EXPECT_TRUE(param_found);
	EXPECT_TRUE(const_tree_found);
}

TEST_F(ReassociatePassFixture, ReassociateMultipleOperations)
{
	auto *region = module->create_region("test_function");

	auto *entry = region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto *param = region->create_node<blm::Node>();
	param->ir_type = blm::NodeType::PARAM;
	param->type_kind = blm::DataType::INT32;

	auto *lit1 = region->create_node<blm::Node>();
	lit1->ir_type = blm::NodeType::LIT;
	lit1->type_kind = blm::DataType::INT32;
	lit1->data.set<int32_t, blm::DataType::INT32>(2);

	auto *lit2 = region->create_node<blm::Node>();
	lit2->ir_type = blm::NodeType::LIT;
	lit2->type_kind = blm::DataType::INT32;
	lit2->data.set<int32_t, blm::DataType::INT32>(3);

	auto *lit3 = region->create_node<blm::Node>();
	lit3->ir_type = blm::NodeType::LIT;
	lit3->type_kind = blm::DataType::INT32;
	lit3->data.set<int32_t, blm::DataType::INT32>(4);

	/* param * (2 * 3) - should stay as is, all constants already grouped */
	auto *mul1 = region->create_node<blm::Node>();
	mul1->ir_type = blm::NodeType::MUL;
	mul1->type_kind = blm::DataType::INT32;
	mul1->inputs.push_back(lit1);
	mul1->inputs.push_back(lit2);
	lit1->users.push_back(mul1);
	lit2->users.push_back(mul1);

	auto *mul2 = region->create_node<blm::Node>();
	mul2->ir_type = blm::NodeType::MUL;
	mul2->type_kind = blm::DataType::INT32;
	mul2->inputs.push_back(param);
	mul2->inputs.push_back(mul1);
	param->users.push_back(mul2);
	mul1->users.push_back(mul2);

	/* (param * 4) * (2 * 3) - reassociate to param * (4 * (2 * 3)) */
	auto *mul3 = region->create_node<blm::Node>();
	mul3->ir_type = blm::NodeType::MUL;
	mul3->type_kind = blm::DataType::INT32;
	mul3->inputs.push_back(param);
	mul3->inputs.push_back(lit3);
	param->users.push_back(mul3);
	lit3->users.push_back(mul3);

	auto *mul4 = region->create_node<blm::Node>();
	mul4->ir_type = blm::NodeType::MUL;
	mul4->type_kind = blm::DataType::INT32;
	mul4->inputs.push_back(mul3);
	mul4->inputs.push_back(mul1);
	mul3->users.push_back(mul4);
	mul1->users.push_back(mul4);

	auto *ret = region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(mul4);
	mul4->users.push_back(ret);

	auto *func = region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::ReassociatePass reassoc;
	bool changed = reassoc.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_GT(pass_ctx.get_stat("reassociate.count"), 0);

	/* ret's input should be reassociated to param * (something) */
	ASSERT_EQ(ret->inputs.size(), 1);
	blm::Node *new_root = ret->inputs[0];
	EXPECT_EQ(new_root->ir_type, blm::NodeType::MUL);
	EXPECT_EQ(new_root->inputs.size(), 2);

	/* one of the inputs should be the param; the other should be a tree of constants */
	bool param_found = false;
	bool const_tree_found = false;

	for (blm::Node *input: new_root->inputs)
	{
		if (input == param)
		{
			param_found = true;
		}
		else
		{
			const_tree_found = true;
			/* Should be a nested MUL tree */
			EXPECT_TRUE(input->ir_type == blm::NodeType::MUL);
		}
	}

	EXPECT_TRUE(param_found);
	EXPECT_TRUE(const_tree_found);
}

TEST_F(ReassociatePassFixture, ReassociateBitwiseOperations)
{
	auto *region = module->create_region("test_function");

	auto *entry = region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto *param = region->create_node<blm::Node>();
	param->ir_type = blm::NodeType::PARAM;
	param->type_kind = blm::DataType::INT32;

	auto *lit1 = region->create_node<blm::Node>();
	lit1->ir_type = blm::NodeType::LIT;
	lit1->type_kind = blm::DataType::INT32;
	lit1->data.set<int32_t, blm::DataType::INT32>(0xFF);

	auto *lit2 = region->create_node<blm::Node>();
	lit2->ir_type = blm::NodeType::LIT;
	lit2->type_kind = blm::DataType::INT32;
	lit2->data.set<int32_t, blm::DataType::INT32>(0xF0);

	/* (param | 0xFF) | 0xF0 */
	auto *or1 = region->create_node<blm::Node>();
	or1->ir_type = blm::NodeType::BOR;
	or1->type_kind = blm::DataType::INT32;
	or1->inputs.push_back(param);
	or1->inputs.push_back(lit1);
	param->users.push_back(or1);
	lit1->users.push_back(or1);

	auto *or2 = region->create_node<blm::Node>();
	or2->ir_type = blm::NodeType::BOR;
	or2->type_kind = blm::DataType::INT32;
	or2->inputs.push_back(or1);
	or2->inputs.push_back(lit2);
	or1->users.push_back(or2);
	lit2->users.push_back(or2);

	auto *ret = region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(or2);
	or2->users.push_back(ret);

	auto *func = region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::ReassociatePass reassoc;
	bool changed = reassoc.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_EQ(pass_ctx.get_stat("reassociate.count"), 1);

	/* Check that ret's input is now param | (0xFF | 0xF0) */
	ASSERT_EQ(ret->inputs.size(), 1);
	blm::Node *new_root = ret->inputs[0];
	EXPECT_EQ(new_root->ir_type, blm::NodeType::BOR);
	EXPECT_EQ(new_root->inputs.size(), 2);

	/*  the other should be a bitwise OR of constants */
	bool param_found = false;
	bool const_or_found = false;

	for (blm::Node *input: new_root->inputs)
	{
		if (input == param)
		{
			param_found = true;
		}
		else if (input->ir_type == blm::NodeType::BOR)
		{
			const_or_found = true;
			/* Check that both inputs to the constant OR are literals */
			EXPECT_EQ(input->inputs.size(), 2);
			EXPECT_EQ(input->inputs[0]->ir_type, blm::NodeType::LIT);
			EXPECT_EQ(input->inputs[1]->ir_type, blm::NodeType::LIT);
		}
	}

	EXPECT_TRUE(param_found);
	EXPECT_TRUE(const_or_found);
}

TEST_F(ReassociatePassFixture, NoOptimizeNodesRespected)
{
	auto *region = module->create_region("test_function");

	auto *entry = region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto *param = region->create_node<blm::Node>();
	param->ir_type = blm::NodeType::PARAM;
	param->type_kind = blm::DataType::INT32;

	auto *lit1 = region->create_node<blm::Node>();
	lit1->ir_type = blm::NodeType::LIT;
	lit1->type_kind = blm::DataType::INT32;
	lit1->data.set<int32_t, blm::DataType::INT32>(10);

	auto *lit2 = region->create_node<blm::Node>();
	lit2->ir_type = blm::NodeType::LIT;
	lit2->type_kind = blm::DataType::INT32;
	lit2->data.set<int32_t, blm::DataType::INT32>(20);

	/* (param + 10) + 20 with NO_OPTIMIZE flag on first add */
	auto *add1 = region->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->props = blm::NodeProps::NO_OPTIMIZE;
	add1->inputs.push_back(param);
	add1->inputs.push_back(lit1);
	param->users.push_back(add1);
	lit1->users.push_back(add1);

	auto *add2 = region->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(add1);
	add2->inputs.push_back(lit2);
	add1->users.push_back(add2);
	lit2->users.push_back(add2);

	auto *ret = region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add2);
	add2->users.push_back(ret);

	auto *func = region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::ReassociatePass reassoc;
	bool changed = reassoc.run(*module, pass_ctx);

	/* add1 cannot be reassociated due to NO_OPTIMIZE but add2 could still be processed */
	EXPECT_TRUE(changed);
	EXPECT_EQ(pass_ctx.get_stat("reassociate.count"), 1);

	/* add1 should remain non-reassociatable */
	bool add1_intact = false;
	for (blm::Node *node: region->get_nodes())
	{
		if (node == add1 &&
		    node->inputs.size() == 2 &&
		    node->inputs[0] == param &&
		    node->inputs[1] == lit1)
		{
			add1_intact = true;
			break;
		}
	}
	EXPECT_TRUE(add1_intact);
}

TEST_F(ReassociatePassFixture, ComplexExpressionsReassociated)
{
	auto *region = module->create_region("test_function");

	auto *entry = region->create_node<blm::Node>();
	entry->ir_type = blm::NodeType::ENTRY;

	auto *param = region->create_node<blm::Node>();
	param->ir_type = blm::NodeType::PARAM;
	param->type_kind = blm::DataType::INT32;

	/* expr: (param + 5) + ((2 + 3) + (4 + param)) */
	auto *lit1 = region->create_node<blm::Node>();
	lit1->ir_type = blm::NodeType::LIT;
	lit1->type_kind = blm::DataType::INT32;
	lit1->data.set<int32_t, blm::DataType::INT32>(5);

	auto *lit2 = region->create_node<blm::Node>();
	lit2->ir_type = blm::NodeType::LIT;
	lit2->type_kind = blm::DataType::INT32;
	lit2->data.set<int32_t, blm::DataType::INT32>(2);

	auto *lit3 = region->create_node<blm::Node>();
	lit3->ir_type = blm::NodeType::LIT;
	lit3->type_kind = blm::DataType::INT32;
	lit3->data.set<int32_t, blm::DataType::INT32>(3);

	auto *lit4 = region->create_node<blm::Node>();
	lit4->ir_type = blm::NodeType::LIT;
	lit4->type_kind = blm::DataType::INT32;
	lit4->data.set<int32_t, blm::DataType::INT32>(4);

	auto *add1 = region->create_node<blm::Node>();
	add1->ir_type = blm::NodeType::ADD;
	add1->type_kind = blm::DataType::INT32;
	add1->inputs.push_back(param);
	add1->inputs.push_back(lit1);
	param->users.push_back(add1);
	lit1->users.push_back(add1);

	auto *add2 = region->create_node<blm::Node>();
	add2->ir_type = blm::NodeType::ADD;
	add2->type_kind = blm::DataType::INT32;
	add2->inputs.push_back(lit2);
	add2->inputs.push_back(lit3);
	lit2->users.push_back(add2);
	lit3->users.push_back(add2);

	auto *add3 = region->create_node<blm::Node>();
	add3->ir_type = blm::NodeType::ADD;
	add3->type_kind = blm::DataType::INT32;
	add3->inputs.push_back(lit4);
	add3->inputs.push_back(param);
	lit4->users.push_back(add3);
	param->users.push_back(add3);

	auto *add4 = region->create_node<blm::Node>();
	add4->ir_type = blm::NodeType::ADD;
	add4->type_kind = blm::DataType::INT32;
	add4->inputs.push_back(add2);
	add4->inputs.push_back(add3);
	add2->users.push_back(add4);
	add3->users.push_back(add4);

	auto *add5 = region->create_node<blm::Node>();
	add5->ir_type = blm::NodeType::ADD;
	add5->type_kind = blm::DataType::INT32;
	add5->inputs.push_back(add1);
	add5->inputs.push_back(add4);
	add1->users.push_back(add5);
	add4->users.push_back(add5);

	auto *ret = region->create_node<blm::Node>();
	ret->ir_type = blm::NodeType::RET;
	ret->inputs.push_back(add5);
	add5->users.push_back(ret);

	auto *func = region->create_node<blm::Node>();
	func->ir_type = blm::NodeType::FUNCTION;
	func->str_id = context->intern_string("test_function");
	module->add_function(func);

	blm::PassContext pass_ctx(*module, 1);
	blm::ReassociatePass reassoc;
	bool changed = reassoc.run(*module, pass_ctx);

	EXPECT_TRUE(changed);
	EXPECT_GT(pass_ctx.get_stat("reassociate.count"), 0);

	ASSERT_EQ(ret->inputs.size(), 1);
	blm::Node *new_root = ret->inputs[0];
	EXPECT_EQ(new_root->ir_type, blm::NodeType::ADD);

	auto found_const_tree = false;
	std::function<int(blm::Node *)> count_param_refs = [&](blm::Node *node) -> int
	{
		if (node == param)
			return 1;

		auto count = 0;
		for (blm::Node *input: node->inputs)
			count += count_param_refs(input);

		return count;
	};

	int param_refs = count_param_refs(new_root);
	EXPECT_EQ(param_refs, 2);

	std::function<bool(blm::Node *)> is_const_expr = [&](blm::Node *node) -> bool
	{
		if (node->ir_type == blm::NodeType::LIT)
			return true;

		if (node->ir_type != blm::NodeType::ADD)
			return false;

		for (blm::Node *input: node->inputs)
		{
			if (!is_const_expr(input))
				return false;
		}

		return true;
	};

	/* one subtree should consist only of constants */
	for (blm::Node *input: new_root->inputs)
	{
		if (input->ir_type == blm::NodeType::ADD && is_const_expr(input))
		{
			found_const_tree = true;
			break;
		}
	}

	EXPECT_TRUE(found_const_tree);
}
