/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/ir/builder.hpp>
#include <bloom/support/bt.hpp>
#include <gtest/gtest.h>

using namespace blm;

class TestBTContext
{
public:
	/* required by BTContext concept */
	void mark_changed()
	{
		made_changes = true;
	}

	/* required by BTContext concept */
	[[nodiscard]] bool has_changed() const
	{
		return made_changes;
	}

	/* required by BTContext concept */
	void reset_changed()
	{
		made_changes = false;
	}

	/* test specific methods */
	void set_node(Node *node)
	{
		current_node = node;
	}

	[[nodiscard]] Node *get_node() const
	{
		return current_node;
	}

	void set_last_action(const std::string &action)
	{
		last_action = action;
	}

	[[nodiscard]] const std::string &get_last_action() const
	{
		return last_action;
	}

	/* fast accessors */
	[[nodiscard]] Node *left() const
	{
		return (current_node && !current_node->inputs.empty()) ? current_node->inputs[0] : nullptr;
	}

	[[nodiscard]] Node *right() const
	{
		return (current_node && current_node->inputs.size() >= 2) ? current_node->inputs[1] : nullptr;
	}

	[[nodiscard]] bool is_type(NodeType type) const
	{
		return current_node && current_node->ir_type == type;
	}

	[[nodiscard]] bool is_add() const
	{
		return is_type(NodeType::ADD);
	}

	[[nodiscard]] bool is_mul() const
	{
		return is_type(NodeType::MUL);
	}

	[[nodiscard]] bool is_sub() const
	{
		return is_type(NodeType::SUB);
	}

	[[nodiscard]] bool right_is_zero() const
	{
		Node *r = right();
		return r && r->ir_type == NodeType::LIT && is_zero_literal(r);
	}

	[[nodiscard]] bool right_is_one() const
	{
		Node *r = right();
		return r && r->ir_type == NodeType::LIT && is_one_literal(r);
	}

private:
	Node *current_node = nullptr;
	bool made_changes = false;
	std::string last_action;

	static bool is_zero_literal(Node *node)
	{
		if (!node || node->ir_type != NodeType::LIT)
			return false;

		switch (node->type_kind)
		{
			case DataType::INT32:
				return node->as<DataType::INT32>() == 0;
			case DataType::UINT32:
				return node->as<DataType::UINT32>() == 0;
			default:
				return false;
		}
	}

	static bool is_one_literal(Node *node)
	{
		if (!node || node->ir_type != NodeType::LIT)
			return false;

		switch (node->type_kind)
		{
			case DataType::INT32:
				return node->as<DataType::INT32>() == 1;
			case DataType::UINT32:
				return node->as<DataType::UINT32>() == 1;
			default:
				return false;
		}
	}
};

class BehaviorTreeTest : public ::testing::Test
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

	Node *create_binary_op(NodeType op_type, Node *left, Node *right)
	{
		Node *node = ctx->create<Node>();
		node->ir_type = op_type;
		node->type_kind = left->type_kind;
		node->inputs = { left, right };
		left->users.push_back(node);
		right->users.push_back(node);
		return node;
	}

	Node *create_literal(std::int32_t value)
	{
		Node *node = ctx->create<Node>();
		node->ir_type = NodeType::LIT;
		node->type_kind = DataType::INT32;
		node->data.set<std::int32_t, DataType::INT32>(value);
		return node;
	}

	Node *create_literal_u32(std::uint32_t value)
	{
		Node *node = ctx->create<Node>();
		node->ir_type = NodeType::LIT;
		node->type_kind = DataType::UINT32;
		node->data.set<std::uint32_t, DataType::UINT32>(value);
		return node;
	}

	std::size_t count_nodes(NodeType type, const Region *region = nullptr) const // NOLINT(*-no-recursion)
	{
		if (!region)
			region = module->get_root_region();

		std::size_t count = 0;
		for (const Node *node: region->get_nodes())
		{
			if (node->ir_type == type)
				count++;
		}

		for (const Region *child: region->get_children())
			count += count_nodes(type, child);

		return count;
	}

	static bool has_property(Node *node, NodeProps prop)
	{
		return node && (node->props & prop) != NodeProps::NONE;
	}

	std::unique_ptr<Context> ctx;
	std::unique_ptr<Builder> builder;
	Module *module = nullptr;
};

TEST_F(BehaviorTreeTest, SimplePatternMatching)
{
	/* create x + 0 */
	Node *x = create_literal(42);
	Node *zero = create_literal(0);
	Node *add = create_binary_op(NodeType::ADD, x, zero);

	TestBTContext test_ctx;
	test_ctx.set_node(add);

	/* pattern should match */
	auto pattern = BT::pattern([](TestBTContext &ctx)
	{
		return ctx.is_add() && ctx.right_is_zero();
	});

	BTStatus result = pattern.execute(test_ctx);
	EXPECT_EQ(result, BTStatus::SUCCESS);
}

TEST_F(BehaviorTreeTest, SimpleTransform)
{
	TestBTContext test_ctx;

	auto transform = BT::transform([](TestBTContext &ctx)
	{
		ctx.set_last_action("optimized");
		ctx.mark_changed();
		return BTStatus::SUCCESS;
	});

	BTStatus result = transform.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::SUCCESS);
	EXPECT_TRUE(test_ctx.has_changed());
	EXPECT_EQ(test_ctx.get_last_action(), "optimized");
}

TEST_F(BehaviorTreeTest, RuleCombinesPatternAndTransform)
{
	/* create x * 1 */
	Node *x = create_literal(42);
	Node *one = create_literal(1);
	Node *mul = create_binary_op(NodeType::MUL, x, one);

	TestBTContext test_ctx;
	test_ctx.set_node(mul);

	auto rule = BT::rule(
		BT::pattern([](TestBTContext &ctx)
		{
			return ctx.is_mul() && ctx.right_is_one();
		}),
		BT::transform([](TestBTContext &ctx)
		{
			ctx.set_last_action("identity_elimination");
			ctx.mark_changed();
			return BTStatus::SUCCESS;
		})
	);

	BTStatus result = rule.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::SUCCESS);
	EXPECT_TRUE(test_ctx.has_changed());
	EXPECT_EQ(test_ctx.get_last_action(), "identity_elimination");
}

TEST_F(BehaviorTreeTest, RuleFailsWhenPatternDoesNotMatch)
{
	/* create x + 1 (not x + 0) */
	Node *x = create_literal(42);
	Node *one = create_literal(1);
	Node *add = create_binary_op(NodeType::ADD, x, one);

	TestBTContext test_ctx;
	test_ctx.set_node(add);

	auto rule = BT::rule(
		BT::pattern([](TestBTContext &ctx)
		{
			return ctx.is_add() && ctx.right_is_zero();
		}),
		BT::transform([](TestBTContext &ctx)
		{
			ctx.set_last_action("should_not_run");
			ctx.mark_changed();
			return BTStatus::SUCCESS;
		})
	);

	BTStatus result = rule.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::FAILURE);
	EXPECT_FALSE(test_ctx.has_changed());
	EXPECT_NE(test_ctx.get_last_action(), "should_not_run");
}

TEST_F(BehaviorTreeTest, SelectorTriesAlternatives)
{
	/* create x - 0 */
	Node *x = create_literal(42);
	Node *zero = create_literal(0);
	Node *sub = create_binary_op(NodeType::SUB, x, zero);

	TestBTContext test_ctx;
	test_ctx.set_node(sub);

	auto selector = BT::selector(
		/* first rule won't match; looking for ADD */
		BT::rule(
			BT::pattern([](TestBTContext &ctx)
			{
				return ctx.is_add() && ctx.right_is_zero();
			}),
			BT::transform([](TestBTContext &ctx)
			{
				ctx.set_last_action("add_identity");
				return BTStatus::SUCCESS;
			})
		),

		/* second rule will match; looking for SUB */
		BT::rule(
			BT::pattern([](TestBTContext &ctx)
			{
				return ctx.is_sub() && ctx.right_is_zero();
			}),
			BT::transform([](TestBTContext &ctx)
			{
				ctx.set_last_action("sub_identity");
				ctx.mark_changed();
				return BTStatus::SUCCESS;
			})
		)
	);

	BTStatus result = selector.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::SUCCESS);
	EXPECT_TRUE(test_ctx.has_changed());
	EXPECT_EQ(test_ctx.get_last_action(), "sub_identity");
}

TEST_F(BehaviorTreeTest, SequenceRequiresAllToSucceed)
{
	TestBTContext test_ctx;

	bool first_ran = false;
	bool second_ran = false;

	auto sequence = BT::sequence(
		BT::transform([&first_ran](TestBTContext &ctx)
		{
			first_ran = true;
			return BTStatus::SUCCESS;
		}),
		BT::transform([&second_ran](TestBTContext &ctx)
		{
			second_ran = true;
			return BTStatus::SUCCESS;
		})
	);

	BTStatus result = sequence.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::SUCCESS);
	EXPECT_TRUE(first_ran);
	EXPECT_TRUE(second_ran);
}

TEST_F(BehaviorTreeTest, SequenceFailsIfAnyChildFails)
{
	TestBTContext test_ctx;

	bool first_ran = false;
	bool second_ran = false;

	auto sequence = BT::sequence(
		BT::transform([&first_ran](TestBTContext &ctx)
		{
			first_ran = true;
			return BTStatus::SUCCESS;
		}),
		BT::transform([&second_ran](TestBTContext &ctx)
		{
			second_ran = true;
			return BTStatus::FAILURE; /* this fails */
		})
	);

	BTStatus result = sequence.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::FAILURE);
	EXPECT_TRUE(first_ran);
	EXPECT_TRUE(second_ran);
}

TEST_F(BehaviorTreeTest, ParallelExecutesAllChildren)
{
	TestBTContext test_ctx;

	bool first_ran = false;
	bool second_ran = false;
	bool third_ran = false;

	auto parallel = BT::parallel(
		BT::transform([&first_ran](TestBTContext &ctx)
		{
			first_ran = true;
			return BTStatus::SUCCESS;
		}),
		BT::transform([&second_ran](TestBTContext &ctx)
		{
			second_ran = true;
			return BTStatus::FAILURE;
		}),
		BT::transform([&third_ran](TestBTContext &ctx)
		{
			third_ran = true;
			return BTStatus::SUCCESS;
		})
	);

	BTStatus result = parallel.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::SUCCESS); /* at least one succeeded */
	EXPECT_TRUE(first_ran);
	EXPECT_TRUE(second_ran);
	EXPECT_TRUE(third_ran);
}

TEST_F(BehaviorTreeTest, InverterFlipsResult)
{
	TestBTContext test_ctx;

	auto failing_pattern = BT::pattern([](TestBTContext &ctx)
	{
		return false; /* always fails */
	});

	auto inverted = BT::invert(failing_pattern);

	BTStatus result = inverted.execute(test_ctx);
	EXPECT_EQ(result, BTStatus::SUCCESS); /* failure becomes success */
}

TEST_F(BehaviorTreeTest, FixpointRepeatsUntilNoChanges)
{
	TestBTContext test_ctx;
	int iteration_count = 0;

	auto fixpoint = BT::fixpoint(
		BT::transform([&iteration_count](TestBTContext &ctx)
		{
			iteration_count++;
			if (iteration_count < 3)
			{
				ctx.mark_changed(); /* trigger another iteration */
				return BTStatus::SUCCESS;
			}
			/* on third iteration, don't mark changed */
			return BTStatus::SUCCESS;
		})
	);

	BTStatus result = fixpoint.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::SUCCESS);
	EXPECT_EQ(iteration_count, 3); /* should run exactly 3 times */
}

TEST_F(BehaviorTreeTest, ComplexOptimizationTree)
{
	/* create x * 0 */
	Node *x = create_literal(42);
	Node *zero = create_literal(0);
	Node *mul = create_binary_op(NodeType::MUL, x, zero);

	TestBTContext test_ctx;
	test_ctx.set_node(mul);

	/* optimization tree with multiple rules */
	auto optimization_tree = BT::selector(
		/* zero multiplication: x * 0 -> 0 */
		BT::rule(
			BT::pattern([](TestBTContext &ctx)
			{
				return ctx.is_mul() && ctx.right_is_zero();
			}),
			BT::transform([](TestBTContext &ctx)
			{
				ctx.set_last_action("zero_multiplication");
				ctx.mark_changed();
				return BTStatus::SUCCESS;
			})
		),

		/* identity multiplication: x * 1 -> x */
		BT::rule(
			BT::pattern([](TestBTContext &ctx)
			{
				return ctx.is_mul() && ctx.right_is_one();
			}),
			BT::transform([](TestBTContext &ctx)
			{
				ctx.set_last_action("identity_multiplication");
				ctx.mark_changed();
				return BTStatus::SUCCESS;
			})
		),

		/* addition identity: x + 0 -> x */
		BT::rule(
			BT::pattern([](TestBTContext &ctx)
			{
				return ctx.is_add() && ctx.right_is_zero();
			}),
			BT::transform([](TestBTContext &ctx)
			{
				ctx.set_last_action("addition_identity");
				ctx.mark_changed();
				return BTStatus::SUCCESS;
			})
		)
	);

	BTStatus result = optimization_tree.execute(test_ctx);

	EXPECT_EQ(result, BTStatus::SUCCESS);
	EXPECT_TRUE(test_ctx.has_changed());
	EXPECT_EQ(test_ctx.get_last_action(), "zero_multiplication");
}
