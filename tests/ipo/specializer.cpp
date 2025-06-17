/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/ipo/specializer.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <gtest/gtest.h>

class FunctionSpecializerFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		context = std::make_unique<blm::Context>();
		builder = std::make_unique<blm::Builder>(*context);
		specializer = std::make_unique<blm::FunctionSpecializer>();
	}

	void TearDown() override
	{
		specializer.reset();
		builder.reset();
		context.reset();
	}

	std::unique_ptr<blm::Context> context;
	std::unique_ptr<blm::Builder> builder;
	std::unique_ptr<blm::FunctionSpecializer> specializer;
};

TEST_F(FunctionSpecializerFixture, BasicFunctionCloning)
{
	auto *module = builder->create_module("test_module");

	/* int add_five(int x) { return x + 5; } */
	auto add_five_func = builder->create_function("add_five", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *add_five_node = add_five_func.get_function();

	blm::Node *param_x = nullptr;
	add_five_func.body([&]
	{
		param_x = add_five_func.add_parameter("x", blm::DataType::INT32);
		auto *five = builder->literal(5);
		auto *result = builder->add(param_x, five);
		builder->ret(result);
	});

	auto caller_func = builder->create_function("test_caller", {}, blm::DataType::INT32);
	blm::Node *call_site = nullptr;
	caller_func.body([&]
	{
		auto *arg = builder->literal(10);
		call_site = builder->call(add_five_node, { arg });
		builder->ret(call_site);
	});

	/* create specialization request for x = 10 */
	blm::FunctionSpecializer::SpecializationRequest req;
	req.original_function = add_five_node;
	req.specialized_params = { { 0, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(10) } };
	req.benefit_score = 5.0;
	req.call_sites = { call_site };

	blm::Node *specialized = specializer->specialize_function(req, *module);

	ASSERT_NE(specialized, nullptr);
	EXPECT_NE(specialized, add_five_node);
	EXPECT_EQ(module->get_functions().size(), 3);
}

TEST_F(FunctionSpecializerFixture, CallSiteRedirection)
{
	auto *module = builder->create_module("test_module");

	/* int double_it(int x) { return x * 2; } */
	auto double_func = builder->create_function("double_it", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *double_node = double_func.get_function();

	double_func.body([&]
	{
		auto *param = double_func.add_parameter("x", blm::DataType::INT32);
		auto *two = builder->literal(2);
		auto *result = builder->mul(param, two);
		builder->ret(result);
	});

	auto caller_func = builder->create_function("caller", {}, blm::DataType::INT32);
	blm::Node *call_site = nullptr;
	caller_func.body([&]
	{
		auto *arg = builder->literal(42);
		call_site = builder->call(double_node, { arg });
		builder->ret(call_site);
	});

	blm::FunctionSpecializer::SpecializationRequest req;
	req.original_function = double_node;
	req.specialized_params = { { 0, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(42) } };
	req.call_sites = { call_site };
	req.benefit_score = 4.0;

	blm::Node *specialized = specializer->specialize_function(req, *module);
	ASSERT_NE(specialized, nullptr);
	EXPECT_EQ(call_site->inputs[0], specialized); /* call should now target specialized function */
	EXPECT_EQ(call_site->inputs.size(), 1);       /* no arguments since parameter was specialized */
}

TEST_F(FunctionSpecializerFixture, ShouldSpecializeHeuristics)
{
	builder->create_module("test_module");
	auto func = builder->create_function("test_func", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *func_node = func.get_function();

	func.body([&]
	{
		auto *param = func.add_parameter("x", blm::DataType::INT32);
		builder->ret(param);
	});

	blm::FunctionSpecializer::SpecializationRequest low_benefit_req;
	low_benefit_req.original_function = func_node;
	low_benefit_req.specialized_params = {
		{ 0, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(10) }
	};
	low_benefit_req.call_sites = { func_node };
	low_benefit_req.benefit_score = 0.5;

	EXPECT_FALSE(specializer->should_specialize(low_benefit_req));

	blm::FunctionSpecializer::SpecializationRequest good_req;
	good_req.original_function = func_node;
	good_req.specialized_params = { { 0, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(10) } };
	good_req.call_sites = { func_node };
	good_req.benefit_score = 5.0;

	EXPECT_TRUE(specializer->should_specialize(good_req));

	specializer->set_max_call_sites(2);
	std::vector many_call_sites(5, func_node);
	good_req.call_sites = many_call_sites;

	EXPECT_FALSE(specializer->should_specialize(good_req));
}

TEST_F(FunctionSpecializerFixture, BenefitScoreCalculation)
{
	builder->create_module("test_module");

	auto func = builder->create_function("test_func", { blm::DataType::INT32, blm::DataType::INT32 },
	                                     blm::DataType::INT32);
	blm::Node *func_node = func.get_function();

	blm::FunctionSpecializer::SpecializationRequest req1;
	req1.original_function = func_node;
	req1.specialized_params = { { 0, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(1) } };
	req1.call_sites = { func_node };

	blm::FunctionSpecializer::SpecializationRequest req2;
	req2.original_function = func_node;
	req2.specialized_params = {
		{ 0, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(1) },
		{ 1, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(2) }
	};
	req2.call_sites = { func_node };

	double score1 = blm::FunctionSpecializer::calculate_benefit_score(req1);
	double score2 = blm::FunctionSpecializer::calculate_benefit_score(req2);

	EXPECT_GT(score2, score1); /* more constants = higher benefit */

	std::vector more_call_sites(3, func_node);
	req1.call_sites = more_call_sites;
	double score_with_more_calls = blm::FunctionSpecializer::calculate_benefit_score(req1);
	EXPECT_GT(score_with_more_calls, score1);
}

TEST_F(FunctionSpecializerFixture, InvokeCallSiteRedirection)
{
	auto *module = builder->create_module("test_module");
	auto target_func = builder->create_function("target", { blm::DataType::INT32 }, blm::DataType::INT32);
	blm::Node *target_node = target_func.get_function();

	target_func.body([&]
	{
		auto *param = target_func.add_parameter("x", blm::DataType::INT32);
		builder->ret(param);
	});

	/* create caller with INVOKE */
	auto caller_func = builder->create_function("caller", {}, blm::DataType::VOID);

	blm::Node *invoke_site = nullptr;
	caller_func.body([&]
	{
		auto invoke_blocks = builder->create_invoke_blocks("normal", "except");

		auto *arg = builder->literal(99);
		invoke_site = builder->invoke(target_node, { arg },
		                              invoke_blocks.normal.get_region()->get_nodes()[0],
		                              invoke_blocks.except.get_region()->get_nodes()[0]);

		invoke_blocks.normal([&]
		{
			builder->ret(nullptr);
		});

		invoke_blocks.except([&]
		{
			builder->ret(nullptr);
		});
	});

	blm::FunctionSpecializer::SpecializationRequest req;
	req.original_function = target_node;
	req.specialized_params = { { 0, blm::LatticeValue::make_constant<std::int32_t, blm::DataType::INT32>(99) } };
	req.call_sites = { invoke_site };
	req.benefit_score = 4.0;

	blm::Node *specialized = specializer->specialize_function(req, *module);
	ASSERT_NE(specialized, nullptr);
	EXPECT_EQ(invoke_site->inputs[0], specialized); /* function target updated */
	EXPECT_EQ(invoke_site->inputs.size(), 3);       /* function + normal_target + except_target, no args */
}
