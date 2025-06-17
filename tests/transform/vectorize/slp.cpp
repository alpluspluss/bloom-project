/* this project is part of the bloom project; licensed under the MIT license. see LICENSE for more info */

#include <gtest/gtest.h>
#include <bloom/analysis/laa.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/transform/vectorize/slp.hpp>

using namespace blm;

class SLPPassTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ctx = std::make_unique<blm::Context>();
		builder = std::make_unique<blm::Builder>(*ctx);
		module = builder->create_module("test_module");
	}

	void TearDown() override
	{
		builder.reset();
		ctx.reset();
	}

	std::pair<std::size_t, std::size_t> run_slp()
	{
		PassContext pass_ctx(*module);

		LocalAliasAnalysisPass laa;
		auto laa_result = laa.analyze(*module, pass_ctx);
		pass_ctx.store_result(typeid(LocalAliasAnalysisPass), std::move(laa_result));

		SLPPass slp;
		slp.run(*module, pass_ctx);

		return {
			pass_ctx.get_stat("slp.vectorized_operations"),
			pass_ctx.get_stat("slp.vector_groups")
		};
	}

	std::size_t count_vector_operations(const Region* region = nullptr) const
	{
		if (!region)
			region = module->get_root_region();

		std::size_t count = 0;
		for (const Node* node : region->get_nodes())
		{
			if (is_vector_type(node->type_kind))
				count++;
		}

		for (const Region* child : region->get_children())
			count += count_vector_operations(child);

		return count;
	}

	std::size_t count_node_type(NodeType type, const Region* region = nullptr) const
	{
		if (!region)
			region = module->get_root_region();

		std::size_t count = 0;
		for (const Node* node : region->get_nodes())
		{
			if (node->ir_type == type)
				count++;
		}

		for (const Region* child : region->get_children())
			count += count_node_type(type, child);

		return count;
	}

	std::unique_ptr<Context> ctx;
	std::unique_ptr<Builder> builder;
	Module* module = nullptr;
};

TEST_F(SLPPassTest, VectorizesIndependentAdds)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto a1 = builder->literal(1);
		auto b1 = builder->literal(2);
		auto a2 = builder->literal(3);
		auto b2 = builder->literal(4);
		auto a3 = builder->literal(5);
		auto b3 = builder->literal(6);
		auto a4 = builder->literal(7);
		auto b4 = builder->literal(8);

		auto add1 = builder->add(a1, b1);
		auto add2 = builder->add(a2, b2);
		auto add3 = builder->add(a3, b3);
		auto add4 = builder->add(a4, b4);

		auto ptr1 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr2 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr3 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr4 = builder->stack_alloc(builder->literal(4u), DataType::INT32);

		builder->store(add1, ptr1);
		builder->store(add2, ptr2);
		builder->store(add3, ptr3);
		builder->store(add4, ptr4);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vectorized_ops, 4);
	EXPECT_EQ(vector_groups, 1);

	std::size_t builds_after = count_node_type(NodeType::VECTOR_BUILD);
	std::size_t extracts_after = count_node_type(NodeType::VECTOR_EXTRACT);
	EXPECT_GT(builds_after, 0);
	EXPECT_GT(extracts_after, 0);
}

TEST_F(SLPPassTest, RespectsDataDependenciesInChain)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto a1 = builder->literal(1);
		auto b1 = builder->literal(2);
		auto a2 = builder->literal(3);
		auto b2 = builder->literal(4);
		auto a3 = builder->literal(5);
		auto b3 = builder->literal(6);
		auto a4 = builder->literal(7);
		auto b4 = builder->literal(8);

		auto add1 = builder->add(a1, b1);
		auto add2 = builder->add(a2, b2);
		auto add3 = builder->add(a3, b3);
		auto add4 = builder->add(a4, b4);

		/* create dependency chain to prevent vectorization */
		auto sum = builder->add(add1, add2);
		sum = builder->add(sum, add3);
		builder->add(sum, add4);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	std::cout << "\nvectorized " << vectorized_ops << " operations into " << vector_groups << " groups\n\n";

	EXPECT_EQ(vector_groups, 0);
	EXPECT_EQ(vectorized_ops, 0);
}

TEST_F(SLPPassTest, VectorizesMultipleOperationTypes)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto a1 = builder->literal(1);
		auto b1 = builder->literal(2);
		auto a2 = builder->literal(3);
		auto b2 = builder->literal(4);

		auto c1 = builder->literal(10);
		auto d1 = builder->literal(20);
		auto c2 = builder->literal(30);
		auto d2 = builder->literal(40);

		auto add1 = builder->add(a1, b1);
		auto add2 = builder->add(a2, b2);

		auto mul1 = builder->mul(c1, d1);
		auto mul2 = builder->mul(c2, d2);

		auto ptr1 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr2 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr3 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr4 = builder->stack_alloc(builder->literal(4u), DataType::INT32);

		builder->store(add1, ptr1);
		builder->store(add2, ptr2);
		builder->store(mul1, ptr3);
		builder->store(mul2, ptr4);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vector_groups, 2);    /* one for ADDs, one for MULs */
	EXPECT_EQ(vectorized_ops, 4);   /* 2 adds + 2 muls */
}

TEST_F(SLPPassTest, RespectsDirectDataDependencies)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto a = builder->literal(1);
		auto b = builder->literal(2);
		auto c = builder->literal(3);

		/* these have a dependency; add2 uses result of add1 */
		auto add1 = builder->add(a, b);
		auto add2 = builder->add(add1, c); /* depends on add1 */

		auto add3 = builder->add(b, c);

		builder->add(add2, add3);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vector_groups, 0);
	EXPECT_EQ(vectorized_ops, 0);
}

TEST_F(SLPPassTest, HandlesWideVectors)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		std::vector<Node*> adds;
		std::vector<Node*> ptrs;

		for (int i = 0; i < 8; ++i)
		{
			auto a = builder->literal(i);
			auto b = builder->literal(i + 10);
			auto add = builder->add(a, b);
			adds.push_back(add);
			ptrs.push_back(builder->stack_alloc(builder->literal(4u), DataType::INT32));
		}

		for (std::size_t i = 0; i < adds.size(); ++i)
			builder->store(adds[i], ptrs[i]);

		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vectorized_ops, 8);   /* all 8 adds should vectorize */
	EXPECT_EQ(vector_groups, 1);    /* into 1 vector group */

	std::size_t vector_builds = count_node_type(NodeType::VECTOR_BUILD);
	EXPECT_GT(vector_builds, 0);
}

TEST_F(SLPPassTest, HandlesDifferentDataTypes)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto i1 = builder->literal(1);
		auto i2 = builder->literal(2);
		auto i3 = builder->literal(3);
		auto i4 = builder->literal(4);
		auto add_i1 = builder->add(i1, i2);
		auto add_i2 = builder->add(i3, i4);

		auto f1 = builder->literal(1.0f);
		auto f2 = builder->literal(2.0f);
		auto f3 = builder->literal(3.0f);
		auto f4 = builder->literal(4.0f);
		auto add_f1 = builder->add(f1, f2);
		auto add_f2 = builder->add(f3, f4);

		auto ptr_i1 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr_i2 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr_f1 = builder->stack_alloc(builder->literal(4u), DataType::FLOAT32);
		auto ptr_f2 = builder->stack_alloc(builder->literal(4u), DataType::FLOAT32);

		builder->store(add_i1, ptr_i1);
		builder->store(add_i2, ptr_i2);
		builder->store(add_f1, ptr_f1);
		builder->store(add_f2, ptr_f2);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vector_groups, 2);    /* one for i32, one for f32 */
	EXPECT_EQ(vectorized_ops, 4);   /* 2 int adds + 2 float adds */
}

TEST_F(SLPPassTest, DoesNotVectorizeUnsupportedOperations)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto ptr = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(16)), DataType::INT32);
		auto value1 = builder->literal(42);
		auto value2 = builder->literal(84);

		builder->store(value1, ptr);
		builder->store(value2, ptr);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vector_groups, 0);
	EXPECT_EQ(vectorized_ops, 0);
}

TEST_F(SLPPassTest, VectorizesAcrossRegions)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto block1 = builder->create_block("block1");
		auto block2 = builder->create_block("block2");
		block1([&]
		{
			auto a1 = builder->literal(1);
			auto b1 = builder->literal(2);
			auto a2 = builder->literal(3);
			auto b2 = builder->literal(4);

			auto add1 = builder->add(a1, b1);
			auto add2 = builder->add(a2, b2);
			auto ptr1 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
			auto ptr2 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
			builder->store(add1, ptr1);
			builder->store(add2, ptr2);

			auto block2_entry = block2.get_region()->get_nodes()[0];
			builder->jump(block2_entry);
		});

		block2([&]
		{
			auto c1 = builder->literal(5);
			auto d1 = builder->literal(6);
			auto c2 = builder->literal(7);
			auto d2 = builder->literal(8);

			auto add3 = builder->add(c1, d1);
			auto add4 = builder->add(c2, d2);
			auto ptr3 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
			auto ptr4 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
			builder->store(add3, ptr3);
			builder->store(add4, ptr4);
			builder->ret(nullptr);
		});
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vector_groups, 2);    /* one group per region */
	EXPECT_EQ(vectorized_ops, 4);   /* 2 adds per region */
}

TEST_F(SLPPassTest, VectorizesMixedArithmetic)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto a1 = builder->literal(10);
		auto b1 = builder->literal(5);
		auto a2 = builder->literal(20);
		auto b2 = builder->literal(4);

		auto c1 = builder->literal(8);
		auto d1 = builder->literal(2);
		auto c2 = builder->literal(15);
		auto d2 = builder->literal(3);

		auto sub1 = builder->sub(a1, b1);
		auto sub2 = builder->sub(a2, b2);

		auto div1 = builder->div(c1, d1);
		auto div2 = builder->div(c2, d2);

		auto ptr1 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr2 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr3 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr4 = builder->stack_alloc(builder->literal(4u), DataType::INT32);

		builder->store(sub1, ptr1);
		builder->store(sub2, ptr2);
		builder->store(div1, ptr3);
		builder->store(div2, ptr4);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vector_groups, 2);    /* one for SUBs, one for DIVs */
	EXPECT_EQ(vectorized_ops, 4);   /* 2 subs + 2 divs */
}

TEST_F(SLPPassTest, HandlesBitwiseOperations)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto a1 = builder->literal(0xFF00);
		auto b1 = builder->literal(0x00FF);
		auto a2 = builder->literal(0xF0F0);
		auto b2 = builder->literal(0x0F0F);

		auto c1 = builder->literal(0xAAAA);
		auto d1 = builder->literal(0x5555);
		auto c2 = builder->literal(0x3333);
		auto d2 = builder->literal(0xCCCC);

		auto and1 = builder->band(a1, b1);
		auto and2 = builder->band(a2, b2);

		auto or1 = builder->bor(c1, d1);
		auto or2 = builder->bor(c2, d2);

		auto ptr1 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr2 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr3 = builder->stack_alloc(builder->literal(4u), DataType::INT32);
		auto ptr4 = builder->stack_alloc(builder->literal(4u), DataType::INT32);

		builder->store(and1, ptr1);
		builder->store(and2, ptr2);
		builder->store(or1, ptr3);
		builder->store(or2, ptr4);
		builder->ret(nullptr);
	});

	auto [vectorized_ops, vector_groups] = run_slp();

	EXPECT_EQ(vector_groups, 2);    /* one for ANDs, one for ORs */
	EXPECT_EQ(vectorized_ops, 4);   /* 2 ands + 2 ors */
}
