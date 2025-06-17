/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/ir/builder.hpp>
#include <gtest/gtest.h>

using namespace blm;

class BuilderTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ctx = std::make_unique<Context>();
		builder = std::make_unique<Builder>(*ctx);
	}

	void TearDown() override
	{
		builder.reset();
		ctx.reset();
	}

	std::unique_ptr<Context> ctx;
	std::unique_ptr<Builder> builder;
};

TEST_F(BuilderTest, CreateModule)
{
	auto *module = builder->create_module("test_module");

	ASSERT_NE(module, nullptr);
	EXPECT_EQ(module->get_name(), "test_module");
	EXPECT_EQ(builder->get_current_module(), module);
}

TEST_F(BuilderTest, CreateSimpleFunction)
{
	builder->create_module("test");
	const auto func = builder->create_function("add", { DataType::INT32, DataType::INT32 }, DataType::INT32);

	EXPECT_EQ(func.get_function()->ir_type, NodeType::FUNCTION);
	EXPECT_EQ(func.get_function()->type_kind,
	          builder->function_type(DataType::INT32, {DataType::INT32, DataType::INT32}));
}

TEST_F(BuilderTest, FunctionWithParameters)
{
	builder->create_module("test");
	auto func = builder->create_function("add", { DataType::INT32, DataType::INT32 }, DataType::INT32);

	auto *a = func.add_parameter("a", DataType::INT32);
	auto *b = func.add_parameter("b", DataType::INT32);

	ASSERT_NE(a, nullptr);
	ASSERT_NE(b, nullptr);
	EXPECT_EQ(a->ir_type, NodeType::PARAM);
	EXPECT_EQ(b->ir_type, NodeType::PARAM);
	EXPECT_EQ(a->type_kind, DataType::INT32);
	EXPECT_EQ(b->type_kind, DataType::INT32);
}

TEST_F(BuilderTest, IntegerLiterals)
{
	builder->create_module("test");

	auto *lit8 = builder->literal(static_cast<std::int8_t>(42));
	auto *lit32 = builder->literal(42);
	auto *lit64 = builder->literal(static_cast<std::int64_t>(42));

	EXPECT_EQ(lit8->ir_type, NodeType::LIT);
	EXPECT_EQ(lit8->type_kind, DataType::INT8);
	EXPECT_EQ(lit8->as<DataType::INT8>(), 42);

	EXPECT_EQ(lit32->ir_type, NodeType::LIT);
	EXPECT_EQ(lit32->type_kind, DataType::INT32);
	EXPECT_EQ(lit32->as<DataType::INT32>(), 42);

	EXPECT_EQ(lit64->ir_type, NodeType::LIT);
	EXPECT_EQ(lit64->type_kind, DataType::INT64);
	EXPECT_EQ(lit64->as<DataType::INT64>(), 42);
}

TEST_F(BuilderTest, FloatingPointLiterals)
{
	builder->create_module("test");

	auto *float_lit = builder->literal(3.14f);
	auto *double_lit = builder->literal(3.14159);

	EXPECT_EQ(float_lit->ir_type, NodeType::LIT);
	EXPECT_EQ(float_lit->type_kind, DataType::FLOAT32);
	EXPECT_FLOAT_EQ(float_lit->as<DataType::FLOAT32>(), 3.14f);

	EXPECT_EQ(double_lit->ir_type, NodeType::LIT);
	EXPECT_EQ(double_lit->type_kind, DataType::FLOAT64);
	EXPECT_DOUBLE_EQ(double_lit->as<DataType::FLOAT64>(), 3.14159);
}

TEST_F(BuilderTest, BooleanLiterals)
{
	builder->create_module("test");

	auto *true_lit = builder->literal(true);
	auto *false_lit = builder->literal(false);

	EXPECT_EQ(true_lit->ir_type, NodeType::LIT);
	EXPECT_EQ(true_lit->type_kind, DataType::BOOL);
	EXPECT_TRUE(true_lit->as<DataType::BOOL>());

	EXPECT_EQ(false_lit->ir_type, NodeType::LIT);
	EXPECT_EQ(false_lit->type_kind, DataType::BOOL);
	EXPECT_FALSE(false_lit->as<DataType::BOOL>());
}

TEST_F(BuilderTest, ArithmeticOperations)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::INT32);

	func.body([&]
	{
		auto *a = builder->literal(10);
		auto *b = builder->literal(20);

		auto *add_result = builder->add(a, b);
		auto *sub_result = builder->sub(a, b);
		auto *mul_result = builder->mul(a, b);
		auto *div_result = builder->div(a, b);

		EXPECT_EQ(add_result->ir_type, NodeType::ADD);
		EXPECT_EQ(add_result->type_kind, DataType::INT32);
		EXPECT_EQ(add_result->inputs.size(), 2);
		EXPECT_EQ(add_result->inputs[0], a);
		EXPECT_EQ(add_result->inputs[1], b);

		EXPECT_EQ(sub_result->ir_type, NodeType::SUB);
		EXPECT_EQ(mul_result->ir_type, NodeType::MUL);
		EXPECT_EQ(div_result->ir_type, NodeType::DIV);
	});
}

TEST_F(BuilderTest, TypeInference)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::FLOAT64);

	func.body([&]
	{
		auto *int_val = builder->literal(10);
		auto *float_val = builder->literal(3.14f);
		auto *double_val = builder->literal(2.718);

		auto *mixed_result = builder->add(int_val, float_val);
		auto *double_result = builder->add(float_val, double_val);

		/* int + float = float */
		EXPECT_EQ(mixed_result->type_kind, DataType::FLOAT32);

		/* float + double = double */
		EXPECT_EQ(double_result->type_kind, DataType::FLOAT64);
	});
}

TEST_F(BuilderTest, ComparisonOperations)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::BOOL);

	func.body([&]
	{
		auto *a = builder->literal(10);
		auto *b = builder->literal(20);

		auto *eq_result = builder->eq(a, b);
		auto *neq_result = builder->neq(a, b);
		auto *lt_result = builder->lt(a, b);
		auto *lte_result = builder->lte(a, b);
		auto *gt_result = builder->gt(a, b);
		auto *gte_result = builder->gte(a, b);

		EXPECT_EQ(eq_result->type_kind, DataType::BOOL);
		EXPECT_EQ(neq_result->type_kind, DataType::BOOL);
		EXPECT_EQ(lt_result->type_kind, DataType::BOOL);
		EXPECT_EQ(lte_result->type_kind, DataType::BOOL);
		EXPECT_EQ(gt_result->type_kind, DataType::BOOL);
		EXPECT_EQ(gte_result->type_kind, DataType::BOOL);
		EXPECT_EQ(eq_result->inputs[0], a);
		EXPECT_EQ(eq_result->inputs[1], b);
	});
}

TEST_F(BuilderTest, MemoryAllocation)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::VOID);
	auto memoryalloc = builder->create_function("malloc", { DataType::INT32 }, DataType::VOID);
	memoryalloc.get_function()->props |= NodeProps::EXPORT;

	func.body([&]
	{
	   auto *size = builder->literal(1024);
	   auto alignment = 16u;

	   auto *stack_ptr = builder->stack_alloc(size, DataType::INT32, alignment);
	   auto *heap_ptr = builder->heap_alloc(memoryalloc.get_function(),
		  size, DataType::INT32, alignment);

	   EXPECT_EQ(stack_ptr->ir_type, NodeType::STACK_ALLOC);
	   EXPECT_EQ(heap_ptr->ir_type, NodeType::HEAP_ALLOC);

	   auto expected_type = builder->pointer_type(DataType::INT32);
	   EXPECT_EQ(stack_ptr->type_kind, expected_type);
	   EXPECT_EQ(heap_ptr->type_kind, expected_type);

	   EXPECT_EQ(stack_ptr->inputs[0], size);

	   EXPECT_EQ(heap_ptr->inputs[0], memoryalloc.get_function());
	   EXPECT_EQ(heap_ptr->inputs[1], size);

	   if (alignment > 0)
	   {
		  EXPECT_EQ(stack_ptr->inputs.size(), 2);
		  EXPECT_EQ(heap_ptr->inputs.size(), 3);
	   }
	});
}

TEST_F(BuilderTest, LoadStoreOperations)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::VOID);

	func.body([&]
	{
		auto *size = builder->literal(4);
		auto *ptr = builder->stack_alloc(size, DataType::INT32);
		auto *value = builder->literal(42);

		auto *store_op = builder->store(value, ptr);
		auto *load_op = builder->load(ptr, DataType::INT32);

		EXPECT_EQ(store_op->ir_type, NodeType::STORE);
		EXPECT_EQ(load_op->ir_type, NodeType::LOAD);

		/* check operand conventions */
		/* STORE: inputs[0] = value, inputs[1] = address */
		EXPECT_EQ(store_op->inputs[0], value);
		EXPECT_EQ(store_op->inputs[1], ptr);

		/* LOAD: inputs[0] = address */
		EXPECT_EQ(load_op->inputs[0], ptr);
		EXPECT_EQ(load_op->type_kind, DataType::INT32);
	});
}

TEST_F(BuilderTest, PointerOperations)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::VOID);

	func.body([&]
	{
		auto *size = builder->literal(4);
		auto *ptr = builder->stack_alloc(size, DataType::INT32);
		auto *offset = builder->literal(1);
		auto *value = builder->literal(42);

		auto *ptr_add_result = builder->ptr_add(ptr, offset);
		auto *store_op = builder->ptr_store(value, ptr_add_result);
		auto *load_op = builder->ptr_load(ptr_add_result, DataType::INT32);

		/* check operand conventions */
		/* PTR_ADD: inputs[0] = base_ptr, inputs[1] = offset */
		EXPECT_EQ(ptr_add_result->inputs[0], ptr);
		EXPECT_EQ(ptr_add_result->inputs[1], offset);

		/* PTR_STORE: inputs[0] = value, inputs[1] = pointer */
		EXPECT_EQ(store_op->inputs[0], value);
		EXPECT_EQ(store_op->inputs[1], ptr_add_result);

		/* PTR_LOAD: inputs[0] = pointer */
		EXPECT_EQ(load_op->inputs[0], ptr_add_result);
	});
}

TEST_F(BuilderTest, FunctionCalls)
{
	builder->create_module("test");

	auto callee = builder->create_function("callee", { DataType::INT32, DataType::INT32 }, DataType::INT32);
	auto caller = builder->create_function("caller", {}, DataType::INT32);

	caller.body([&]
	{
		auto *arg1 = builder->literal(10);
		auto *arg2 = builder->literal(20);

		auto *call_result = builder->call(callee.get_function(), { arg1, arg2 });

		EXPECT_EQ(call_result->ir_type, NodeType::CALL);
		EXPECT_EQ(call_result->type_kind, DataType::INT32);

		/* check operand conventions: function, args... */
		EXPECT_EQ(call_result->inputs[0], callee.get_function());
		EXPECT_EQ(call_result->inputs[1], arg1);
		EXPECT_EQ(call_result->inputs[2], arg2);

		builder->ret(call_result);
	});
}

TEST_F(BuilderTest, ControlFlowInstructions)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::VOID);

	func.body([&]
	{
		auto *condition = builder->literal(true);

		auto true_block = builder->create_block("true_block");
		auto false_block = builder->create_block("false_block");
		auto exit_block = builder->create_block("exit_block");

		auto *true_entry = true_block.get_region()->get_nodes()[0];
		auto *false_entry = false_block.get_region()->get_nodes()[0];
		auto *exit_entry = exit_block.get_region()->get_nodes()[0];

		auto *branch_instr = builder->branch(condition, true_entry, false_entry);

		EXPECT_EQ(branch_instr->ir_type, NodeType::BRANCH);
		/* check operand conventions: condition, true_target, false_target */
		EXPECT_EQ(branch_instr->inputs[0], condition);
		EXPECT_EQ(branch_instr->inputs[1], true_entry);
		EXPECT_EQ(branch_instr->inputs[2], false_entry);

		/* test jump instruction */
		true_block([&]
		{
			auto *jump_instr = builder->jump(exit_entry);
			EXPECT_EQ(jump_instr->ir_type, NodeType::JUMP);
			EXPECT_EQ(jump_instr->inputs[0], exit_entry);
		});
	});
}

TEST_F(BuilderTest, HighLevelControlFlow)
{
	builder->create_module("test");
	auto func = builder->create_function("test", {}, DataType::INT32);

	func.body([&]
	{
		auto *condition = builder->literal(true);

		auto [true_branch, false_branch] = builder->create_if(condition, "then", "else");

		true_branch([&]
		{
			auto *result = builder->literal(1);
			builder->ret(result);
		});

		false_branch([&]
		{
			auto *result = builder->literal(0);
			builder->ret(result);
		});
	});
}

TEST_F(BuilderTest, InvokeInstructions)
{
	builder->create_module("test");

	auto risky_func = builder->create_function("risky", { DataType::INT32 }, DataType::INT32);
	auto caller = builder->create_function("caller", { DataType::INT32 }, DataType::INT32);

	caller.body([&]
	{
		auto *param = builder->literal(42);

		auto [normal, except] = builder->create_invoke_blocks("normal", "except");

		auto *normal_entry = normal.get_region()->get_nodes()[0];
		auto *except_entry = except.get_region()->get_nodes()[0];

		auto *invoke_result = builder->invoke(risky_func.get_function(), { param },
		                                      normal_entry, except_entry);

		EXPECT_EQ(invoke_result->ir_type, NodeType::INVOKE);
		/* check operand conventions: function, args..., normal_target, except_target */
		EXPECT_EQ(invoke_result->inputs[0], risky_func.get_function());
		EXPECT_EQ(invoke_result->inputs[1], param);
		EXPECT_EQ(invoke_result->inputs[2], normal_entry);
		EXPECT_EQ(invoke_result->inputs[3], except_entry);

		normal([&]
		{
			builder->ret(invoke_result);
		});

		except([&]
		{
			auto *error_code = builder->literal(-1);
			builder->ret(error_code);
		});
	});
}

TEST_F(BuilderTest, TypeCreation)
{
	builder->create_module("test");

	auto func_type = builder->function_type(DataType::INT32, { DataType::INT32, DataType::INT32 });
	EXPECT_NE(func_type, DataType::VOID);

	auto ptr_type = builder->pointer_type(DataType::INT32);
	EXPECT_NE(ptr_type, DataType::VOID);

	auto array_type = builder->array_type(DataType::INT32, 10);
	EXPECT_NE(array_type, DataType::VOID);

	std::vector<std::pair<std::string, DataType> > fields = {
		{ "x", DataType::INT32 },
		{ "y", DataType::INT32 }
	};
	auto struct_type = builder->struct_type(fields, 8, 4);
	EXPECT_NE(struct_type, DataType::VOID);
}

TEST_F(BuilderTest, ErrorConditions)
{
	/* creating function without module should throw */
	EXPECT_THROW(builder->create_function("test", {}, DataType::VOID), std::runtime_error);
}

TEST_F(BuilderTest, CompleteFunction)
{
	builder->create_module("test");
	auto func = builder->create_function("fibonacci", { DataType::INT32 }, DataType::INT32);

	auto *n = func.add_parameter("n", DataType::INT32);

	func.body([&]
	{
		builder->literal(0);
		auto *one = builder->literal(1);
		auto *two = builder->literal(2);

		/* if (n <= 1) return n; */
		auto *condition = builder->lte(n, one);
		auto [base_case, recursive_case] = builder->create_if(condition, "base", "recursive");

		base_case([&]
		{
			builder->ret(n);
		});

		recursive_case([&]
		{
			/* return fib(n-1) + fib(n-2) */
			auto *n_minus_1 = builder->sub(n, one);
			auto *n_minus_2 = builder->sub(n, two);

			auto *fib_n_1 = builder->call(func.get_function(), { n_minus_1 });
			auto *fib_n_2 = builder->call(func.get_function(), { n_minus_2 });

			auto *result = builder->add(fib_n_1, fib_n_2);
			builder->ret(result);
		});
	});

	EXPECT_EQ(func.get_function()->ir_type, NodeType::FUNCTION);
	EXPECT_FALSE(func.get_region()->get_nodes().empty());
}
