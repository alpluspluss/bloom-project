/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <bloom/transform/dce.hpp>
#include <bloom/transform/instcombine/instcombine.hpp>
#include <gtest/gtest.h>

using namespace blm;

class InstcombinePassTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ctx = std::make_unique<Context>();
		builder = std::make_unique<Builder>(*ctx);
		module = builder->create_module("test_module");
	}

	void TearDown() override
	{
		builder.reset();
		ctx.reset();
	}

	std::size_t run_agbs()
	{
		PassContext pass_ctx(*module);
		InstcombinePass agbs;
		agbs.run(*module, pass_ctx);
		return pass_ctx.get_stat("agbs.simplified_expressions");
	}

	void print_ir(const std::string &label = "")
	{
		if (!label.empty())
			std::cout << label << ":\n";
		IRPrinter printer(std::cout);
		printer.print_module(*module);
		std::cout << "\n";
	}

	std::unique_ptr<Context> ctx;
	std::unique_ptr<Builder> builder;
	Module *module = nullptr;
};

TEST_F(InstcombinePassTest, ArithmeticIdentities)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* x + 0 = x, x * 1 = x */
		auto add_zero = builder->add(param, builder->literal(0));
		auto mul_one = builder->mul(param, builder->literal(1));
		auto result = builder->add(add_zero, mul_one);
		builder->ret(result);
	});

	std::size_t simplified = run_agbs();
	EXPECT_GE(simplified, 2); /* should optimize both x+0 and x*1 */
}

TEST_F(InstcombinePassTest, BitwiseIdentities)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* x & 0 = 0, x | 0 = x, x ^ x = 0 */
		auto and_zero = builder->band(param, builder->literal(0));
		auto or_zero = builder->bor(param, builder->literal(0));
		auto xor_self = builder->bxor(param, param);

		auto tmp = builder->add(and_zero, or_zero);
		auto result = builder->add(tmp, xor_self);
		builder->ret(result);
	});

	std::size_t simplified = run_agbs();
	EXPECT_GE(simplified, 3);
}

TEST_F(InstcombinePassTest, ComparisonIdentities)
{
	auto func = builder->create_function("test", {}, DataType::BOOL);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* x == x = true, x != x = false, x < x = false */
		auto eq_self = builder->eq(param, param);
		auto neq_self = builder->neq(param, param);
		auto lt_self = builder->lt(param, param);

		/* should all be constants after optimization */
		auto tmp = builder->band(eq_self, builder->bnot(neq_self));
		auto result = builder->band(tmp, builder->bnot(lt_self));
		builder->ret(result);
	});

	std::size_t simplified = run_agbs();
	EXPECT_GE(simplified, 3);
}

TEST_F(InstcombinePassTest, DoubleNegation)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* ~~x = x */
		auto not1 = builder->bnot(param);
		auto not2 = builder->bnot(not1);
		builder->ret(not2);
	});

	std::size_t simplified = run_agbs();
	EXPECT_EQ(simplified, 1);

	Node *ret_node = nullptr;
	for (Node *node: module->get_root_region()->get_children()[0]->get_nodes())
	{
		if (node->ir_type == NodeType::RET)
		{
			ret_node = node;
			break;
		}
	}
	ASSERT_NE(ret_node, nullptr);
	EXPECT_EQ(ret_node->inputs[0]->ir_type, NodeType::PARAM);
}

TEST_F(InstcombinePassTest, SmallConstantMultiplication)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* x * 3 = (x << 1) + x, x * 5 = (x << 2) + x */
		auto mul3 = builder->mul(param, builder->literal(3));
		auto mul5 = builder->mul(param, builder->literal(5));
		auto mul6 = builder->mul(param, builder->literal(6));
		auto mul9 = builder->mul(param, builder->literal(9));

		auto tmp1 = builder->add(mul3, mul5);
		auto tmp2 = builder->add(mul6, mul9);
		auto result = builder->add(tmp1, tmp2);
		builder->ret(result);
	});

	print_ir("before");
	std::size_t simplified = run_agbs();
	print_ir("after");

	EXPECT_EQ(simplified, 4);

	std::size_t shift_count = 0;
	std::size_t mul_count = 0;

	for (const Region *child: module->get_root_region()->get_children())
	{
		for (Node *node: child->get_nodes())
		{
			if (node->ir_type == NodeType::BSHL)
				shift_count++;
			if (node->ir_type == NodeType::MUL)
				mul_count++;
		}
	}

	EXPECT_GT(shift_count, 0);
	EXPECT_EQ(mul_count, 0);
}

TEST_F(InstcombinePassTest, PowerOfTwoStrengthReduction)
{
	auto func = builder->create_function("test", {}, DataType::UINT32);
	auto param = func.add_parameter("x", DataType::UINT32);

	func.body([&]
	{
		/* x * 8 = x << 3, x / 4 = x >> 2 */
		const auto mul8 = builder->mul(param, builder->literal(static_cast<std::uint32_t>(8)));
		const auto div4 = builder->div(param, builder->literal(static_cast<std::uint32_t>(4)));
		const auto result = builder->add(mul8, div4);
		builder->ret(result);
	});

	const std::size_t simplified = run_agbs();
	EXPECT_EQ(simplified, 2);
	std::size_t shift_count = 0;
	for (const Region *child: module->get_root_region()->get_children())
	{
		for (Node *node: child->get_nodes())
		{
			if (node->ir_type == NodeType::BSHL || node->ir_type == NodeType::BSHR)
				shift_count++;
		}
	}
	EXPECT_EQ(shift_count, 2);
}

TEST_F(InstcombinePassTest, NegationSinking)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* x + (-y) = x - y, (-x) * y = -(x * y) */
		auto neg_param = builder->sub(builder->literal(0), param);
		auto add_neg = builder->add(param, neg_param);
		auto mul_neg = builder->mul(neg_param, param);
		auto result = builder->add(add_neg, mul_neg);
		builder->ret(result);
	});

	print_ir("Before negation sinking");
	std::size_t simplified = run_agbs();
	print_ir("After negation sinking");

	EXPECT_GE(simplified, 2);
}

TEST_F(InstcombinePassTest, CarryBorrowElimination)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto x = func.add_parameter("x", DataType::INT32);
	auto y = func.add_parameter("y", DataType::INT32);

	func.body([&]
	{
		/* x - (x & y) = x & ~y, x + (x & y) = x | y */
		auto and_xy = builder->band(x, y);
		auto sub_pattern = builder->sub(x, and_xy);
		auto add_pattern = builder->add(x, and_xy);
		auto result = builder->add(sub_pattern, add_pattern);
		builder->ret(result);
	});

	print_ir("Before carry/borrow elimination");
	std::size_t simplified = run_agbs();
	print_ir("After carry/borrow elimination");

	EXPECT_GE(simplified, 2);

	/* should see bitwise NOT operations */
	std::size_t not_count = 0;
	for (const Region *child: module->get_root_region()->get_children())
	{
		for (Node *node: child->get_nodes())
		{
			if (node->ir_type == NodeType::BNOT)
				not_count++;
		}
	}
	EXPECT_GT(not_count, 0);
}

TEST_F(InstcombinePassTest, AdvancedBitwisePatterns)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto x = func.add_parameter("x", DataType::INT32);
	auto y = func.add_parameter("y", DataType::INT32);

	func.body([&]
	{
		/* x & (x | y) = x, x | (x & y) = x */
		auto or_xy = builder->bor(x, y);
		auto pattern1 = builder->band(x, or_xy);

		auto and_xy = builder->band(x, y);
		auto pattern2 = builder->bor(x, and_xy);

		auto result = builder->add(pattern1, pattern2);
		builder->ret(result);
	});

	print_ir();
	std::size_t simplified = run_agbs();
	print_ir();
	EXPECT_EQ(simplified, 3);
}

TEST_F(InstcombinePassTest, ComparisonStrengthReduction)
{
	auto func = builder->create_function("test", {}, DataType::BOOL);
	auto param = func.add_parameter("x", DataType::UINT32);

	func.body([&]
	{
		/* x < 8 = (x & ~7) == 0 for unsigned */
		auto lt8 = builder->lt(param, builder->literal(static_cast<std::uint32_t>(8)));
		auto gte16 = builder->gte(param, builder->literal(static_cast<std::uint32_t>(16)));
		auto result = builder->band(lt8, gte16);
		builder->ret(result);
	});

	print_ir("Before comparison strength reduction");
	std::size_t simplified = run_agbs();
	print_ir("After comparison strength reduction");

	EXPECT_GE(simplified, 2);
}

TEST_F(InstcombinePassTest, BooleanOptimizations)
{
	auto func = builder->create_function("test", {}, DataType::BOOL);
	auto param = func.add_parameter("flag", DataType::BOOL);

	func.body([&]
	{
		/* flag == false = !flag, flag != false = flag */
		auto eq_false = builder->eq(param, builder->literal(false));
		auto neq_false = builder->neq(param, builder->literal(false));
		auto result = builder->bor(eq_false, neq_false);
		builder->ret(result);
	});

	print_ir();
	const std::size_t simplified = run_agbs();
	print_ir();
	EXPECT_GE(simplified, 2);
}

TEST_F(InstcombinePassTest, MixedOptimizations)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto x = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* complex expression with multiple optimization opportunities */
		auto zero = builder->literal(0);
		auto one = builder->literal(1);
		builder->literal(2);
		auto three = builder->literal(3);

		/* x + 0 = x */
		auto step1 = builder->add(x, zero);
		/* x * 1 = x */
		auto step2 = builder->mul(step1, one);
		/* x * 3 = (x << 1) + x */
		auto step3 = builder->mul(step2, three);
		/* x - x = 0 */
		auto step4 = builder->sub(step3, step3);
		/* x | 0 = x */
		auto step5 = builder->bor(step3, step4);

		builder->ret(step5);
	});

	print_ir("Before mixed optimizations");
	std::size_t simplified = run_agbs();
	print_ir("After mixed optimizations");

	EXPECT_GE(simplified, 4);
}

TEST_F(InstcombinePassTest, NoOptimizeFlag)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		auto zero = builder->literal(0);
		auto add_node = builder->add(param, zero);

		/* manually set NO_OPTIMIZE flag */
		for (const Region *child: module->get_root_region()->get_children())
		{
			for (Node *node: child->get_nodes())
			{
				if (node->ir_type == NodeType::ADD)
				{
					node->props = NodeProps::NO_OPTIMIZE;
					break;
				}
			}
		}

		builder->ret(add_node);
	});

	std::size_t simplified = run_agbs();
	EXPECT_EQ(simplified, 0); /* should not optimize due to NO_OPTIMIZE flag */
}

TEST_F(InstcombinePassTest, ComplexConstantFolding)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto x = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* test various constant folding scenarios */
		auto minus_one = builder->literal(-1);
		auto all_ones = builder->literal(0xFFFFFFFF);

		/* x * -1 = -x */
		auto mul_neg = builder->mul(x, minus_one);
		/* x & -1 = x */
		auto and_all = builder->band(x, all_ones);
		/* x | -1 = -1 */
		auto or_all = builder->bor(x, all_ones);
		/* x / -1 = -x */
		auto div_neg = builder->div(x, minus_one);

		auto tmp1 = builder->add(mul_neg, and_all);
		auto tmp2 = builder->add(or_all, div_neg);
		auto result = builder->add(tmp1, tmp2);
		builder->ret(result);
	});

	std::size_t simplified = run_agbs();
	EXPECT_GE(simplified, 4);
}

TEST_F(InstcombinePassTest, PowerOfTwoPlusMinusOne)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		/* x * 17 = x * (16 + 1) = (x << 4) + x */
		auto mul17 = builder->mul(param, builder->literal(17));
		/* x * 15 = x * (16 - 1) = (x << 4) - x */
		auto mul15 = builder->mul(param, builder->literal(15));
		/* x * 33 = x * (32 + 1) = (x << 5) + x */
		auto mul33 = builder->mul(param, builder->literal(33));

		auto tmp = builder->add(mul17, mul15);
		auto result = builder->add(tmp, mul33);
		builder->ret(result);
	});

	print_ir("Before 2^n±1 optimization");
	std::size_t simplified = run_agbs();
	print_ir("After 2^n±1 optimization");

	EXPECT_EQ(simplified, 3);
}

TEST_F(InstcombinePassTest, ShiftIdentities)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto x = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		auto zero = builder->literal(0);

		/* x << 0 = x, x >> 0 = x, 0 << x = 0, 0 >> x = 0 */
		auto shl_zero = builder->bshl(x, zero);
		auto shr_zero = builder->bshr(x, zero);
		auto zero_shl = builder->bshl(zero, x);
		auto zero_shr = builder->bshr(zero, x);

		auto tmp1 = builder->add(shl_zero, shr_zero);
		auto tmp2 = builder->add(zero_shl, zero_shr);
		auto result = builder->add(tmp1, tmp2);
		builder->ret(result);
	});

	print_ir("before");
	std::size_t simplified = run_agbs();
	print_ir("after");
	EXPECT_EQ(simplified, 7);
}

TEST_F(InstcombinePassTest, SimpleMultiplicationDebug)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		auto mul3 = builder->mul(param, builder->literal(3));
		builder->ret(mul3);
	});

	print_ir("before");
	std::size_t simplified = run_agbs();
	print_ir("after");

	std::cout << "Simplified count: " << simplified << std::endl;

	std::size_t shift_count = 0;
	std::size_t mul_count = 0;

	for (const Region *child: module->get_root_region()->get_children())
	{
		for (Node *node: child->get_nodes())
		{
			std::cout << "Node type: " << static_cast<int>(node->ir_type) << std::endl;
			if (node->ir_type == NodeType::BSHL)
				shift_count++;
			if (node->ir_type == NodeType::MUL)
				mul_count++;
		}
	}

	std::cout << "Shift count: " << shift_count << std::endl;
	std::cout << "Mul count: " << mul_count << std::endl;

	EXPECT_EQ(simplified, 1);
	EXPECT_GT(shift_count, 0);
	EXPECT_EQ(mul_count, 0);
}
