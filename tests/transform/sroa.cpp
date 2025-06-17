/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <bloom/transform/sroa.hpp>
#include <gtest/gtest.h>

using namespace blm;

class SROAPassTest : public ::testing::Test
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

	std::pair<std::size_t, std::size_t> run_sroa()
	{
		PassContext pass_ctx(*module);

		/* run LAA first since SROA depends on it */
		LocalAliasAnalysisPass laa;
		auto laa_result = laa.analyze(*module, pass_ctx);
		pass_ctx.store_result(typeid(LocalAliasAnalysisPass), std::move(laa_result));

		SROAPass sroa;
		sroa.run(*module, pass_ctx);

		return {
			pass_ctx.get_stat("sroa.promoted_allocations"),
			pass_ctx.get_stat("sroa.scalar_replacements")
		};
	}

	DataType create_simple_struct()
	{
		std::vector<std::pair<std::string, DataType>> fields = {
			{"x", DataType::INT32},
			{"y", DataType::INT32}
		};
		return ctx->create_struct_type(fields, 8, 4);
	}

	DataType create_mixed_struct()
	{
		std::vector<std::pair<std::string, DataType>> fields = {
			{ "a", DataType::INT8 },
			{ "b", DataType::INT32 },
			{ "c", DataType::FLOAT64 }
		};
		return ctx->create_struct_type(fields, 16, 8);
	}

	std::size_t count_stack_allocs(const Region *region = nullptr) const
	{
		if (!region)
			region = module->get_root_region();

		std::size_t count = 0;
		for (const Node *node : region->get_nodes())
		{
			if (node->ir_type == NodeType::STACK_ALLOC)
				count++;
		}

		for (const Region *child : region->get_children())
			count += count_stack_allocs(child);

		return count;
	}

	std::unique_ptr<Context> ctx;
	std::unique_ptr<Builder> builder;
	Module *module = nullptr;
};

TEST_F(SROAPassTest, PromotesSimpleStructAccess)
{
	DataType point_type = create_simple_struct();

	auto func = builder->create_function("test", {}, DataType::INT32);
	func.body([&]
	{
		auto struct_size = builder->literal(static_cast<std::uint64_t>(8));
		auto point_alloc = builder->name_node(builder->stack_alloc(struct_size, point_type), "point_alloc");
		auto point_addr = builder->name_node(builder->addr_of(point_alloc), "point_addr");
		auto x_addr = builder->name_node(builder->ptr_add(point_addr, builder->literal(static_cast<std::int32_t>(0))), "x_addr");
		auto y_addr = builder->name_node(builder->ptr_add(point_addr, builder->literal(static_cast<std::int32_t>(4))), "y_addr");

		builder->ptr_store(builder->literal(10), x_addr);
		builder->ptr_store(builder->literal(20), y_addr);

		auto x_val = builder->ptr_load(x_addr, DataType::INT32);
		auto y_val = builder->ptr_load(y_addr, DataType::INT32);
		auto sum = builder->add(x_val, y_val);
		builder->ret(sum);
	});

	std::cout << "before SROA:\n";
	IRPrinter printer_before(std::cout);
	printer_before.print_module(*module);

	std::size_t allocs_before = count_stack_allocs();
	auto [promoted, scalars] = run_sroa();

	std::cout << "\nafter SROA:\n";
	IRPrinter printer_after(std::cout);
	printer_after.print_module(*module);
	std::cout << "\npromoted " << promoted << " allocations, created " << scalars << " scalars\n\n";

	EXPECT_EQ(promoted, 1);
	EXPECT_EQ(scalars, 2);
	EXPECT_GT(count_stack_allocs(), allocs_before); /* more allocs after SROA (scalars) */
}

TEST_F(SROAPassTest, HandlesDirectStructAccess)
{
	DataType point_type = create_simple_struct();

	auto func = builder->create_function("test", {}, DataType::INT32);
	func.body([&]
	{
		auto struct_size = builder->literal(static_cast<std::uint64_t>(8));
		auto point_alloc = builder->stack_alloc(struct_size, point_type);
		auto point_addr = builder->addr_of(point_alloc);
		builder->ptr_store(builder->literal(42), point_addr);
		auto result = builder->ptr_load(point_addr, DataType::INT32);
		builder->ret(result);
	});

	auto [promoted, scalars] = run_sroa();
	EXPECT_EQ(promoted, 1);
	EXPECT_GT(scalars, 0);
}

TEST_F(SROAPassTest, PreventPromotionWhenEscaped)
{
	DataType point_type = create_simple_struct();
	auto dummy_func = builder->create_function("dummy", {ctx->create_pointer_type(DataType::INT32)}, DataType::VOID);
	dummy_func.body([&]
	{
		builder->ret(nullptr);
	});

	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto struct_size = builder->literal(static_cast<std::uint64_t>(8));
		auto point_alloc = builder->stack_alloc(struct_size, point_type);
		auto point_addr = builder->addr_of(point_alloc);
		auto x_addr = builder->ptr_add(point_addr, builder->literal(static_cast<std::int32_t>(0)));
		builder->call(dummy_func.get_function(), {x_addr});

		builder->ret(nullptr);
	});

	auto [promoted, scalars] = run_sroa();
	EXPECT_EQ(promoted, 1); /* do a single promotion to prevent further promotions */
	EXPECT_EQ(scalars, 1);
}

TEST_F(SROAPassTest, HandlesPartialPromotion)
{
	DataType mixed_type = create_mixed_struct();
	auto dummy_func = builder->create_function("dummy", {ctx->create_pointer_type(DataType::INT32)}, DataType::VOID);
	dummy_func.body([&]
	{
		builder->ret(nullptr);
	});

	auto func = builder->create_function("test", {}, DataType::FLOAT64);
	func.body([&]
	{
		auto struct_size = builder->literal(static_cast<std::uint64_t>(16));
		auto mixed_alloc = builder->stack_alloc(struct_size, mixed_type);

		auto struct_addr = builder->addr_of(mixed_alloc);
		auto a_addr = builder->ptr_add(struct_addr, builder->literal(static_cast<std::int32_t>(0)));  /* INT8 a */
		auto b_addr = builder->ptr_add(struct_addr, builder->literal(static_cast<std::int32_t>(4)));  /* INT32 b */
		auto c_addr = builder->ptr_add(struct_addr, builder->literal(static_cast<std::int32_t>(8)));  /* FLOAT64 c */
		builder->ptr_store(builder->literal(static_cast<std::int8_t>(1)), a_addr);
		builder->ptr_store(builder->literal(3.14), c_addr);
		builder->call(dummy_func.get_function(), {b_addr});
		auto result = builder->ptr_load(c_addr, DataType::FLOAT64);
		builder->ret(result);
	});

	std::cout << "before partial SROA:\n";
	IRPrinter printer_before(std::cout);
	printer_before.print_module(*module);

	auto [promoted, scalars] = run_sroa();

	std::cout << "\nafter partial SROA:\n";
	IRPrinter printer_after(std::cout);
	printer_after.print_module(*module);
	std::cout << "\npromoted " << promoted << " allocations, created " << scalars << " scalars\n\n";

	EXPECT_EQ(promoted, 1);
	EXPECT_GT(scalars, 0);
	EXPECT_LT(scalars, 3);
}

TEST_F(SROAPassTest, RejectsNonConstantOffsets)
{
	DataType point_type = create_simple_struct();

	auto func = builder->create_function("test", {}, DataType::INT32);
	auto param = func.add_parameter("offset", DataType::INT32);

	func.body([&]
	{
		auto struct_size = builder->literal(static_cast<std::uint64_t>(8));
		auto point_alloc = builder->stack_alloc(struct_size, point_type);
		auto point_addr = builder->addr_of(point_alloc);
		auto dynamic_addr = builder->ptr_add(point_addr, param);
		auto result = builder->ptr_load(dynamic_addr, DataType::INT32);
		builder->ret(result);
	});

	auto [promoted, scalars] = run_sroa();
	EXPECT_EQ(promoted, 0); /* should not promote with dynamic offset */
	EXPECT_EQ(scalars, 0);
}

TEST_F(SROAPassTest, RejectsStructPassedToFunction)
{
	DataType point_type = create_simple_struct();
	auto dummy_func = builder->create_function("dummy", {point_type}, DataType::VOID);
	dummy_func.body([&]
	{
		builder->ret(nullptr);
	});

	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		/* allocate struct */
		auto struct_size = builder->literal(static_cast<std::uint64_t>(8));
		auto point_alloc = builder->stack_alloc(struct_size, point_type);

		/* pass entire struct to function */
		builder->call(dummy_func.get_function(), {point_alloc});
		builder->ret(nullptr);
	});

	auto [promoted, scalars] = run_sroa();
	EXPECT_EQ(promoted, 0); /* should not promote when struct is passed to function */
	EXPECT_EQ(scalars, 0);
}

TEST_F(SROAPassTest, RejectsStructReturnedFromFunction)
{
	DataType point_type = create_simple_struct();

	auto func = builder->create_function("test", {}, point_type);
	func.body([&]
	{
		auto struct_size = builder->literal(static_cast<std::uint64_t>(8));
		auto point_alloc = builder->stack_alloc(struct_size, point_type);
		builder->ret(point_alloc);
	});

	auto [promoted, scalars] = run_sroa();
	EXPECT_EQ(promoted, 0);
	EXPECT_EQ(scalars, 0);
}
