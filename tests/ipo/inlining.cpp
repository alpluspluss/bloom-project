/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <sstream>
#include <bloom/foundation/context.hpp>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/ipo/inlining.hpp>
#include <bloom/ipo/pass-manager.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <gtest/gtest.h>

class IPOInliningPassFixture : public ::testing::Test
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

	std::string print_module_ir(const blm::Module &module, const std::string &label = "")
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

	void run_inlining_pass(std::vector<blm::Module *> &modules)
	{
		pass_manager = std::make_unique<blm::IPOPassManager>(modules);
		pass_manager->add_pass<blm::CallGraphAnalysisPass>();
		pass_manager->add_pass<blm::IPOInliningPass>();
		pass_manager->run_all();
	}

	blm::Region *find_inlined_region(blm::Module *module)
	{
		for (const blm::Region *child: module->get_root_region()->get_children())
		{
			if (child->get_name().find("inlined") != std::string_view::npos)
				return const_cast<blm::Region *>(child);
		}
		return nullptr;
	}

	blm::Region *find_region_by_name(blm::Module *module, std::string_view name)
	{
		for (const blm::Region *child: module->get_root_region()->get_children())
		{
			if (child->get_name() == name)
				return const_cast<blm::Region *>(child);
		}
		return nullptr;
	}

	bool has_literal_value(blm::Region *region, std::int32_t value)
	{
		for (blm::Node *node: region->get_nodes())
		{
			if (node->ir_type == blm::NodeType::LIT &&
			    node->type_kind == blm::DataType::INT32 &&
			    node->as<blm::DataType::INT32>() == value)
			{
				return true;
			}
		}
		return false;
	}

	std::size_t count_nodes_of_type(blm::Region *region, blm::NodeType type)
	{
		std::size_t count = 0;
		for (blm::Node *node: region->get_nodes())
		{
			if (node->ir_type == type)
				count++;
		}
		return count;
	}

	std::unique_ptr<blm::Context> context;
	std::unique_ptr<blm::Builder> builder;
	std::unique_ptr<blm::IPOPassManager> pass_manager;
};

TEST_F(IPOInliningPassFixture, ParameterSubstitutionCorrectness)
{
	auto *module = builder->create_module("test_module");

	/* int complex_func(int a, int b, int c) { return (a + b) * c; } */
	auto complex_func = builder->create_function("complex_func",
	                                             { blm::DataType::INT32, blm::DataType::INT32, blm::DataType::INT32 },
	                                             blm::DataType::INT32);
	blm::Node *complex_node = complex_func.get_function();

	blm::Node *param_a = nullptr;
	blm::Node *param_b = nullptr;
	blm::Node *param_c = nullptr;
	complex_func.body([&]
	{
		param_a = complex_func.add_parameter("a", blm::DataType::INT32);
		param_b = complex_func.add_parameter("b", blm::DataType::INT32);
		param_c = complex_func.add_parameter("c", blm::DataType::INT32);

		auto *sum = builder->add(param_a, param_b);
		auto *result = builder->mul(sum, param_c);
		builder->ret(result);
	});

	/* int caller() { return complex_func(10, 20, 3); } */
	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);
	blm::Node *call_site = nullptr;

	caller_func.body([&]
	{
		auto *arg_10 = builder->literal(10);
		auto *arg_20 = builder->literal(20);
		auto *arg_3 = builder->literal(3);
		call_site = builder->call(complex_node, { arg_10, arg_20, arg_3 });
		builder->ret(call_site);
	});

	std::string ir_before = print_module_ir(*module, "before");
	std::cout << ir_before << std::endl;

	std::vector modules = { module };
	run_inlining_pass(modules);

	std::string ir_after = print_module_ir(*module, "after");
	std::cout << ir_after << std::endl;

	pass_manager->print_statistics();

	blm::Region *inlined_region = find_inlined_region(module);
	ASSERT_NE(inlined_region, nullptr);

	EXPECT_EQ(count_nodes_of_type(inlined_region, blm::NodeType::PARAM), 0);

	EXPECT_TRUE(has_literal_value(inlined_region, 10));
	EXPECT_TRUE(has_literal_value(inlined_region, 20));
	EXPECT_TRUE(has_literal_value(inlined_region, 3));

	bool found_add_with_literals = false;
	bool found_mul_with_literals = false;

	for (blm::Node *node: inlined_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::ADD && node->inputs.size() == 2)
		{
			blm::Node *lhs = node->inputs[0];
			blm::Node *rhs = node->inputs[1];

			if (lhs->ir_type == blm::NodeType::LIT && rhs->ir_type == blm::NodeType::LIT)
			{
				std::int32_t lhs_val = lhs->as<blm::DataType::INT32>();
				std::int32_t rhs_val = rhs->as<blm::DataType::INT32>();

				if ((lhs_val == 10 && rhs_val == 20) || (lhs_val == 20 && rhs_val == 10))
					found_add_with_literals = true;
			}
		}

		if (node->ir_type == blm::NodeType::MUL && node->inputs.size() == 2)
		{
			for (blm::Node *input: node->inputs)
			{
				if (input->ir_type == blm::NodeType::LIT)
				{
					std::int32_t val = input->as<blm::DataType::INT32>();
					if (val == 3)
					{
						found_mul_with_literals = true;
						break;
					}
				}
			}
		}
	}
	EXPECT_TRUE(found_add_with_literals);
	EXPECT_TRUE(found_mul_with_literals);
}

TEST_F(IPOInliningPassFixture, ParameterSubstitutionOrder)
{
	auto *module = builder->create_module("test_module");

	/* int subtract(int first, int second) { return first - second; } */
	auto subtract_func = builder->create_function("subtract",
	                                              { blm::DataType::INT32, blm::DataType::INT32 },
	                                              blm::DataType::INT32);
	blm::Node *subtract_node = subtract_func.get_function();

	subtract_func.body([&]
	{
		auto *param_first = subtract_func.add_parameter("first", blm::DataType::INT32);
		auto *param_second = subtract_func.add_parameter("second", blm::DataType::INT32);
		auto *result = builder->sub(param_first, param_second); /* order matters! */
		builder->ret(result);
	});

	/* int test() { return subtract(100, 30); } */
	auto test_func = builder->create_function("test", {}, blm::DataType::INT32);

	test_func.body([&]
	{
		auto *arg_100 = builder->literal(100);
		auto *arg_30 = builder->literal(30);
		auto *call_result = builder->call(subtract_node, { arg_100, arg_30 });
		builder->ret(call_result);
	});

	/* print IR before inlining */
	std::string ir_before = print_module_ir(*module, "before");
	std::cout << ir_before << std::endl;

	std::vector<blm::Module *> modules = { module };
	run_inlining_pass(modules);

	const std::string ir_after = print_module_ir(*module, "after");
	std::cout << ir_after << std::endl;

	pass_manager->print_statistics();

	blm::Region *inlined_region = find_inlined_region(module);
	ASSERT_NE(inlined_region, nullptr);

	bool found_correct_subtraction = false;
	for (const blm::Node *node: inlined_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::SUB && node->inputs.size() == 2)
		{
			blm::Node *lhs = node->inputs[0];
			if (blm::Node *rhs = node->inputs[1];
				lhs->ir_type == blm::NodeType::LIT && rhs->ir_type == blm::NodeType::LIT)
			{
				std::int32_t lhs_val = lhs->as<blm::DataType::INT32>();
				std::int32_t rhs_val = rhs->as<blm::DataType::INT32>();

				if (lhs_val == 100 && rhs_val == 30)
					found_correct_subtraction = true;
				EXPECT_FALSE(lhs_val == 30 && rhs_val == 100);
			}
		}
	}

	EXPECT_TRUE(found_correct_subtraction);
}

TEST_F(IPOInliningPassFixture, LargeFunctionNotInlined)
{
	auto *module = builder->create_module("test_module");

	auto large_func = builder->create_function("large_function", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *large_node = large_func.get_function();

	large_func.body([&]
	{
		auto *param = large_func.add_parameter("x", blm::DataType::INT32);
		blm::Node *current = param;
		for (int i = 0; i < 100; ++i)
		{
			auto *one = builder->literal(1);
			current = builder->add(current, one);
		}

		builder->ret(current);
	});

	auto test_func = builder->create_function("test", {}, blm::DataType::INT32);

	test_func.body([&]
	{
		auto *five = builder->literal(5);
		auto *result = builder->call(large_node, { five });
		builder->ret(result);
	});

	std::string ir_before = print_module_ir(*module, "befpre");
	std::cout << ir_before << std::endl;

	std::vector modules = { module };
	run_inlining_pass(modules);

	std::string ir_after = print_module_ir(*module, "after");
	std::cout << ir_after << std::endl;
	pass_manager->print_statistics();

	EXPECT_EQ(pass_manager->get_context().get_stat("ipo_inlining.optimized_calls"), 1);
	/* only specialized, not inlined */

	blm::Region *test_region = find_region_by_name(module, "test");
	ASSERT_NE(test_region, nullptr);
	EXPECT_GT(count_nodes_of_type(test_region, blm::NodeType::CALL), 0);
}

TEST_F(IPOInliningPassFixture, NoInlineableTargets)
{
	auto *module = builder->create_module("test_module");
	auto recursive_func = builder->create_function("recursive", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *recursive_node = recursive_func.get_function();

	recursive_func.body([&]
	{
		auto *param = recursive_func.add_parameter("n", blm::DataType::INT32);
		auto *zero = builder->literal(0);
		auto *condition = builder->gt(param, zero);

		auto [true_block, false_block] = builder->create_if(condition, "recursive_case", "base_case");

		true_block([&]
		{
			auto *one = builder->literal(1);
			auto *n_minus_1 = builder->sub(param, one);
			auto *recursive_call = builder->call(recursive_node, { n_minus_1 });
			builder->ret(recursive_call);
		});

		false_block.ret(zero);
	});

	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);
	caller_func.body([&]
	{
		auto *five = builder->literal(5);
		auto *result = builder->call(recursive_node, { five });
		builder->ret(result);
	});

	std::cout << print_module_ir(*module, "before") << "\n";
	std::vector modules = { module };
	run_inlining_pass(modules);
	std::cout << print_module_ir(*module, "after") << "\n";

	EXPECT_EQ(pass_manager->get_context().get_stat("ipo_inlining.optimized_calls"), 0);
	EXPECT_EQ(find_inlined_region(module), nullptr);
}

TEST_F(IPOInliningPassFixture, LoadOperationInlining)
{
	auto *module = builder->create_module("test_module");

	/*
	 * int load_and_add(int* ptr_a, int* ptr_b)
	 * {
	 *     return *ptr_a + *ptr_b;
	 * }
	 */
	auto load_func = builder->create_function("load_and_add",
	                                          {
		                                          builder->pointer_type(blm::DataType::INT32),
		                                          builder->pointer_type(blm::DataType::INT32)
	                                          },
	                                          blm::DataType::INT32);
	blm::Node *load_node = load_func.get_function();

	load_func.body([&]
	{
		auto *param_ptr_a = load_func.add_parameter("ptr_a", builder->pointer_type(blm::DataType::INT32));
		auto *param_ptr_b = load_func.add_parameter("ptr_b", builder->pointer_type(blm::DataType::INT32));

		auto *loaded_a = builder->ptr_load(param_ptr_a, blm::DataType::INT32);
		auto *loaded_b = builder->ptr_load(param_ptr_b, blm::DataType::INT32);
		auto *sum = builder->add(loaded_a, loaded_b);

		builder->ret(sum);
	});

	/*
	 * int caller() {
	 *     int x = 42;
	 *     int y = 58;
	 *     return load_and_add(&x, &y);
	 * }
	 */
	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);

	caller_func.body([&]
	{
		/* allocate local variables */
		auto *size_int = builder->literal(static_cast<std::uint32_t>(4));
		auto *x_alloc = builder->stack_alloc(size_int, blm::DataType::INT32);
		auto *y_alloc = builder->stack_alloc(size_int, blm::DataType::INT32);

		/* initialize them */
		auto *val_42 = builder->literal(42);
		auto *val_58 = builder->literal(58);
		builder->ptr_store(val_42, x_alloc);
		builder->ptr_store(val_58, y_alloc);

		/* call function with addresses */
		auto *result = builder->call(load_node, { x_alloc, y_alloc });
		builder->ret(result);
	});

	/* print IR before inlining */
	std::string ir_before = print_module_ir(*module, "IR BEFORE LOAD INLINING");
	std::cout << ir_before << std::endl;

	/* run inlining pass */
	std::vector<blm::Module *> modules = { module };
	run_inlining_pass(modules);

	/* print IR after inlining */
	std::string ir_after = print_module_ir(*module, "IR AFTER LOAD INLINING");
	std::cout << ir_after << std::endl;

	/* print pass statistics */
	std::cout << "=== LOAD INLINING STATISTICS ===\n";
	pass_manager->print_statistics();
	std::cout << std::endl;

	/* verify inlining occurred */
	blm::Region *inlined_region = find_inlined_region(module);
	ASSERT_NE(inlined_region, nullptr) << "Inlined region should exist";

	/* verify no parameter nodes remain */
	EXPECT_EQ(count_nodes_of_type(inlined_region, blm::NodeType::PARAM), 0)
		<< "Parameter nodes should be substituted";

	/* verify load operations are present in inlined code */
	EXPECT_GT(count_nodes_of_type(inlined_region, blm::NodeType::PTR_LOAD), 0)
		<< "Load operations should be present in inlined region";

	/* verify addition operation is present */
	EXPECT_GT(count_nodes_of_type(inlined_region, blm::NodeType::ADD), 0)
		<< "ADD operation should be present in inlined region";

	/* verify the loads now reference the allocated stack variables directly */
	bool found_loads_with_stack_args = false;
	std::size_t load_count = 0;

	for (blm::Node *node: inlined_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::PTR_LOAD && !node->inputs.empty())
		{
			blm::Node *address = node->inputs[0];
			/* the address should be a stack allocation (from the caller) */
			if (address->ir_type == blm::NodeType::STACK_ALLOC)
			{
				found_loads_with_stack_args = true;
				load_count++;
			}
		}
	}

	EXPECT_TRUE(found_loads_with_stack_args)
		<< "Load operations should reference stack allocations from caller";
	EXPECT_EQ(load_count, 2)
		<< "Should have exactly 2 load operations (one for each parameter)";

	/* verify the addition uses the loaded values */
	bool found_add_with_loads = false;
	for (blm::Node *node: inlined_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::ADD && node->inputs.size() == 2)
		{
			blm::Node *lhs = node->inputs[0];
			blm::Node *rhs = node->inputs[1];

			/* both inputs should be the result of load operations */
			if (lhs->ir_type == blm::NodeType::PTR_LOAD &&
			    rhs->ir_type == blm::NodeType::PTR_LOAD)
			{
				found_add_with_loads = true;
			}
		}
	}

	EXPECT_TRUE(found_add_with_loads)
		<< "ADD operation should use the results of the load operations";
}

TEST_F(IPOInliningPassFixture, LoadStoreOrderPreservation)
{
	auto *module = builder->create_module("test_module");

	/*
	 * int modify_and_load(int* ptr)
	 * {
	 *     *ptr = 100;
	 *     return *ptr;
	 * }
	 */
	auto modify_func = builder->create_function("modify_and_load",
	                                            { builder->pointer_type(blm::DataType::INT32) },
	                                            blm::DataType::INT32);
	blm::Node *modify_node = modify_func.get_function();

	modify_func.body([&]
	{
		auto *param_ptr = modify_func.add_parameter("ptr", builder->pointer_type(blm::DataType::INT32));

		/* store then load */
		auto *val_100 = builder->literal(100);
		builder->ptr_store(val_100, param_ptr);
		auto *loaded_val = builder->ptr_load(param_ptr, blm::DataType::INT32);

		builder->ret(loaded_val);
	});

	/*
	 * int caller()
	 * {
	 *     int x = 42;
	 *     return modify_and_load(&x);
	 * }
	 */
	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);

	caller_func.body([&]
	{
		auto *size_int = builder->literal(static_cast<std::uint32_t>(4));
		auto *x_alloc = builder->stack_alloc(size_int, blm::DataType::INT32);

		auto *val_42 = builder->literal(42);
		builder->ptr_store(val_42, x_alloc);

		auto *result = builder->call(modify_node, { x_alloc });
		builder->ret(result);
	});

	std::string ir_before = print_module_ir(*module, "before");
	std::cout << ir_before << std::endl;

	std::vector modules = { module };
	run_inlining_pass(modules);

	std::string ir_after = print_module_ir(*module, "after");
	std::cout << ir_after << std::endl;

	pass_manager->print_statistics();

	blm::Region *inlined_region = find_inlined_region(module);
	ASSERT_NE(inlined_region, nullptr);

	EXPECT_GT(count_nodes_of_type(inlined_region, blm::NodeType::PTR_STORE), 0);
	EXPECT_GT(count_nodes_of_type(inlined_region, blm::NodeType::PTR_LOAD), 0);

	std::vector<blm::Node *> store_nodes;
	std::vector<blm::Node *> load_nodes;
	for (blm::Node *node: inlined_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::PTR_STORE)
			store_nodes.push_back(node);
		else if (node->ir_type == blm::NodeType::PTR_LOAD)
			load_nodes.push_back(node);
	}

	ASSERT_FALSE(store_nodes.empty());
	ASSERT_FALSE(load_nodes.empty());
	const auto &nodes = inlined_region->get_nodes();
	auto store_pos = std::ranges::find(nodes, store_nodes[0]);
	auto load_pos = std::ranges::find(nodes, load_nodes[0]);

	EXPECT_LT(store_pos, load_pos);

	blm::Node *store_target = store_nodes[0]->inputs.size() >= 2 ? store_nodes[0]->inputs[1] : nullptr;
	blm::Node *load_source = load_nodes[0]->inputs.size() >= 1 ? load_nodes[0]->inputs[0] : nullptr;

	ASSERT_NE(store_target, nullptr);
	ASSERT_NE(load_source, nullptr);
	EXPECT_EQ(store_target, load_source);
}

TEST_F(IPOInliningPassFixture, MultiplCallsLiteralReuse)
{
	auto *module = builder->create_module("test_module");

	/* int square(int x) { return x * x; } */
	auto square_func = builder->create_function("square", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *square_node = square_func.get_function();

	square_func.body([&]
	{
		auto *param = square_func.add_parameter("x", blm::DataType::INT32);
		auto *result = builder->mul(param, param);
		builder->ret(result);
	});

	/* int caller() { return square(5) + square(5) + square(7); } */
	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);

	caller_func.body([&]
	{
		auto *five = builder->literal(5);
		auto *seven = builder->literal(7);

		auto *call1 = builder->call(square_node, { five });
		auto *call2 = builder->call(square_node, { five });
		auto *call3 = builder->call(square_node, { seven });

		auto *sum1 = builder->add(call1, call2);
		auto *sum2 = builder->add(sum1, call3);
		builder->ret(sum2);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module *> modules = { module };
	run_inlining_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	EXPECT_EQ(pass_manager->get_context().get_stat("ipo_inlining.optimized_calls"), 6);

	blm::Region *caller_region = find_region_by_name(module, "caller");
	ASSERT_NE(caller_region, nullptr);

	std::size_t literal_5_count = 0;
	std::size_t literal_7_count = 0;
	std::size_t mul_count = 0;

	for (blm::Node *node: caller_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::LIT && node->type_kind == blm::DataType::INT32)
		{
			std::int32_t val = node->as<blm::DataType::INT32>();
			if (val == 5)
				literal_5_count++;
			if (val == 7)
				literal_7_count++;
		}
		if (node->ir_type == blm::NodeType::MUL)
		{
			mul_count++;
		}
	}

	EXPECT_EQ(mul_count, 3);
	EXPECT_GE(literal_5_count, 1);
	EXPECT_GE(literal_7_count, 1);
}

TEST_F(IPOInliningPassFixture, CrossModuleInlining)
{
	auto *module_a = builder->create_module("module_a");
	auto *module_b = builder->create_module("module_b");

	builder->set_current_module(module_a);
	auto utility_func = builder->create_function("utility", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *utility_node = utility_func.get_function();

	utility_func.body([&]
	{
		auto *param = utility_func.add_parameter("val", blm::DataType::INT32);
		auto *two = builder->literal(2);
		auto *result = builder->mul(param, two);
		builder->ret(result);
	});

	builder->set_current_module(module_b);
	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);

	caller_func.body([&]
	{
		auto *fifteen = builder->literal(15);
		auto *result = builder->call(utility_node, { fifteen });
		builder->ret(result);
	});

	std::cout << "A before" << std::endl;
	std::cout << print_module_ir(*module_a) << std::endl;
	std::cout << "B before" << std::endl;
	std::cout << print_module_ir(*module_b) << std::endl;

	std::vector modules = { module_a, module_b };
	run_inlining_pass(modules);

	std::cout << "A after" << std::endl;
	std::cout << print_module_ir(*module_a) << std::endl;
	std::cout << "B after" << std::endl;
	std::cout << print_module_ir(*module_b) << std::endl;

	pass_manager->print_statistics();

	EXPECT_GT(pass_manager->get_context().get_stat("ipo_inlining.optimized_calls"), 0);

	blm::Region *inlined_region = find_inlined_region(module_b);
	EXPECT_NE(inlined_region, nullptr);

	if (inlined_region)
	{
		EXPECT_GT(count_nodes_of_type(inlined_region, blm::NodeType::MUL), 0);
		EXPECT_TRUE(has_literal_value(inlined_region, 2));
	}
}

TEST_F(IPOInliningPassFixture, IdentityFunctionInlining)
{
	auto *module = builder->create_module("test_module");

	/* int identity(int x) { return x; } */
	auto identity_func = builder->create_function("identity", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *identity_node = identity_func.get_function();

	identity_func.body([&]
	{
		auto *param = identity_func.add_parameter("x", blm::DataType::INT32);
		builder->ret(param);
	});

	/* int caller() { return identity(42); } */
	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);

	caller_func.body([&]
	{
		auto *forty_two = builder->literal(42);
		auto *result = builder->call(identity_node, { forty_two });
		builder->ret(result);
	});

	std::cout << print_module_ir(*module, "before") << std::endl;

	std::vector<blm::Module *> modules = { module };
	run_inlining_pass(modules);

	std::cout << print_module_ir(*module, "after") << std::endl;
	pass_manager->print_statistics();

	EXPECT_GT(pass_manager->get_context().get_stat("ipo_inlining.optimized_calls"), 0);

	blm::Region *caller_region = find_region_by_name(module, "caller");
	ASSERT_NE(caller_region, nullptr);
	EXPECT_EQ(count_nodes_of_type(caller_region, blm::NodeType::CALL), 0);
	EXPECT_TRUE(has_literal_value(caller_region, 42));
	bool found_ret_with_literal = false;
	for (blm::Node *node: caller_region->get_nodes())
	{
		if (node->ir_type == blm::NodeType::RET && !node->inputs.empty())
		{
			blm::Node *ret_val = node->inputs[0];
			if (ret_val->ir_type == blm::NodeType::LIT &&
			    ret_val->type_kind == blm::DataType::INT32 &&
			    ret_val->as<blm::DataType::INT32>() == 42)
			{
				found_ret_with_literal = true;
			}
		}
	}

	EXPECT_TRUE(found_ret_with_literal);
}
