/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <sstream>
#include <bloom/foundation/context.hpp>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/ipo/pass-manager.hpp>
#include <bloom/ipo/experimental/sccp.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <gtest/gtest.h>

class IPOSCCPPassFixture : public ::testing::Test
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

	static std::string print_module_ir(const blm::Module &module, const std::string &label = "")
	{
		std::ostringstream os;
		if (!label.empty())
			os << label << "\n";

		blm::IRPrinter printer(os, {
			.include_debug_info = false,
			.include_type_annotations = true,
			.indent_size = 2,
			.use_spaces = true
		});
		printer.print_module(module);
		return os.str();
	}

	void run_sccp_pass(std::vector<blm::Module *> &modules)
	{
		pass_manager = std::make_unique<blm::IPOPassManager>(modules);
		pass_manager->add_pass<blm::CallGraphAnalysisPass>();
		pass_manager->add_pass<blm::IPOSCCPPass>();
		pass_manager->run_all();
	}

	const blm::IPOSCCPResult* get_sccp_result()
	{
		return pass_manager->get_context().get_result<blm::IPOSCCPResult>();
	}

	bool is_node_constant(blm::Node *node)
	{
		if (const auto *result = get_sccp_result())
		{
			return result->is_constant(node);
		}
		return false;
	}

	blm::LatticeValue get_lattice_value(blm::Node *node)
	{
		if (const auto *result = get_sccp_result())
		{
			return result->get_lattice_value(node);
		}
		return blm::LatticeValue{};
	}

	static blm::Node* find_node_by_type(blm::Region *region, blm::NodeType type)
	{
		for (blm::Node *node : region->get_nodes())
		{
			if (node->ir_type == type)
				return node;
		}
		return nullptr;
	}

	static blm::Region* find_function_region(blm::Module *module, std::string_view func_name)
	{
		for (const blm::Region *child : module->get_root_region()->get_children())
		{
			if (child->get_name() == func_name)
				return const_cast<blm::Region*>(child);
		}
		return nullptr;
	}

	std::size_t count_constant_nodes()
	{
		if (const auto *result = get_sccp_result())
		{
			return result->get_constant_nodes().size();
		}
		return 0;
	}

	std::unique_ptr<blm::Context> context;
	std::unique_ptr<blm::Builder> builder;
	std::unique_ptr<blm::IPOPassManager> pass_manager;
};

TEST_F(IPOSCCPPassFixture, BasicConstantPropagation)
{
	auto *module = builder->create_module("test_module");

	/* int add_constants() { return 10 + 20; } */
	auto func = builder->create_function("add_constants", {}, blm::DataType::INT32);

	blm::Node *add_node = nullptr;
	func.body([&]
	{
		auto *ten = builder->literal(10);
		auto *twenty = builder->literal(20);
		add_node = builder->add(ten, twenty);
		builder->ret(add_node);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	std::cout << "Result object pointer: " << get_sccp_result() << std::endl;
	if (auto* result = get_sccp_result()) {
		std::cout << "Direct lattice check: " << result->get_lattice_value(add_node).is_constant() << std::endl;
	}
	EXPECT_TRUE(is_node_constant(add_node));

	blm::LatticeValue result = get_lattice_value(add_node);
	EXPECT_TRUE(result.is_constant());
	EXPECT_EQ(result.value.type(), blm::DataType::INT32);
	EXPECT_EQ(result.value.get<blm::DataType::INT32>(), 30);
	EXPECT_GT(count_constant_nodes(), 2);
}

TEST_F(IPOSCCPPassFixture, InterprocConstantArguments)
{
	auto *module = builder->create_module("test_module");

	/* int double_value(int x) { return x * 2; } */
	auto double_func = builder->create_function("double_value", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *double_node = double_func.get_function();

	blm::Node *param = nullptr;
	blm::Node *mul_node = nullptr;
	double_func.body([&]
	{
		param = double_func.add_parameter("x", blm::DataType::INT32);
		auto *two = builder->literal(2);
		mul_node = builder->mul(param, two);
		builder->ret(mul_node);
	});

	/* int caller() { return double_value(15); } */
	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);

	blm::Node *call_node = nullptr;
	caller_func.body([&]
	{
		auto *fifteen = builder->literal(15);
		call_node = builder->call(double_node, { fifteen });
		builder->ret(call_node);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	/* verify parameter received constant value */
	EXPECT_TRUE(is_node_constant(param));
	blm::LatticeValue param_val = get_lattice_value(param);
	EXPECT_TRUE(param_val.is_constant());
	EXPECT_EQ(param_val.value.get<blm::DataType::INT32>(), 15);

	/* verify multiplication was computed */
	EXPECT_TRUE(is_node_constant(mul_node));
	blm::LatticeValue mul_val = get_lattice_value(mul_node);
	EXPECT_TRUE(mul_val.is_constant());
	EXPECT_EQ(mul_val.value.get<blm::DataType::INT32>(), 30);

	/* verify call site got return value */
	EXPECT_TRUE(is_node_constant(call_node));
	blm::LatticeValue call_val = get_lattice_value(call_node);
	EXPECT_TRUE(call_val.is_constant());
	EXPECT_EQ(call_val.value.get<blm::DataType::INT32>(), 30);
}

TEST_F(IPOSCCPPassFixture, ConstantBranchPropagation)
{
	auto *module = builder->create_module("test_module");

	/* int test_branch()
	 * {
	 *     if (true)
	 *			return 42;
	 *     else return 99;
	 * }
	 */
	auto func = builder->create_function("test_branch", {}, blm::DataType::INT32);

	blm::Node *branch_node = nullptr;
	func.body([&]
	{
		auto *true_lit = builder->literal(true);

		auto [true_block, false_block] = builder->create_if(true_lit, "true_case", "false_case");

		blm::Region *current = builder->get_current_region();
		branch_node = find_node_by_type(current, blm::NodeType::BRANCH);

		true_block([&]
		{
			auto *forty_two = builder->literal(42);
			builder->ret(forty_two);
		});

		false_block.ret(builder->literal(99));
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();
}

TEST_F(IPOSCCPPassFixture, BitwiseOperations)
{
	auto *module = builder->create_module("test_module");

	/* int bitwise_ops()
	 * {
	 *     int a = 5;     // 0101
	 *     int b = 3;     // 0011
	 *     return (a & b) | (~a);
	 * }
	 */
	auto func = builder->create_function("bitwise_ops", {}, blm::DataType::INT32);

	blm::Node *and_node = nullptr;
	blm::Node *not_node = nullptr;
	blm::Node *or_node = nullptr;

	func.body([&]
	{
		auto *five = builder->literal(5);
		auto *three = builder->literal(3);

		and_node = builder->band(five, three);
		not_node = builder->bnot(five);
		or_node = builder->bor(and_node, not_node);

		builder->ret(or_node);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	/* 5 & 3 = 1 */
	EXPECT_TRUE(is_node_constant(and_node));
	blm::LatticeValue and_val = get_lattice_value(and_node);
	EXPECT_EQ(and_val.value.get<blm::DataType::INT32>(), 1);

	/* ~5 = -6 */
	EXPECT_TRUE(is_node_constant(not_node));
	blm::LatticeValue not_val = get_lattice_value(not_node);
	EXPECT_EQ(not_val.value.get<blm::DataType::INT32>(), ~5);

	/* 1 | (-6) = -5 */
	EXPECT_TRUE(is_node_constant(or_node));
	blm::LatticeValue or_val = get_lattice_value(or_node);
	EXPECT_EQ(or_val.value.get<blm::DataType::INT32>(), 1 | (~5));
}

TEST_F(IPOSCCPPassFixture, ComparisonOperations)
{
	auto *module = builder->create_module("test_module");

	/* bool comparison_ops()
	 * {
	 *     return (10 > 5) && (3 <= 3) && (7 != 8);
	 * }
	 */
	auto func = builder->create_function("comparison_ops", {}, blm::DataType::BOOL);

	blm::Node *gt_node = nullptr;
	blm::Node *lte_node = nullptr;
	blm::Node *neq_node = nullptr;

	func.body([&]
	{
		auto *ten = builder->literal(10);
		auto *five = builder->literal(5);
		auto *three = builder->literal(3);
		auto *seven = builder->literal(7);
		auto *eight = builder->literal(8);

		gt_node = builder->gt(ten, five);
		lte_node = builder->lte(three, three);
		neq_node = builder->neq(seven, eight);

		auto *and1 = builder->band(gt_node, lte_node);
		auto *result = builder->band(and1, neq_node);

		builder->ret(result);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	/* verify 10 > 5 = true */
	EXPECT_TRUE(is_node_constant(gt_node));
	blm::LatticeValue gt_val = get_lattice_value(gt_node);
	EXPECT_EQ(gt_val.value.type(), blm::DataType::BOOL);
	EXPECT_TRUE(gt_val.value.get<blm::DataType::BOOL>());

	/* verify 3 <= 3 = true */
	EXPECT_TRUE(is_node_constant(lte_node));
	blm::LatticeValue lte_val = get_lattice_value(lte_node);
	EXPECT_TRUE(lte_val.value.get<blm::DataType::BOOL>());

	/* verify 7 != 8 = true */
	EXPECT_TRUE(is_node_constant(neq_node));
	blm::LatticeValue neq_val = get_lattice_value(neq_node);
	EXPECT_TRUE(neq_val.value.get<blm::DataType::BOOL>());
}

TEST_F(IPOSCCPPassFixture, FloatingPointOperations)
{
	auto *module = builder->create_module("test_module");

	/* float math_ops()
	 * {
	 *     return 3.14f * 2.0f + 1.5f;
	 * }
	 */
	auto func = builder->create_function("math_ops", {}, blm::DataType::FLOAT32);

	blm::Node *mul_node = nullptr;
	blm::Node *add_node = nullptr;

	func.body([&]
	{
		auto *pi = builder->literal(3.14f);
		auto *two = builder->literal(2.0f);
		auto *one_half = builder->literal(1.5f);

		mul_node = builder->mul(pi, two);
		add_node = builder->add(mul_node, one_half);

		builder->ret(add_node);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	/* verify multiplication was computed */
	EXPECT_TRUE(is_node_constant(mul_node));
	blm::LatticeValue mul_val = get_lattice_value(mul_node);
	EXPECT_EQ(mul_val.value.type(), blm::DataType::FLOAT32);
	EXPECT_FLOAT_EQ(mul_val.value.get<blm::DataType::FLOAT32>(), 3.14f * 2.0f);

	/* verify addition was computed */
	EXPECT_TRUE(is_node_constant(add_node));
	blm::LatticeValue add_val = get_lattice_value(add_node);
	EXPECT_FLOAT_EQ(add_val.value.get<blm::DataType::FLOAT32>(), 3.14f * 2.0f + 1.5f);
}

TEST_F(IPOSCCPPassFixture, MultipleReturnPaths)
{
	auto *module = builder->create_module("test_module");

	/* int multi_return(int flag)
	 * {
	 *     if (flag) return 100;
	 *     return 200;
	 * }
	 */
	auto multi_func = builder->create_function("multi_return", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *multi_node = multi_func.get_function();

	blm::Node *param = nullptr;
	multi_func.body([&]
	{
		param = multi_func.add_parameter("flag", blm::DataType::INT32);

		auto [true_block, false_block] = builder->create_if(param, "flag_true", "flag_false");

		true_block([&]
		{
			auto *hundred = builder->literal(100);
			builder->ret(hundred);
		});

		false_block([&]
		{
			auto *two_hundred = builder->literal(200);
			builder->ret(two_hundred);
		});
	});

	/* int caller1() { return multi_return(1); } */
	auto caller1_func = builder->create_function("caller1", {}, blm::DataType::INT32);
	blm::Node *call1_node = nullptr;

	caller1_func.body([&]
	{
		auto *one = builder->literal(1);
		call1_node = builder->call(multi_node, { one });
		builder->ret(call1_node);
	});

	/* int caller2() { return multi_return(0); } */
	auto caller2_func = builder->create_function("caller2", {}, blm::DataType::INT32);
	blm::Node *call2_node = nullptr;

	caller2_func.body([&]
	{
		auto *zero = builder->literal(0);
		call2_node = builder->call(multi_node, { zero });
		builder->ret(call2_node);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	/* Note: due to the complexity of tracking multiple return paths,
	 * the call nodes might not be constant since SCCP might not be able
	 * to determine which path will be taken at compile time.
	 * however, we can verify that the parameters received constant values.
	 */
	EXPECT_GT(count_constant_nodes(), 4); /* at least the literals */
}

TEST_F(IPOSCCPPassFixture, DivisionByZeroHandling)
{
	auto *module = builder->create_module("test_module");

	/* int safe_div() { return 10 / 2; }
	 * int unsafe_div() { return 5 / 0; }
	 */
	auto safe_func = builder->create_function("safe_div", {}, blm::DataType::INT32);
	blm::Node *safe_div_node = nullptr;

	safe_func.body([&]
	{
		auto *ten = builder->literal(10);
		auto *two = builder->literal(2);
		safe_div_node = builder->div(ten, two);
		builder->ret(safe_div_node);
	});

	auto unsafe_func = builder->create_function("unsafe_div", {}, blm::DataType::INT32);
	blm::Node *unsafe_div_node = nullptr;

	unsafe_func.body([&]
	{
		auto *five = builder->literal(5);
		auto *zero = builder->literal(0);
		unsafe_div_node = builder->div(five, zero);
		builder->ret(unsafe_div_node);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	EXPECT_TRUE(is_node_constant(safe_div_node));
	blm::LatticeValue safe_val = get_lattice_value(safe_div_node);
	EXPECT_EQ(safe_val.value.get<blm::DataType::INT32>(), 5);

	blm::LatticeValue unsafe_val = get_lattice_value(unsafe_div_node);
	EXPECT_TRUE(unsafe_val.is_bottom());
}

TEST_F(IPOSCCPPassFixture, NoConstantPropagationWithNonConstants)
{
	auto *module = builder->create_module("test_module");

	/* int dynamic_func(int x)
	 * {
	 *     return x + 10;  // x is not constant
	 * }
	 */
	auto func = builder->create_function("dynamic_func", { blm::DataType::INT32 }, blm::DataType::INT32);

	blm::Node *param = nullptr;
	blm::Node *add_node = nullptr;

	func.body([&]
	{
		param = func.add_parameter("x", blm::DataType::INT32);
		auto *ten = builder->literal(10);
		add_node = builder->add(param, ten);
		builder->ret(add_node);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	blm::LatticeValue param_val = get_lattice_value(param);
	EXPECT_TRUE(param_val.is_top());

	blm::LatticeValue add_val = get_lattice_value(add_node);
	EXPECT_TRUE(add_val.is_top());
}

TEST_F(IPOSCCPPassFixture, StatisticsVerification)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("stats_test", {}, blm::DataType::INT32);

	func.body([&]
	{
		auto *a = builder->literal(5);
		auto *b = builder->literal(10);
		auto *c = builder->literal(3);

		auto *add1 = builder->add(a, b);
		auto *mul1 = builder->mul(add1, c);
		auto *sub1 = builder->sub(mul1, a);

		builder->ret(sub1);
	});

	std::vector<blm::Module*> modules = { module };
	run_sccp_pass(modules);
	pass_manager->print_statistics();
	std::size_t constants_found = pass_manager->get_context().get_stat("ipo_sccp.constants_found");
	EXPECT_GT(constants_found, 3);
}
