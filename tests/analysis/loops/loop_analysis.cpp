/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/loops/loop-analysis.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <gtest/gtest.h>

class LoopAnalysisTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		context = std::make_unique<blm::Context>();
		builder = std::make_unique<blm::Builder>(*context);
		module = builder->create_module("loop_test");
	}

	void TearDown() override
	{
		builder.reset();
		context.reset();
	}

	/* simple loop: counter < n */
	blm::Node *create_simple_loop()
	{
		auto func = builder->create_function("simple_loop", { blm::DataType::INT32 }, blm::DataType::INT32);
		auto *n = func.add_parameter("n", blm::DataType::INT32);

		func.body([&]
		{
			auto loop = builder->create_while_loop("header", "body", "exit");

			auto *zero = builder->literal(0);
			auto *one = builder->literal(1);
			auto *counter_ptr = builder->stack_alloc(builder->literal(4), blm::DataType::INT32);
			builder->store(zero, counter_ptr);

			/* jump to header */
			builder->jump(loop.header.get_region()->get_nodes()[0]);

			loop.header([&]
			{
				auto *counter = builder->load(counter_ptr, blm::DataType::INT32);
				auto *condition = builder->lt(counter, n);
				builder->branch(condition,
				                loop.body.get_region()->get_nodes()[0],
				                loop.exit.get_region()->get_nodes()[0]);
			});

			loop.body([&]
			{
				auto *counter = builder->load(counter_ptr, blm::DataType::INT32);
				auto *new_counter = builder->add(counter, one);
				builder->store(new_counter, counter_ptr);
				builder->jump(loop.header.get_region()->get_nodes()[0]); /* back-edge */
			});

			loop.exit([&]
			{
				auto *final_counter = builder->load(counter_ptr, blm::DataType::INT32);
				builder->ret(final_counter);
			});
		});

		return func.get_function();
	}

	/* nested loops: i < rows, j < cols */
	blm::Node *create_nested_loops()
	{
		auto func = builder->create_function("nested", { blm::DataType::INT32 }, blm::DataType::INT32);
		auto *n = func.add_parameter("n", blm::DataType::INT32);

		func.body([&]
		{
			auto outer = builder->create_while_loop("outer_header", "outer_body", "outer_exit");

			auto *zero = builder->literal(0);
			auto *one = builder->literal(1);
			auto *i_ptr = builder->stack_alloc(builder->literal(4), blm::DataType::INT32);
			builder->store(zero, i_ptr);

			builder->jump(outer.header.get_region()->get_nodes()[0]);

			outer.header([&]
			{
				auto *i = builder->load(i_ptr, blm::DataType::INT32);
				auto *condition = builder->lt(i, n);
				builder->branch(condition,
				                outer.body.get_region()->get_nodes()[0],
				                outer.exit.get_region()->get_nodes()[0]);
			});

			outer.body([&]
			{
				/* inner loop */
				auto inner = builder->create_while_loop("inner_header", "inner_body", "inner_exit");
				auto *j_ptr = builder->stack_alloc(builder->literal(4), blm::DataType::INT32);
				builder->store(zero, j_ptr);

				builder->jump(inner.header.get_region()->get_nodes()[0]);

				inner.header([&]
				{
					auto *j = builder->load(j_ptr, blm::DataType::INT32);
					auto *condition = builder->lt(j, n);
					builder->branch(condition,
					                inner.body.get_region()->get_nodes()[0],
					                inner.exit.get_region()->get_nodes()[0]);
				});

				inner.body([&]
				{
					auto *j = builder->load(j_ptr, blm::DataType::INT32);
					auto *new_j = builder->add(j, one);
					builder->store(new_j, j_ptr);
					builder->jump(inner.header.get_region()->get_nodes()[0]); /* inner back-edge */
				});

				inner.exit([&]
				{
					auto *i = builder->load(i_ptr, blm::DataType::INT32);
					auto *new_i = builder->add(i, one);
					builder->store(new_i, i_ptr);
					builder->jump(outer.header.get_region()->get_nodes()[0]); /* outer back-edge */
				});
			});

			outer.exit([&]
			{
				auto *final_i = builder->load(i_ptr, blm::DataType::INT32);
				builder->ret(final_i);
			});
		});

		return func.get_function();
	}

	std::unique_ptr<blm::Context> context;
	std::unique_ptr<blm::Builder> builder;
	blm::Module *module = nullptr;
};

TEST_F(LoopAnalysisTest, DetectSingleLoop)
{
	auto *func = create_simple_loop();

	blm::PassContext pass_ctx(*module, 1);
	blm::LoopAnalysisPass pass;
	auto result = pass.analyze(*module, pass_ctx);

	auto *loop_result = dynamic_cast<blm::LoopAnalysisResult *>(result.get());
	ASSERT_NE(loop_result, nullptr);

	const auto *tree = loop_result->get_loops_for_function(func);
	ASSERT_NE(tree, nullptr);

	EXPECT_EQ(tree->all_loops.size(), 1);
	EXPECT_EQ(tree->root_loops.size(), 1);
	EXPECT_EQ(tree->max_depth, 0);

	auto *loop = tree->root_loops[0];
	EXPECT_TRUE(loop->is_natural());
	EXPECT_EQ(loop->depth, 0);
	EXPECT_EQ(loop->children.size(), 0);
}

TEST_F(LoopAnalysisTest, DetectNestedLoops)
{
	auto *func = create_nested_loops();

	blm::PassContext pass_ctx(*module, 1);
	blm::LoopAnalysisPass pass;
	auto result = pass.analyze(*module, pass_ctx);

	auto *loop_result = dynamic_cast<blm::LoopAnalysisResult *>(result.get());
	ASSERT_NE(loop_result, nullptr);

	const auto *tree = loop_result->get_loops_for_function(func);
	ASSERT_NE(tree, nullptr);

	EXPECT_EQ(tree->all_loops.size(), 2);
	EXPECT_EQ(tree->root_loops.size(), 1);
	EXPECT_EQ(tree->max_depth, 1);

	auto *outer = tree->root_loops[0];
	EXPECT_EQ(outer->depth, 0);
	EXPECT_EQ(outer->children.size(), 1);

	auto *inner = outer->children[0];
	EXPECT_EQ(inner->depth, 1);
	EXPECT_EQ(inner->parent, outer);
}

TEST_F(LoopAnalysisTest, RegionMapping)
{
	create_simple_loop();

	blm::PassContext pass_ctx(*module, 1);
	blm::LoopAnalysisPass pass;
	auto result = pass.analyze(*module, pass_ctx);

	auto *loop_result = dynamic_cast<blm::LoopAnalysisResult *>(result.get());
	ASSERT_NE(loop_result, nullptr);

	/* find header region */
	blm::Region *header = nullptr;
	for (auto *child: module->get_root_region()->get_children())
	{
		if (child->get_name() == "simple_loop")
		{
			for (auto *grandchild: child->get_children())
			{
				if (grandchild->get_name() == "header")
				{
					header = grandchild;
					break;
				}
			}
			break;
		}
	}

	ASSERT_NE(header, nullptr);
	auto *loop = loop_result->get_loop_for_region(header);
	EXPECT_NE(loop, nullptr);
}

TEST_F(LoopAnalysisTest, NoLoopsFunction)
{
	auto func = builder->create_function("no_loops", { blm::DataType::INT32 }, blm::DataType::INT32);
	auto *param = func.add_parameter("x", blm::DataType::INT32);

	func.body([&]
	{
		auto *result = builder->add(param, builder->literal(1));
		builder->ret(result);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LoopAnalysisPass pass;
	auto result = pass.analyze(*module, pass_ctx);

	auto *loop_result = dynamic_cast<blm::LoopAnalysisResult *>(result.get());
	ASSERT_NE(loop_result, nullptr);

	const auto *tree = loop_result->get_loops_for_function(func.get_function());
	ASSERT_NE(tree, nullptr);

	EXPECT_EQ(tree->all_loops.size(), 0);
	EXPECT_EQ(tree->root_loops.size(), 0);
	EXPECT_EQ(tree->max_depth, 0);
}

TEST_F(LoopAnalysisTest, Statistics)
{
	create_simple_loop();
	create_nested_loops();

	blm::PassContext pass_ctx(*module, 1);
	blm::LoopAnalysisPass pass;
	pass.analyze(*module, pass_ctx);

	EXPECT_EQ(pass_ctx.get_stat("loop_analysis.total_loops"), 3);
	EXPECT_EQ(pass_ctx.get_stat("loop_analysis.max_nesting_depth"), 1);
}
