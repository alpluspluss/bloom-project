/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/laa.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/transform/dse.hpp>
#include <gtest/gtest.h>

using namespace blm;

class DSEPassTest : public ::testing::Test
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

	std::pair<std::size_t, std::size_t> run_dse()
	{
		PassContext pass_ctx(*module);

		LocalAliasAnalysisPass laa;
		auto laa_result = laa.analyze(*module, pass_ctx);
		pass_ctx.store_result(typeid(LocalAliasAnalysisPass), std::move(laa_result));
		DSEPass dse;
		dse.run(*module, pass_ctx);

		return {
			pass_ctx.get_stat("dse.removed_stores"),
			count_stores()
		};
	}

	std::size_t count_stores(const Region *region = nullptr) const
	{
		if (!region)
			region = module->get_root_region();

		std::size_t count = 0;
		for (const Node *node: region->get_nodes())
		{
			if (node->ir_type == NodeType::STORE ||
				node->ir_type == NodeType::PTR_STORE ||
				node->ir_type == NodeType::ATOMIC_STORE)
			{
				count++;
			}
		}

		for (const Region *child: region->get_children())
			count += count_stores(child);

		return count;
	}

	std::size_t count_nodes(const Region *region = nullptr) const
	{
		if (!region)
			region = module->get_root_region();

		std::size_t count = region->get_nodes().size();
		for (const Region *child: region->get_children())
			count += count_nodes(child);

		return count;
	}

	void print_ir(const std::string& label = "")
	{
		if (!label.empty())
			std::cout << label << std::endl;
		IRPrinter printer(std::cout);
		printer.print_module(*module);
		std::cout << std::endl;
	}

	std::unique_ptr<Context> ctx;
	std::unique_ptr<Builder> builder;
	Module *module = nullptr;
};

TEST_F(DSEPassTest, EliminatesSimpleDeadStore)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		const auto local = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);
		builder->store(builder->literal(42), local);
		builder->store(builder->literal(100), local);
		builder->load(local, DataType::INT32);
		builder->ret(nullptr);
	});

	print_ir("before DSE");

	auto [removed_stores, stores_after] = run_dse();

	print_ir("after DSE");

	EXPECT_EQ(removed_stores, 1);
	EXPECT_EQ(stores_after, 1);
}

TEST_F(DSEPassTest, PreservesStoreReadByLoad)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	func.body([&]
	{
		auto local = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);

		builder->store(builder->literal(42), local);
		auto result = builder->load(local, DataType::INT32);
		builder->ret(result);
	});

	auto [removed_stores, stores_after] = run_dse();

	EXPECT_EQ(removed_stores, 0);
	EXPECT_EQ(stores_after, 1);
}

TEST_F(DSEPassTest, PreservesEscapedStores)
{
	auto callee = builder->create_function("callee", { builder->pointer_type(DataType::INT32) }, DataType::VOID);

	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto local = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);
		auto ptr = builder->addr_of(local);

		builder->store(builder->literal(42), local);
		builder->call(callee.get_function(), {ptr});
		builder->store(builder->literal(100), local);

		builder->ret(nullptr);
	});

	print_ir();
	auto [removed_stores, stores_after] = run_dse();
	print_ir();
	EXPECT_EQ(removed_stores, 0);
	EXPECT_EQ(stores_after, 2);
}

TEST_F(DSEPassTest, EliminatesChainOfDeadStores)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	func.body([&]
	{
		auto local = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);

		builder->store(builder->literal(10), local);
		builder->store(builder->literal(20), local);
		builder->store(builder->literal(30), local);
		builder->store(builder->literal(40), local);

		auto result = builder->load(local, DataType::INT32);
		builder->ret(result);
	});

	auto [removed_stores, stores_after] = run_dse();

	EXPECT_EQ(removed_stores, 3);
	EXPECT_EQ(stores_after, 1);
}

TEST_F(DSEPassTest, HandlesPartialOverlap)
{
	/* create function with overlapping stores to same allocation */
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto local = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(8)), DataType::INT64);
		auto small_val = builder->literal(42);
		builder->store(small_val, local);
		auto large_val = builder->literal(static_cast<std::int64_t>(100));
		builder->store(large_val, local);
		builder->ret(nullptr);
	});

	auto [removed_stores, stores_after] = run_dse();

	EXPECT_EQ(removed_stores, 1);
	EXPECT_EQ(stores_after, 1);
}

TEST_F(DSEPassTest, PreservesVolatileStores)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto local = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);

		auto volatile_store = builder->store(builder->literal(42), local);
		volatile_store->props |= NodeProps::NO_OPTIMIZE;

		builder->store(builder->literal(100), local);
		builder->ret(nullptr);
	});

	auto [removed_stores, stores_after] = run_dse();

	EXPECT_EQ(removed_stores, 0);
	EXPECT_EQ(stores_after, 2);
}

TEST_F(DSEPassTest, HandlesDifferentAddressSpaces)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		const auto local1 = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);
		const auto local2 = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);
		builder->store(builder->literal(42), local1);
		builder->store(builder->literal(100), local1);
		builder->store(builder->literal(200), local2);
		builder->load(local1, DataType::INT32);
		builder->load(local2, DataType::INT32);
		builder->ret(nullptr);
	});

	auto [removed_stores, stores_after] = run_dse();

	EXPECT_EQ(removed_stores, 1);
	EXPECT_EQ(stores_after, 2);
}

TEST_F(DSEPassTest, HandlesPtrStoreOperations)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.body([&]
	{
		auto local = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);

		builder->ptr_store(builder->literal(42), local);
		builder->ptr_store(builder->literal(100), local);
		builder->ptr_load(local, DataType::INT32);
		builder->ret(nullptr);
	});

	auto [removed_stores, stores_after] = run_dse();

	EXPECT_EQ(removed_stores, 1);
	EXPECT_EQ(stores_after, 1);
}
