/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/ipo/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <gtest/gtest.h>

class CallGraphAnalysisFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		context = std::make_unique<blm::Context>();
		builder = std::make_unique<blm::Builder>(*context);
	}

	void TearDown() override
	{
		builder.reset();
		context.reset();
	}

	std::unique_ptr<blm::Context> context;
	std::unique_ptr<blm::Builder> builder;
};

TEST_F(CallGraphAnalysisFixture, EmptyModuleCallGraph)
{
	auto *module = builder->create_module("empty_module");

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	ASSERT_NE(cg_result, nullptr);

	const auto &call_graph = cg_result->get_call_graph();
	EXPECT_TRUE(call_graph.empty());
	EXPECT_EQ(call_graph.size(), 0);
}

TEST_F(CallGraphAnalysisFixture, SingleFunctionNoCallsCallGraph)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("main", {}, blm::DataType::VOID);

	func.body([&]
	{
		auto *value = builder->literal(42);
		builder->ret(value);
	});

	std::vector<blm::Module *> modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	ASSERT_NE(cg_result, nullptr);

	const auto &call_graph = cg_result->get_call_graph();
	EXPECT_EQ(call_graph.size(), 0);

	auto entry_points = call_graph.get_entry_points();
	EXPECT_EQ(entry_points.size(), 0);

	auto leaf_functions = call_graph.get_leaf_functions();
	EXPECT_EQ(leaf_functions.size(), 0);
	EXPECT_FALSE(call_graph.has_cycles());
	EXPECT_EQ(ipo_ctx.get_stat("callgraph.functions_analyzed"), 1);
}

TEST_F(CallGraphAnalysisFixture, DirectCallGraph)
{
	auto *module = builder->create_module("test_module");
	auto callee_func = builder->create_function("callee", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *callee_node = callee_func.get_function();

	callee_func.body([&]
	{
		auto *param = callee_func.add_parameter("x", blm::DataType::INT32);
		builder->ret(param);
	});

	auto caller_func = builder->create_function("caller", {}, blm::DataType::VOID);
	blm::Node *caller_node = caller_func.get_function();

	caller_func.body([&]
	{
		auto *arg = builder->literal(42);
		builder->call(callee_node, { arg });
		builder->ret(nullptr);
	});

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	ASSERT_NE(cg_result, nullptr);

	const auto &call_graph = cg_result->get_call_graph();
	EXPECT_EQ(call_graph.size(), 2);

	auto *caller_cg_node = call_graph.get_node(caller_node);
	auto *callee_cg_node = call_graph.get_node(callee_node);

	ASSERT_NE(caller_cg_node, nullptr);
	ASSERT_NE(callee_cg_node, nullptr);

	EXPECT_TRUE(caller_cg_node->calls(callee_cg_node));
	EXPECT_TRUE(callee_cg_node->called_by(caller_cg_node));

	EXPECT_EQ(caller_cg_node->get_callees().size(), 1);
	EXPECT_EQ(callee_cg_node->get_callers().size(), 1);
	EXPECT_EQ(caller_cg_node->get_call_count(), 1);

	auto entry_points = call_graph.get_entry_points();
	EXPECT_EQ(entry_points.size(), 1);
	EXPECT_EQ(entry_points[0], caller_cg_node);

	auto leaf_functions = call_graph.get_leaf_functions();
	EXPECT_EQ(leaf_functions.size(), 1);
	EXPECT_EQ(leaf_functions[0], callee_cg_node);
}

TEST_F(CallGraphAnalysisFixture, ChainedCallsCallGraph)
{
	auto *module = builder->create_module("test_module");

	/* func_c */
	auto func_c = builder->create_function("func_c", {}, blm::DataType::VOID);
	blm::Node *func_c_node = func_c.get_function();

	func_c.body([&]
	{
		builder->ret(nullptr);
	});

	/* func_b calls func_c */
	auto func_b = builder->create_function("func_b", {}, blm::DataType::VOID);
	blm::Node *func_b_node = func_b.get_function();

	func_b.body([&]
	{
		builder->call(func_c_node, {});
		builder->ret(nullptr);
	});

	/* func_a calls func_b */
	auto func_a = builder->create_function("func_a", {}, blm::DataType::VOID);
	blm::Node *func_a_node = func_a.get_function();

	func_a.body([&]
	{
		builder->call(func_b_node, {});
		builder->ret(nullptr);
	});

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	ASSERT_NE(cg_result, nullptr);

	const auto &call_graph = cg_result->get_call_graph();
	EXPECT_EQ(call_graph.size(), 3);

	auto *cg_a = call_graph.get_node(func_a_node);
	auto *cg_b = call_graph.get_node(func_b_node);
	auto *cg_c = call_graph.get_node(func_c_node);

	ASSERT_NE(cg_a, nullptr);
	ASSERT_NE(cg_b, nullptr);
	ASSERT_NE(cg_c, nullptr);

	EXPECT_TRUE(cg_a->calls(cg_b));
	EXPECT_TRUE(cg_b->calls(cg_c));
	EXPECT_FALSE(cg_a->calls(cg_c)); /* direct call only */

	auto entry_points = call_graph.get_entry_points();
	EXPECT_EQ(entry_points.size(), 1);
	EXPECT_EQ(entry_points[0], cg_a);

	auto leaf_functions = call_graph.get_leaf_functions();
	EXPECT_EQ(leaf_functions.size(), 1);
	EXPECT_EQ(leaf_functions[0], cg_c);

	EXPECT_FALSE(call_graph.has_cycles());
}

TEST_F(CallGraphAnalysisFixture, CyclicCallGraph)
{
	auto *module = builder->create_module("test_module");

	/* forward declare func_b for mutual recursion */
	auto func_b = builder->create_function("func_b", {}, blm::DataType::VOID);
	blm::Node *func_b_node = func_b.get_function();

	/* func_a calls func_b */
	auto func_a = builder->create_function("func_a", {}, blm::DataType::VOID);
	blm::Node *func_a_node = func_a.get_function();

	func_a.body([&]
	{
		builder->call(func_b_node, {});
		builder->ret(nullptr);
	});

	/* func_b calls func_a */
	func_b.body([&]
	{
		builder->call(func_a_node, {});
		builder->ret(nullptr);
	});

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	ASSERT_NE(cg_result, nullptr);

	const auto &call_graph = cg_result->get_call_graph();
	EXPECT_EQ(call_graph.size(), 2);

	auto *cg_a = call_graph.get_node(func_a_node);
	auto *cg_b = call_graph.get_node(func_b_node);

	ASSERT_NE(cg_a, nullptr);
	ASSERT_NE(cg_b, nullptr);

	EXPECT_TRUE(cg_a->calls(cg_b));
	EXPECT_TRUE(cg_b->calls(cg_a));

	EXPECT_TRUE(call_graph.has_cycles());

	auto entry_points = call_graph.get_entry_points();
	EXPECT_EQ(entry_points.size(), 0);
}

TEST_F(CallGraphAnalysisFixture, MultiModuleCallGraph)
{
	auto *module1 = builder->create_module("module1");

	auto func1 = builder->create_function("func1", {}, blm::DataType::VOID);
	blm::Node *func1_node = func1.get_function();

	auto *module2 = context->create_module("module2");
	builder->set_current_module(module2);

	auto func2 = builder->create_function("func2", {}, blm::DataType::VOID);
	blm::Node *func2_node = func2.get_function();

	func2.body([&]
	{
		builder->ret(nullptr);
	});

	/* back to module1, make func1 call func2 */
	builder->set_current_module(module1);
	func1.body([&]
	{
		builder->call(func2_node, {});
		builder->ret(nullptr);
	});

	std::vector modules = { module1, module2 };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	ASSERT_NE(cg_result, nullptr);

	const auto &call_graph = cg_result->get_call_graph();
	EXPECT_EQ(call_graph.size(), 2);

	auto *cg1 = call_graph.get_node(func1_node);
	auto *cg2 = call_graph.get_node(func2_node);

	ASSERT_NE(cg1, nullptr);
	ASSERT_NE(cg2, nullptr);

	EXPECT_TRUE(cg1->calls(cg2));

	auto deps = cg_result->depends_on_modules();
	EXPECT_EQ(deps.size(), 2);
	EXPECT_TRUE(deps.contains(module1));
	EXPECT_TRUE(deps.contains(module2));
}

TEST_F(CallGraphAnalysisFixture, TopologicalOrderCallGraph)
{
	auto *module = builder->create_module("test_module");

	/* main -> func1 -> func2 -> func3 */
	auto func3 = builder->create_function("func3", {}, blm::DataType::VOID);
	blm::Node *func3_node = func3.get_function();

	func3.body([&]
	{
		builder->ret(nullptr);
	});

	auto func2 = builder->create_function("func2", {}, blm::DataType::VOID);
	blm::Node *func2_node = func2.get_function();

	func2.body([&]
	{
		builder->call(func3_node, {});
		builder->ret(nullptr);
	});

	auto func1 = builder->create_function("func1", {}, blm::DataType::VOID);
	blm::Node *func1_node = func1.get_function();

	func1.body([&]
	{
		builder->call(func2_node, {});
		builder->ret(nullptr);
	});

	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();

	main_func.body([&]
	{
		builder->call(func1_node, {});
		builder->ret(nullptr);
	});

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	ASSERT_NE(cg_result, nullptr);

	const auto &call_graph = cg_result->get_call_graph();
	EXPECT_EQ(call_graph.size(), 4);

	auto post_order = call_graph.get_post_order();
	EXPECT_EQ(post_order.size(), 4);

	EXPECT_EQ(post_order[0]->get_function(), func3_node);
	EXPECT_EQ(post_order[3]->get_function(), main_node);

	auto reverse_post_order = call_graph.get_reverse_post_order();
	EXPECT_EQ(reverse_post_order.size(), 4);
	EXPECT_EQ(reverse_post_order[0]->get_function(), main_node);
	EXPECT_EQ(reverse_post_order[3]->get_function(), func3_node);
}

TEST_F(CallGraphAnalysisFixture, StatisticsCollection)
{
	auto *module = builder->create_module("test_module");
	auto func1 = builder->create_function("func1", {}, blm::DataType::VOID);

	auto func2 = builder->create_function("func2", {}, blm::DataType::VOID);
	blm::Node *func2_node = func2.get_function();

	auto func3 = builder->create_function("func3", {}, blm::DataType::VOID);
	blm::Node *func3_node = func3.get_function();

	func3.body([&]
	{
		builder->ret(nullptr);
	});

	func2.body([&]
	{
		builder->call(func3_node, {});
		builder->ret(nullptr);
	});

	func1.body([&]
	{
		builder->call(func2_node, {});
		builder->ret(nullptr);
	});

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);
	blm::CallGraphAnalysisPass cg_pass;

	bool result = cg_pass.run(modules, ipo_ctx);
	EXPECT_TRUE(result);

	EXPECT_EQ(ipo_ctx.get_stat("callgraph.functions_analyzed"), 3);
	EXPECT_GE(ipo_ctx.get_stat("callgraph.global_functions"), 0);
	EXPECT_EQ(ipo_ctx.get_stat("callgraph.total_edges"), 2); /* func1->func2, func2->func3 */
}
