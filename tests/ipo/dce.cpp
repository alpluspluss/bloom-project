/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/ipo/dce.hpp>
#include <bloom/ipo/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <gtest/gtest.h>

class IPODCEPassFixture : public ::testing::Test
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

TEST_F(IPODCEPassFixture, NoUnreachableFunctions)
{
	auto *module = builder->create_module("test_module");

	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();
	main_node->props |= blm::NodeProps::DRIVER;

	main_func.body([&]
	{
		builder->ret(nullptr);
	});

	std::vector<blm::Module *> modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);

	blm::CallGraphAnalysisPass cg_pass;
	cg_pass.run(modules, ipo_ctx);
	blm::IPODCEPass dce_pass;

	bool result = dce_pass.run(modules, ipo_ctx);
	EXPECT_FALSE(result);
	EXPECT_EQ(module->get_functions().size(), 1);
	EXPECT_EQ(ipo_ctx.get_stat("ipo_dce.removed_functions"), 0);
}

TEST_F(IPODCEPassFixture, RemoveUnreachableFunction)
{
	auto *module = builder->create_module("test_module");

	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();
	main_node->props |= blm::NodeProps::DRIVER;

	main_func.body([&]
	{
		builder->ret(nullptr);
	});

	/* create unreachable function */
	auto dead_func = builder->create_function("dead_function", {}, blm::DataType::VOID);
	dead_func.body([&]
	{
		builder->ret(nullptr);
	});

	EXPECT_EQ(module->get_functions().size(), 2);

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);

	blm::CallGraphAnalysisPass cg_pass;
	cg_pass.run(modules, ipo_ctx);

	blm::IPODCEPass dce_pass;
	bool result = dce_pass.run(modules, ipo_ctx);

	EXPECT_TRUE(result);
	EXPECT_EQ(module->get_functions().size(), 1);
	EXPECT_EQ(ipo_ctx.get_stat("ipo_dce.removed_functions"), 1);

	bool found_main = false;
	for (blm::Node *func: module->get_functions())
	{
		if (module->get_context().get_string(func->str_id) == "main")
		{
			found_main = true;
			break;
		}
	}
	EXPECT_TRUE(found_main);
}

TEST_F(IPODCEPassFixture, PreserveCalledFunctions)
{
	auto *module = builder->create_module("test_module");

	auto helper_func = builder->create_function("helper", {}, blm::DataType::INT32);
	blm::Node *helper_node = helper_func.get_function();

	helper_func.body([&]
	{
		auto *value = builder->literal(42);
		builder->ret(value);
	});

	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();
	main_node->props |= blm::NodeProps::DRIVER;

	main_func.body([&]
	{
		builder->call(helper_node, {});
		builder->ret(nullptr);
	});

	auto dead_func = builder->create_function("dead_function", {}, blm::DataType::VOID);
	dead_func.body([&]
	{
		builder->ret(nullptr);
	});

	EXPECT_EQ(module->get_functions().size(), 3);

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);

	blm::CallGraphAnalysisPass cg_pass;
	cg_pass.run(modules, ipo_ctx);

	blm::IPODCEPass dce_pass;
	bool result = dce_pass.run(modules, ipo_ctx);

	EXPECT_TRUE(result);
	EXPECT_EQ(module->get_functions().size(), 2);
	EXPECT_EQ(ipo_ctx.get_stat("ipo_dce.removed_functions"), 1);

	std::unordered_set<std::string> remaining_functions;
	for (blm::Node *func: module->get_functions())
		remaining_functions.insert(std::string(module->get_context().get_string(func->str_id)));

	EXPECT_TRUE(remaining_functions.contains("main"));
	EXPECT_TRUE(remaining_functions.contains("helper"));
	EXPECT_FALSE(remaining_functions.contains("dead_function"));
}

TEST_F(IPODCEPassFixture, PreserveExportedFunctions)
{
	auto *module = builder->create_module("test_module");

	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();
	main_node->props |= blm::NodeProps::DRIVER;

	main_func.body([&]
	{
		builder->ret(nullptr);
	});

	auto exported_func = builder->create_function("exported_api", {}, blm::DataType::INT32);
	blm::Node *exported_node = exported_func.get_function();
	exported_node->props |= blm::NodeProps::EXPORT;

	exported_func.body([&]
	{
		auto *value = builder->literal(123);
		builder->ret(value);
	});

	auto dead_func = builder->create_function("dead_function", {}, blm::DataType::VOID);
	dead_func.body([&]
	{
		builder->ret(nullptr);
	});

	EXPECT_EQ(module->get_functions().size(), 3);

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);

	blm::CallGraphAnalysisPass cg_pass;
	cg_pass.run(modules, ipo_ctx);

	blm::IPODCEPass dce_pass;
	bool result = dce_pass.run(modules, ipo_ctx);

	EXPECT_TRUE(result);
	EXPECT_EQ(module->get_functions().size(), 2);
	EXPECT_EQ(ipo_ctx.get_stat("ipo_dce.removed_functions"), 1);

	std::unordered_set<std::string> remaining_functions;
	for (blm::Node *func: module->get_functions())
		remaining_functions.insert(std::string(module->get_context().get_string(func->str_id)));

	EXPECT_TRUE(remaining_functions.contains("main"));
	EXPECT_TRUE(remaining_functions.contains("exported_api"));
	EXPECT_FALSE(remaining_functions.contains("dead_function"));
}

TEST_F(IPODCEPassFixture, CrossModuleDCE)
{
	auto *module1 = builder->create_module("module1");

	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();
	main_node->props |= blm::NodeProps::DRIVER;

	auto *module2 = context->create_module("module2");
	builder->set_current_module(module2);

	auto used_func = builder->create_function("used_function", {}, blm::DataType::INT32);
	blm::Node *used_node = used_func.get_function();

	used_func.body([&]
	{
		auto *value = builder->literal(99);
		builder->ret(value);
	});

	auto dead_func = builder->create_function("dead_function", {}, blm::DataType::VOID);
	dead_func.body([&]
	{
		builder->ret(nullptr);
	});

	builder->set_current_module(module1);
	main_func.body([&]
	{
		builder->call(used_node, {});
		builder->ret(nullptr);
	});

	EXPECT_EQ(module1->get_functions().size(), 1);
	EXPECT_EQ(module2->get_functions().size(), 2);

	std::vector modules = { module1, module2 };
	blm::IPOPassContext ipo_ctx(modules, 1);

	blm::CallGraphAnalysisPass cg_pass;
	cg_pass.run(modules, ipo_ctx);

	blm::IPODCEPass dce_pass;
	bool result = dce_pass.run(modules, ipo_ctx);

	EXPECT_TRUE(result);
	EXPECT_EQ(module1->get_functions().size(), 1);
	EXPECT_EQ(module2->get_functions().size(), 1);
	EXPECT_EQ(ipo_ctx.get_stat("ipo_dce.removed_functions"), 1);
	EXPECT_EQ(module1->get_context().get_string(module1->get_functions()[0]->str_id), "main");
	EXPECT_EQ(module2->get_context().get_string(module2->get_functions()[0]->str_id), "used_function");
}

TEST_F(IPODCEPassFixture, ChainedCallsPreservation)
{
	auto *module = builder->create_module("test_module");

	/* main -> func1 -> func2 -> func3 */
	auto func3 = builder->create_function("func3", {}, blm::DataType::INT32);
	blm::Node *func3_node = func3.get_function();

	func3.body([&]
	{
		auto *value = builder->literal(42);
		builder->ret(value);
	});

	auto func2 = builder->create_function("func2", {}, blm::DataType::INT32);
	blm::Node *func2_node = func2.get_function();

	func2.body([&]
	{
		auto *result = builder->call(func3_node, {});
		builder->ret(result);
	});

	auto func1 = builder->create_function("func1", {}, blm::DataType::INT32);
	blm::Node *func1_node = func1.get_function();

	func1.body([&]
	{
		auto *result = builder->call(func2_node, {});
		builder->ret(result);
	});

	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();
	main_node->props |= blm::NodeProps::DRIVER;

	main_func.body([&]
	{
		builder->call(func1_node, {});
		builder->ret(nullptr);
	});

	auto dead_func = builder->create_function("dead_function", {}, blm::DataType::VOID);
	dead_func.body([&]
	{
		builder->ret(nullptr);
	});

	EXPECT_EQ(module->get_functions().size(), 5);

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);

	blm::CallGraphAnalysisPass cg_pass;
	cg_pass.run(modules, ipo_ctx);

	blm::IPODCEPass dce_pass;
	bool result = dce_pass.run(modules, ipo_ctx);

	EXPECT_TRUE(result);
	EXPECT_EQ(module->get_functions().size(), 4);
	EXPECT_EQ(ipo_ctx.get_stat("ipo_dce.removed_functions"), 1);

	std::unordered_set<std::string> remaining_functions;
	for (blm::Node *func: module->get_functions())
		remaining_functions.insert(std::string(module->get_context().get_string(func->str_id)));

	EXPECT_TRUE(remaining_functions.contains("main"));
	EXPECT_TRUE(remaining_functions.contains("func1"));
	EXPECT_TRUE(remaining_functions.contains("func2"));
	EXPECT_TRUE(remaining_functions.contains("func3"));
	EXPECT_FALSE(remaining_functions.contains("dead_function"));
}

TEST_F(IPODCEPassFixture, AutoRunCallGraphAnalysis)
{
	auto *module = builder->create_module("test_module");
	auto main_func = builder->create_function("main", {}, blm::DataType::VOID);
	blm::Node *main_node = main_func.get_function();
	main_node->props |= blm::NodeProps::DRIVER;

	main_func.body([&]
	{
		builder->ret(nullptr);
	});

	auto dead_func = builder->create_function("dead_function", {}, blm::DataType::VOID);
	dead_func.body([&]
	{
		builder->ret(nullptr);
	});

	std::vector modules = { module };
	blm::IPOPassContext ipo_ctx(modules, 1);

	blm::IPODCEPass dce_pass;
	bool result = dce_pass.run(modules, ipo_ctx);

	EXPECT_TRUE(result);
	EXPECT_EQ(module->get_functions().size(), 1);
	EXPECT_EQ(ipo_ctx.get_stat("ipo_dce.removed_functions"), 1);

	const auto *cg_result = ipo_ctx.get_result<blm::CallGraphResult>();
	EXPECT_NE(cg_result, nullptr);
}
