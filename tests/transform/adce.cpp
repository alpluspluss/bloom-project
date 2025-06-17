/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/ir/print.hpp>
#include <bloom/transform/adce.hpp>
#include <bloom/transform/constfold.hpp>
#include <gtest/gtest.h>

using namespace blm;

class ADCEPassTest : public ::testing::Test
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

	std::pair<std::size_t, std::size_t> run_adce()
	{
		PassContext pass_ctx(*module);
		ConstantFoldingPass cf;
		cf.run(*module, pass_ctx);
		ADCEPass adce;
		adce.run(*module, pass_ctx);

		return {
			pass_ctx.get_stat("adce.removed_regions"),
			pass_ctx.get_stat("adce.removed_nodes")
		};
	}

	std::size_t count_regions(const Region *region = nullptr) const
	{
		if (!region)
			region = module->get_root_region();

		std::size_t count = 1;
		for (const Region *child: region->get_children())
			count += count_regions(child);

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

	std::unique_ptr<Context> ctx;
	std::unique_ptr<Builder> builder;
	Module *module = nullptr;
};

TEST_F(ADCEPassTest, RemovesUnreachableRegions)
{
	auto main_func = builder->create_function("main", {}, DataType::INT32);
	main_func.body([&]
	{
		auto cond = builder->literal(false); /* constant false condition */
		auto [true_block, false_block] = builder->create_if(cond, "unreachable", "reachable");
		true_block([&]
		{
			auto dead_var = builder->literal(42);
			auto dead_add = builder->add(dead_var, dead_var);
			true_block.ret(dead_add);
		});

		false_block([&]
		{
			auto result = builder->literal(0);
			false_block.ret(result);
		});
	});

	std::cout << "before ADCE\n";
	IRPrinter printer_before(std::cout);
	printer_before.print_module(*module);

	std::size_t regions_before = count_regions();
	std::size_t nodes_before = count_nodes();

	auto [removed_regions, removed_nodes] = run_adce();

	std::cout << "\nafter ADCE\n";
	IRPrinter printer_after(std::cout);
	printer_after.print_module(*module);
	std::cout << "\nremoved " << removed_regions << " regions, " << removed_nodes << " nodes\n\n";

	EXPECT_GT(removed_regions, 0);
	EXPECT_GT(removed_nodes, 0);
	EXPECT_LT(count_regions(), regions_before);
	EXPECT_LT(count_nodes(), nodes_before);
}

TEST_F(ADCEPassTest, RemovesDeadNodesInLiveRegions)
{
	auto func = builder->create_function("test", {}, DataType::INT32);
	func.get_function()->props |= NodeProps::DRIVER;
	func.add_parameter("x", DataType::INT32);

	func.body([&]
	{
		const auto dead1 = builder->literal(10);
		const auto dead2 = builder->literal(20);
		builder->add(dead1, dead2);

		auto live_result = builder->literal(42);
		builder->ret(live_result);
	});

	std::cout << "before ADCE\n";
	IRPrinter printer_before(std::cout);
	printer_before.print_module(*module);

	auto [removed_regions, removed_nodes] = run_adce();

	std::cout << "\nafter ADCE\n";
	IRPrinter printer_after(std::cout);
	printer_after.print_module(*module);
	std::cout << "\nremoved " << removed_regions << " regions, " << removed_nodes << " nodes\n\n";

	EXPECT_EQ(removed_regions, 0);
	EXPECT_GT(removed_nodes, 0);
}

TEST_F(ADCEPassTest, PreservesCriticalNodes)
{
	/* create function with side effects */
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.get_function()->props |= NodeProps::DRIVER;
	func.body([&]
	{
		auto ptr = builder->stack_alloc(builder->literal(static_cast<std::uint32_t>(4)), DataType::INT32);
		auto value = builder->literal(42);
		builder->store(value, ptr);
		builder->ret(nullptr);
	});

	auto [removed_regions, removed_nodes] = run_adce();
	EXPECT_EQ(removed_regions, 0);
	EXPECT_EQ(removed_nodes, 0);
}

TEST_F(ADCEPassTest, FollowsControlFlowThroughCalls)
{
	/* create caller and callee functions */
	auto callee = builder->create_function("callee", {}, DataType::INT32);
	callee.body([&]
	{
		auto result = builder->literal(100);
		builder->ret(result);
	});

	auto caller = builder->create_function("caller", {}, DataType::INT32);
	caller.get_function()->props |= NodeProps::DRIVER;
	caller.body([&]
	{
		auto call_result = builder->call(callee.get_function(), {});
		builder->ret(call_result);
	});

	auto [removed_regions, removed_nodes] = run_adce();
	EXPECT_EQ(removed_regions, 0);
	EXPECT_EQ(removed_nodes, 0);
}

TEST_F(ADCEPassTest, NoRemoveUnreachableFunction)
{
	auto main_func = builder->create_function("main", {}, DataType::INT32);
	main_func.body([&]
	{
		builder->ret(builder->literal(0));
	});

	auto dead_func = builder->create_function("dead", {}, DataType::INT32);
	dead_func.body([&]
	{
		auto computation = builder->add(builder->literal(1), builder->literal(2));
		builder->ret(computation);
	});

	const std::size_t regions_before = count_regions();
	std::cout << "before ADCE\n";
	IRPrinter printer_before(std::cout);
	printer_before.print_module(*module);

	auto [removed_regions, removed_nodes] = run_adce();

	std::cout << "\nafter ADCE\n";
	IRPrinter printer_after(std::cout);
	printer_after.print_module(*module);

	EXPECT_EQ(removed_regions, 0);
	EXPECT_EQ(count_regions(), regions_before);
}

TEST_F(ADCEPassTest, HandlesJumpTargets)
{
	auto func = builder->create_function("test", {}, DataType::VOID);
	func.get_function()->props |= NodeProps::DRIVER;
	func.body([&]
	{
		auto target_block = builder->create_block("target");
		auto unreachable_block = builder->create_block("unreachable");

		/* jump directly to target, bypassing unreachable */
		auto target_entry = target_block.get_region()->get_nodes()[0]; /* entry node */
		builder->jump(target_entry);

		/* target block is reachable via jump */
		target_block([&]
		{
			builder->ret(nullptr);
		});

		/* this block is never reached */
		unreachable_block([&]
		{
			builder->literal(999);
			builder->ret(nullptr);
		});
	});

	std::cout << "before ADCE\n";
	IRPrinter printer_before(std::cout);
	printer_before.print_module(*module);

	auto [removed_regions, removed_nodes] = run_adce();

	std::cout << "\nafter ADCE\n";
	IRPrinter printer_after(std::cout);
	printer_after.print_module(*module);
	std::cout << "\nremoved " << removed_regions << " regions, " << removed_nodes << " nodes\n\n";
	EXPECT_EQ(removed_regions, 1);
}
