/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <gtest/gtest.h>

class LocalAliasAnalysisFixture : public ::testing::Test
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

TEST_F(LocalAliasAnalysisFixture, NoAliasForDifferentAllocations)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *heap_alloc1 = nullptr;
	blm::Node *heap_alloc2 = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(16);

		heap_alloc1 = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
		heap_alloc2 = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	/* different allocations should not alias */
	EXPECT_EQ(aa_result->alias(heap_alloc1, heap_alloc2), blm::AliasResult::NO_ALIAS);
	EXPECT_FALSE(aa_result->may_alias(heap_alloc1, heap_alloc2));
}

TEST_F(LocalAliasAnalysisFixture, MustAliasForSamePointer)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *heap_alloc = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(16);

		heap_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	/* same pointer must alias with itself */
	EXPECT_EQ(aa_result->alias(heap_alloc, heap_alloc), blm::AliasResult::MUST_ALIAS);
	EXPECT_TRUE(aa_result->must_alias(heap_alloc, heap_alloc));
}

TEST_F(LocalAliasAnalysisFixture, PointerArithmeticWithConstantOffset)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *heap_alloc = nullptr;
	blm::Node *ptr_add = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(32);
		auto *offset = builder->literal(8);

		heap_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
		ptr_add = builder->ptr_add(heap_alloc, offset);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	const blm::MemoryLocation *loc_base = aa_result->get_location(heap_alloc);
	ASSERT_NE(loc_base, nullptr);
	EXPECT_EQ(loc_base->base, heap_alloc);
	EXPECT_EQ(loc_base->offset, 0);

	const blm::MemoryLocation *loc_offset = aa_result->get_location(ptr_add);
	ASSERT_NE(loc_offset, nullptr);
	EXPECT_EQ(loc_offset->base, heap_alloc);
	EXPECT_EQ(loc_offset->offset, 8);
}

TEST_F(LocalAliasAnalysisFixture, NoAliasForNonOverlappingOffsets)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *ptr_add1 = nullptr;
	blm::Node *ptr_add2 = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(32);
		auto *offset1 = builder->literal(0);
		auto *offset2 = builder->literal(8);

		auto *heap_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
		ptr_add1 = builder->ptr_add(heap_alloc, offset1);
		ptr_add2 = builder->ptr_add(heap_alloc, offset2);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	const blm::MemoryLocation *loc1 = aa_result->get_location(ptr_add1);
	const blm::MemoryLocation *loc2 = aa_result->get_location(ptr_add2);

	ASSERT_NE(loc1, nullptr);
	ASSERT_NE(loc2, nullptr);

	if (loc1->size == 4 && loc2->size == 4)
		EXPECT_EQ(aa_result->alias(ptr_add1, ptr_add2), blm::AliasResult::NO_ALIAS);
}

TEST_F(LocalAliasAnalysisFixture, PartialAliasForOverlappingOffsets)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *ptr_add1 = nullptr;
	blm::Node *ptr_add2 = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(32);
		auto *offset1 = builder->literal(0);
		auto *offset2 = builder->literal(2);

		auto *heap_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
		ptr_add1 = builder->ptr_add(heap_alloc, offset1);
		ptr_add2 = builder->ptr_add(heap_alloc, offset2);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	const blm::MemoryLocation *loc1 = aa_result->get_location(ptr_add1);
	const blm::MemoryLocation *loc2 = aa_result->get_location(ptr_add2);

	ASSERT_NE(loc1, nullptr);
	ASSERT_NE(loc2, nullptr);

	if (loc1->size == 4 && loc2->size == 4)
		EXPECT_EQ(aa_result->alias(ptr_add1, ptr_add2), blm::AliasResult::PARTIAL_ALIAS);
}

TEST_F(LocalAliasAnalysisFixture, AddressOfVariable)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", { blm::DataType::INT32 }, blm::DataType::VOID);

	blm::Node *addr_of = nullptr;
	blm::Node *param = nullptr;

	func.body([&]
	{
		param = func.add_parameter("value", blm::DataType::INT32);
		addr_of = builder->addr_of(param);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	const blm::MemoryLocation *loc = aa_result->get_location(addr_of);
	ASSERT_NE(loc, nullptr);
	EXPECT_EQ(loc->base, param);
	EXPECT_EQ(loc->offset, 0);
}

TEST_F(LocalAliasAnalysisFixture, PointerParameters)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", { blm::DataType::POINTER }, blm::DataType::VOID);

	blm::Node *ptr_param = nullptr;

	func.body([&]
	{
		ptr_param = func.add_parameter("ptr", blm::DataType::POINTER);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	const blm::MemoryLocation *loc = aa_result->get_location(ptr_param);
	ASSERT_NE(loc, nullptr);
	EXPECT_EQ(loc->base, ptr_param);
	EXPECT_EQ(loc->offset, 0);
	EXPECT_TRUE(aa_result->has_escaped(ptr_param));
}

TEST_F(LocalAliasAnalysisFixture, AllocationSiteTracking)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *heap_alloc = nullptr;
	blm::Node *stack_alloc = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(16);

		heap_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
		stack_alloc = builder->stack_alloc(size, blm::DataType::INT32);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	EXPECT_TRUE(aa_result->is_allocation_site(heap_alloc));
	EXPECT_TRUE(aa_result->is_allocation_site(stack_alloc));
}

TEST_F(LocalAliasAnalysisFixture, ComplexAllocationWithSize)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *heap_alloc = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(16);

		heap_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	EXPECT_TRUE(aa_result->is_allocation_site(heap_alloc));

	const blm::MemoryLocation *loc = aa_result->get_location(heap_alloc);
	ASSERT_NE(loc, nullptr);
	EXPECT_EQ(loc->base, heap_alloc);
	EXPECT_EQ(loc->size, 16);
}

TEST_F(LocalAliasAnalysisFixture, MixedStackAndHeapAllocations)
{
	auto *module = builder->create_module("test_module");
	auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

	blm::Node *heap_alloc = nullptr;
	blm::Node *stack_alloc = nullptr;

	func.body([&]
	{
		auto *malloc_fn = builder->create_function("malloc", { blm::DataType::INT32 }, blm::DataType::POINTER).
				get_function();
		auto *size = builder->literal(32);

		heap_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
		stack_alloc = builder->stack_alloc(size, blm::DataType::INT32);
	});

	blm::PassContext pass_ctx(*module, 1);
	blm::LocalAliasAnalysisPass aa;
	const auto result = aa.analyze(*module, pass_ctx);

	auto *aa_result = dynamic_cast<blm::LocalAliasResult *>(result.get());
	ASSERT_NE(aa_result, nullptr);

	EXPECT_TRUE(aa_result->is_allocation_site(heap_alloc));
	EXPECT_TRUE(aa_result->is_allocation_site(stack_alloc));
	EXPECT_EQ(aa_result->alias(heap_alloc, stack_alloc), blm::AliasResult::NO_ALIAS);
}

/* Enhanced LAA Tests - Store/Load Dependency Analysis */

TEST_F(LocalAliasAnalysisFixture, StoreLoadDependencyBasic)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

    blm::Node* store_node = nullptr;
    blm::Node* load_node = nullptr;

    func.body([&] {
        auto* malloc_fn = builder->create_function("malloc", {blm::DataType::INT32}, blm::DataType::POINTER).get_function();
        auto* size = builder->literal(16);
        auto* value = builder->literal(42);

        auto* ptr = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
        store_node = builder->store(value, ptr);
        load_node = builder->load(ptr, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);

    EXPECT_TRUE(aa_result->maybe_modified_by(load_node, store_node));

    auto affecting_stores = aa_result->get_affecting_stores(load_node);
    EXPECT_EQ(affecting_stores.size(), 1);
    EXPECT_EQ(affecting_stores[0], store_node);

    auto affected_loads = aa_result->get_affected_loads(store_node);
    EXPECT_EQ(affected_loads.size(), 1);
    EXPECT_EQ(affected_loads[0], load_node);
}

TEST_F(LocalAliasAnalysisFixture, IndependentStoreLoad)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

    blm::Node* store_node = nullptr;
    blm::Node* load_node = nullptr;

    func.body([&] {
        auto* malloc_fn = builder->create_function("malloc", {blm::DataType::INT32}, blm::DataType::POINTER).get_function();
        auto* size = builder->literal(16);
        auto* value = builder->literal(42);

        auto* ptr1 = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
        auto* ptr2 = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);

        store_node = builder->store(value, ptr1);
        load_node = builder->load(ptr2, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);

    EXPECT_FALSE(aa_result->maybe_modified_by(load_node, store_node));

    auto affecting_stores = aa_result->get_affecting_stores(load_node);
    EXPECT_EQ(affecting_stores.size(), 0);

    auto affected_loads = aa_result->get_affected_loads(store_node);
    EXPECT_EQ(affected_loads.size(), 0);
}

TEST_F(LocalAliasAnalysisFixture, PointerArithmeticDependency)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

    blm::Node* store_node = nullptr;
    blm::Node* load1_node = nullptr;
    blm::Node* load2_node = nullptr;

    func.body([&] {
        auto* malloc_fn = builder->create_function("malloc", {blm::DataType::INT32}, blm::DataType::POINTER).get_function();
        auto* size = builder->literal(32);
        auto* value = builder->literal(42);
        auto* offset = builder->literal(8);

        auto* base_ptr = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
        auto* offset_ptr = builder->ptr_add(base_ptr, offset);

        store_node = builder->store(value, base_ptr);
        load1_node = builder->load(base_ptr, blm::DataType::INT32);
        load2_node = builder->load(offset_ptr, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);

    EXPECT_TRUE(aa_result->maybe_modified_by(load1_node, store_node));

    auto affecting_stores_load2 = aa_result->get_affecting_stores(load2_node);
    auto affecting_stores_load1 = aa_result->get_affecting_stores(load1_node);
    EXPECT_EQ(affecting_stores_load1.size(), 1);
    EXPECT_EQ(affecting_stores_load1[0], store_node);
}

TEST_F(LocalAliasAnalysisFixture, MultipleStoresSameLocation)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

    blm::Node* store1_node = nullptr;
    blm::Node* store2_node = nullptr;
    blm::Node* load_node = nullptr;

    func.body([&] {
        auto* malloc_fn = builder->create_function("malloc", {blm::DataType::INT32}, blm::DataType::POINTER).get_function();
        auto* size = builder->literal(16);
        auto* value1 = builder->literal(42);
        auto* value2 = builder->literal(84);

        auto* ptr = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);

        store1_node = builder->store(value1, ptr);
        store2_node = builder->store(value2, ptr);
        load_node = builder->load(ptr, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);

    EXPECT_TRUE(aa_result->maybe_modified_by(load_node, store1_node));
    EXPECT_TRUE(aa_result->maybe_modified_by(load_node, store2_node));

    auto affecting_stores = aa_result->get_affecting_stores(load_node);
    EXPECT_EQ(affecting_stores.size(), 2);

    auto affected_by_store1 = aa_result->get_affected_loads(store1_node);
    auto affected_by_store2 = aa_result->get_affected_loads(store2_node);
    EXPECT_EQ(affected_by_store1.size(), 1);
    EXPECT_EQ(affected_by_store2.size(), 1);
    EXPECT_EQ(affected_by_store1[0], load_node);
    EXPECT_EQ(affected_by_store2[0], load_node);
}

TEST_F(LocalAliasAnalysisFixture, ParameterEscapeAnalysis)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {blm::DataType::POINTER}, blm::DataType::VOID);

    blm::Node* param_ptr = nullptr;
    blm::Node* store_node = nullptr;
    blm::Node* load_node = nullptr;

    func.body([&] {
        param_ptr = func.add_parameter("ptr", blm::DataType::POINTER);
        auto* value = builder->literal(42);

        store_node = builder->store(value, param_ptr);
        load_node = builder->load(param_ptr, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);

    EXPECT_TRUE(aa_result->has_escaped(param_ptr));
	EXPECT_TRUE(aa_result->maybe_modified_by(load_node, store_node));
}

TEST_F(LocalAliasAnalysisFixture, ComplexPointerChains)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

    blm::Node* store_node = nullptr;
    blm::Node* load_node = nullptr;
    blm::Node* original_alloc = nullptr;
    blm::Node* derived_ptr = nullptr;

    func.body([&] {
        auto* malloc_fn = builder->create_function("malloc", {blm::DataType::INT32}, blm::DataType::POINTER).get_function();
        auto* size = builder->literal(32);
        auto* value = builder->literal(42);
        auto* offset = builder->literal(4);

        original_alloc = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
        derived_ptr = builder->ptr_add(original_alloc, offset);

        store_node = builder->store(value, original_alloc);
        load_node = builder->load(derived_ptr, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);

    EXPECT_TRUE(aa_result->is_allocation_site(original_alloc));

    const blm::MemoryLocation* base_loc = aa_result->get_location(original_alloc);
    const blm::MemoryLocation* derived_loc = aa_result->get_location(derived_ptr);

    ASSERT_NE(base_loc, nullptr);
    ASSERT_NE(derived_loc, nullptr);

    EXPECT_EQ(base_loc->base, original_alloc);
    EXPECT_EQ(base_loc->offset, 0);

    EXPECT_EQ(derived_loc->base, original_alloc);
    EXPECT_EQ(derived_loc->offset, 4);
}

TEST_F(LocalAliasAnalysisFixture, MixedStackHeapAnalysis)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

    blm::Node* heap_store = nullptr;
    blm::Node* stack_store = nullptr;
    blm::Node* heap_load = nullptr;
    blm::Node* stack_load = nullptr;

    func.body([&] {
        auto* malloc_fn = builder->create_function("malloc", {blm::DataType::INT32}, blm::DataType::POINTER).get_function();
        auto* size = builder->literal(16);
        auto* value = builder->literal(42);

        auto* heap_ptr = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
        auto* stack_ptr = builder->stack_alloc(size, blm::DataType::INT32);

        heap_store = builder->store(value, heap_ptr);
        stack_store = builder->store(value, stack_ptr);
        heap_load = builder->load(heap_ptr, blm::DataType::INT32);
        stack_load = builder->load(stack_ptr, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);

    EXPECT_TRUE(aa_result->maybe_modified_by(heap_load, heap_store));
    EXPECT_FALSE(aa_result->maybe_modified_by(stack_load, heap_store));
	EXPECT_TRUE(aa_result->maybe_modified_by(stack_load, stack_store));
    EXPECT_FALSE(aa_result->maybe_modified_by(heap_load, stack_store));
}

TEST_F(LocalAliasAnalysisFixture, AtomicOperations)
{
    auto* module = builder->create_module("test_module");
    auto func = builder->create_function("test_function", {}, blm::DataType::VOID);

    blm::Node* atomic_store = nullptr;
    blm::Node* atomic_load = nullptr;
    blm::Node* regular_load = nullptr;

    func.body([&]
    {
        auto* malloc_fn = builder->create_function("malloc", {blm::DataType::INT32}, blm::DataType::POINTER).get_function();
        auto* size = builder->literal(16);
        auto* value = builder->literal(42);

        auto* ptr = builder->heap_alloc(malloc_fn, size, blm::DataType::INT32);
    	atomic_store = builder->get_current_region()->create_node<blm::Node>();
        atomic_store->ir_type = blm::NodeType::ATOMIC_STORE;
        atomic_store->inputs.push_back(value);
        atomic_store->inputs.push_back(ptr);
        value->users.push_back(atomic_store);
        ptr->users.push_back(atomic_store);

        atomic_load = builder->get_current_region()->create_node<blm::Node>();
        atomic_load->ir_type = blm::NodeType::ATOMIC_LOAD;
        atomic_load->type_kind = blm::DataType::INT32;
        atomic_load->inputs.push_back(ptr);
        ptr->users.push_back(atomic_load);

        regular_load = builder->load(ptr, blm::DataType::INT32);
    });

    blm::PassContext pass_ctx(*module, 1);
    blm::LocalAliasAnalysisPass aa;
    auto result = aa.analyze(*module, pass_ctx);

    auto* aa_result = dynamic_cast<blm::LocalAliasResult*>(result.get());
    ASSERT_NE(aa_result, nullptr);
	EXPECT_TRUE(aa_result->maybe_modified_by(atomic_load, atomic_store));
    EXPECT_TRUE(aa_result->maybe_modified_by(regular_load, atomic_store));
}
