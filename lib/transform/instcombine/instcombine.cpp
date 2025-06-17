/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <unordered_set>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/transform/instcombine/instcombine.hpp>

namespace blm
{
	std::string_view InstcombinePass::name() const
	{
		return "instcombine";
	}

	std::string_view InstcombinePass::description() const
	{
		return "simplifies expressions using algebraic identities and strength reduction";
	}

	bool InstcombinePass::run(Module &m, PassContext &ctx)
	{
		const auto simplified = static_cast<std::int64_t>(process_region(m.get_root_region()));
		ctx.update_stat("agbs.simplified_expressions", simplified);
		return simplified > 0;
	}

	std::size_t InstcombinePass::process_region(Region *region) // NOLINT(*-no-recursion)
	{
		if (!region)
			return 0;

		std::size_t simplified = 0;
		std::vector<Node *> nodes_copy = region->get_nodes();
		std::unordered_set<Node *> processed;
		for (Node *node : nodes_copy)
		{
			if (!node || node->users.empty() || processed.contains(node))
				continue;

			processed.insert(node);

			if (Node *simplified_node = simplify_node(node, region);
				simplified_node && simplified_node != node)
			{
				replace_all_uses(node, simplified_node);
				region->remove_node(node);
				simplified++;
			}
		}

		for (Region *child : region->get_children())
			simplified += process_region(child);

		return simplified;
	}

	Node *InstcombinePass::simplify_node(Node *node, Region *region)
	{
		if (!node || (node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE)
			return nullptr;

		Node *result = nullptr;

		/* try specialized patterns first */
		result = simplify_negation_patterns(node, region);
		if (result)
			return result;

		result = simplify_carry_borrow_patterns(node, region);
		if (result)
			return result;

		/* try general binary operation patterns */
		if (node->inputs.size() == 2)
		{
			Node *left = node->inputs[0];
			Node *right = node->inputs[1];
			if (!left || !right)
				return nullptr;

			if (left == right)
				result = simplify_identical_operands(node, left, region);
			if (!result)
				result = simplify_with_constants(node, left, right, region);
			if (!result)
				result = apply_strength_reduction(node, left, right, region);
			if (!result)
				result = apply_advanced_patterns(node, left, right);
			if (!result)
				result = simplify_small_constant_multiplication(node, left, right, region);
			if (!result)
				result = simplify_comparison_patterns(node, left, right, region);
		}
		/* try unary operation patterns */
		else if (node->inputs.size() == 1)
		{
			result = simplify_unary_operation(node, node->inputs[0], region);
		}

		return result;
	}

	Node *InstcombinePass::simplify_negation_patterns(Node *node, Region *region)
	{
		if (node->inputs.size() != 2)
			return nullptr;

		Node *left = node->inputs[0];
		Node *right = node->inputs[1];

		switch (node->ir_type)
		{
			case NodeType::ADD:
				/* x + (-y) -> x - y */
				if (is_negation(right))
					return create_binary_op(region, NodeType::SUB, left, get_negated_value(right), node);
			/* (-x) + y -> y - x */
				if (is_negation(left))
					return create_binary_op(region, NodeType::SUB, right, get_negated_value(left), node);
				break;

			case NodeType::SUB:
				/* x - (-y) -> x + y */
				if (is_negation(right))
					return create_binary_op(region, NodeType::ADD, left, get_negated_value(right), node);
			/* (-x) - y -> -(x + y) */
				if (is_negation(left))
				{
					Node *add = create_binary_op(region, NodeType::ADD, get_negated_value(left), right, node);
					return create_negation(region, add, node);
				}
				break;

			case NodeType::MUL:
				/* (-x) * y -> -(x * y) */
				if (is_negation(left))
				{
					Node *mul = create_binary_op(region, NodeType::MUL, get_negated_value(left), right, node);
					return create_negation(region, mul, node);
				}
			/* x * (-y) -> -(x * y) */
				if (is_negation(right))
				{
					Node *mul = create_binary_op(region, NodeType::MUL, left, get_negated_value(right), node);
					return create_negation(region, mul, node);
				}
				break;

			default:
				break;
		}

		return nullptr;
	}

	Node *InstcombinePass::simplify_carry_borrow_patterns(Node *node, Region *region)
	{
		if (node->inputs.size() != 2)
			return nullptr;

		Node *left = node->inputs[0];
		Node *right = node->inputs[1];

		switch (node->ir_type)
		{
			case NodeType::SUB:
				/* x - (x & y) -> x & ~y */
				if (right->ir_type == NodeType::BAND && right->inputs.size() == 2)
				{
					if (right->inputs[0] == left || right->inputs[1] == left)
					{
						Node *y = (right->inputs[0] == left) ? right->inputs[1] : right->inputs[0];
						Node *not_y = create_bitwise_not(region, y, node);
						return create_binary_op(region, NodeType::BAND, left, not_y, node);
					}
				}
			/* (x | y) - x -> ~x & y */
				if (left->ir_type == NodeType::BOR && left->inputs.size() == 2)
				{
					if (left->inputs[0] == right || left->inputs[1] == right)
					{
						Node *y = (left->inputs[0] == right) ? left->inputs[1] : left->inputs[0];
						Node *not_x = create_bitwise_not(region, right, node);
						return create_binary_op(region, NodeType::BAND, not_x, y, node);
					}
				}
				break;

			case NodeType::ADD:
				/* x + (x & y) -> x | y */
				if (right->ir_type == NodeType::BAND && right->inputs.size() == 2)
				{
					if (right->inputs[0] == left || right->inputs[1] == left)
					{
						Node *y = (right->inputs[0] == left) ? right->inputs[1] : right->inputs[0];
						return create_binary_op(region, NodeType::BOR, left, y, node);
					}
				}
			/* (x & y) + (x | y) -> x + y */
				if ((left->ir_type == NodeType::BAND && right->ir_type == NodeType::BOR) ||
				    (left->ir_type == NodeType::BOR && right->ir_type == NodeType::BAND))
				{
					Node *and_node = (left->ir_type == NodeType::BAND) ? left : right;
					Node *or_node = (left->ir_type == NodeType::BOR) ? left : right;

					if (and_node->inputs.size() == 2 && or_node->inputs.size() == 2)
					{
						if ((and_node->inputs[0] == or_node->inputs[0] && and_node->inputs[1] == or_node->inputs[1]) ||
						    (and_node->inputs[0] == or_node->inputs[1] && and_node->inputs[1] == or_node->inputs[0]))
						{
							return create_binary_op(region, NodeType::ADD, and_node->inputs[0], and_node->inputs[1],
							                        node);
						}
					}
				}
				break;

			default:
				break;
		}

		return nullptr;
	}

	Node *InstcombinePass::simplify_identical_operands(Node *node, Node *operand, Region *region)
	{
		switch (node->ir_type)
		{
			case NodeType::SUB:
				return create_zero_literal(region, node->type_kind, node);
			case NodeType::DIV:
				if (!is_zero_constant(operand))
					return create_one_literal(region, node->type_kind, node);
				break;
			case NodeType::BXOR:
				return create_zero_literal(region, node->type_kind, node);
			case NodeType::BAND:
			case NodeType::BOR:
				return operand;
			case NodeType::EQ:
			case NodeType::GTE:
			case NodeType::LTE:
				return find_or_create_literal<bool, DataType::BOOL>(region, true, node);
			case NodeType::NEQ:
			case NodeType::GT:
			case NodeType::LT:
				return find_or_create_literal<bool, DataType::BOOL>(region, false, node);
			default:
				break;
		}
		return nullptr;
	}

	Node *InstcombinePass::simplify_with_constants(Node *node, Node *left, Node *right, Region *region)
	{
		switch (node->ir_type)
		{
			case NodeType::ADD:
				if (is_zero_constant(right))
					return left;
				if (is_zero_constant(left))
					return right;
				if (left == right) /* x + x = x * 2 */
					return create_binary_op(region, NodeType::MUL, left,
					                        create_constant_with_value(region, 2, node->type_kind, node), node);
				break;

			case NodeType::SUB:
				if (is_zero_constant(right))
					return left;
			/* 0 - (0 - x) = x; double negation */
				if (is_zero_constant(left) && right->ir_type == NodeType::SUB &&
				    right->inputs.size() == 2 && is_zero_constant(right->inputs[0]))
					return right->inputs[1];
				break;

			case NodeType::MUL:
				if (is_zero_constant(right) || is_zero_constant(left))
					return create_zero_literal(region, node->type_kind, node);
				if (is_one_constant(right))
					return left;
				if (is_one_constant(left))
					return right;
				if (is_minus_one_constant(right))
					return create_negation(region, left, node);
				if (is_minus_one_constant(left))
					return create_negation(region, right, node);
				break;

			case NodeType::DIV:
				if (is_one_constant(right))
					return left;
				if (is_zero_constant(left) && !is_zero_constant(right))
					return create_zero_literal(region, node->type_kind, node);
				if (is_minus_one_constant(right))
					return create_negation(region, left, node);
				break;

			case NodeType::BAND:
				if (is_zero_constant(right) || is_zero_constant(left))
					return create_zero_literal(region, node->type_kind, node);
				if (is_minus_one_constant(right))
					return left;
				if (is_minus_one_constant(left))
					return right;
				break;

			case NodeType::BOR:
				if (is_zero_constant(right))
					return left;
				if (is_zero_constant(left))
					return right;
				if (is_minus_one_constant(right) || is_minus_one_constant(left))
					return create_minus_one_literal(region, node->type_kind, node);
				break;

			case NodeType::BXOR:
				if (is_zero_constant(right))
					return left;
				if (is_zero_constant(left))
					return right;
				break;

			case NodeType::BSHL:
			case NodeType::BSHR:
				if (is_zero_constant(right))
					return left;
				if (is_zero_constant(left))
					return create_zero_literal(region, node->type_kind, node);
				break;

			case NodeType::LT:
				/* x < 0 = false for unsigned */
				if (is_unsigned_type(left->type_kind) && is_zero_constant(right))
					return find_or_create_literal<bool, DataType::BOOL>(region, false, node);
			/* 0 < x = x != 0 for unsigned */
				if (is_unsigned_type(right->type_kind) && is_zero_constant(left))
					return create_binary_op(region, NodeType::NEQ, right,
					                        create_zero_literal(region, right->type_kind, node), node);
				break;

			default:
				break;
		}

		return nullptr;
	}

	Node *InstcombinePass::apply_strength_reduction(Node *node, Node *left, Node *right, Region *region)
	{
		switch (node->ir_type)
		{
			case NodeType::MUL:
				/* x * 2^n -> x << n */
				if (is_power_of_two_constant(right))
					return create_shift(region, NodeType::BSHL, left, get_log2_of_constant(right), node);
			/* 2^n * x -> x << n */
				if (is_power_of_two_constant(left))
					return create_shift(region, NodeType::BSHL, right, get_log2_of_constant(left), node);
				break;

			case NodeType::DIV:
				/* x / 2^n -> x >> n for unsigned */
				if (is_power_of_two_constant(right) && is_unsigned_type(node->type_kind))
					return create_shift(region, NodeType::BSHR, left, get_log2_of_constant(right), node);
				break;

			default:
				break;
		}

		return nullptr;
	}

	Node *InstcombinePass::apply_advanced_patterns(Node *node, Node *left, Node *right)
	{
		switch (node->ir_type)
		{
			case NodeType::BAND:
				/* x & (x | y) -> x */
				if (right->ir_type == NodeType::BOR && right->inputs.size() == 2 &&
				    (right->inputs[0] == left || right->inputs[1] == left))
					return left;
			/* (x | y) & x -> x */
				if (left->ir_type == NodeType::BOR && left->inputs.size() == 2 &&
				    (left->inputs[0] == right || left->inputs[1] == right))
					return right;
				break;

			case NodeType::BOR:
				/* x | (x & y) -> x */
				if (right->ir_type == NodeType::BAND && right->inputs.size() == 2 &&
				    (right->inputs[0] == left || right->inputs[1] == left))
					return left;
			/* (x & y) | x -> x */
				if (left->ir_type == NodeType::BAND && left->inputs.size() == 2 &&
				    (left->inputs[0] == right || left->inputs[1] == right))
					return right;
				break;

			default:
				break;
		}

		return nullptr;
	}

	Node *InstcombinePass::simplify_small_constant_multiplication(
		Node *node, Node *left, Node *right, Region *region)
	{
		if (node->ir_type != NodeType::MUL)
			return nullptr;

		Node *var = nullptr;
		std::uint64_t constant = 0;

		if (is_constant(right))
		{
			var = left;
			constant = get_constant_value(right);
		}
		else if (is_constant(left))
		{
			var = right;
			constant = get_constant_value(left);
		}
		else
			return nullptr;

		/* decompose specific small constants */
		switch (constant)
		{
			case 3: /* x * 3 = (x << 1) + x */
				return create_shift_add(region, var, 1, var, node);
			case 5: /* x * 5 = (x << 2) + x */
				return create_shift_add(region, var, 2, var, node);
			case 6: /* x * 6 = (x << 1) + (x << 2) */
				return create_shift_add(region, var, 1, create_shift_left(region, var, 2, node), node);
			case 7: /* x * 7 = (x << 3) - x */
				return create_shift_sub(region, var, 3, var, node);
			case 9: /* x * 9 = (x << 3) + x */
				return create_shift_add(region, var, 3, var, node);
			case 10: /* x * 10 = (x << 1) + (x << 3) */
				return create_shift_add(region, var, 1, create_shift_left(region, var, 3, node), node);
			case 12: /* x * 12 = (x << 2) + (x << 3) */
				return create_shift_add(region, var, 2, create_shift_left(region, var, 3, node), node);
			case 15: /* x * 15 = (x << 4) - x */
				return create_shift_sub(region, var, 4, var, node);
			default:
				/* check for 2^n + 1 patterns */
				if (constant > 2 && is_power_of_two_plus_one(constant))
					return create_shift_add(region, var, get_log2_of_constant_minus_one(constant), var, node);
			/* check for 2^n - 1 patterns */
				if (constant > 2 && is_power_of_two_minus_one(constant))
					return create_shift_sub(region, var, get_log2_of_constant_plus_one(constant), var, node);
				break;
		}

		return nullptr;
	}

	Node *InstcombinePass::simplify_comparison_patterns(Node *node, Node *left, Node *right, Region *region)
	{
		switch (node->ir_type)
		{
			case NodeType::LT:
				/* x < C where C is power of 2 -> (x & ~(C-1)) == 0 for unsigned */
				if (is_unsigned_type(left->type_kind) && is_power_of_two_constant(right))
				{
					std::uint64_t c = get_constant_value(right);
					Node *mask = create_constant_with_value(region, ~(c - 1), left->type_kind, node);
					Node *and_op = create_binary_op(region, NodeType::BAND, left, mask, node);
					Node *zero = create_zero_literal(region, left->type_kind, node);
					return create_binary_op(region, NodeType::EQ, and_op, zero, node);
				}
				break;

			case NodeType::GTE:
				/* x >= C where C is power of 2 -> (x & ~(C-1)) != 0 for unsigned */
				if (is_unsigned_type(left->type_kind) && is_power_of_two_constant(right))
				{
					std::uint64_t c = get_constant_value(right);
					Node *mask = create_constant_with_value(region, ~(c - 1), left->type_kind, node);
					Node *and_op = create_binary_op(region, NodeType::BAND, left, mask, node);
					Node *zero = create_zero_literal(region, left->type_kind, node);
					return create_binary_op(region, NodeType::NEQ, and_op, zero, node);
				}
				break;

			case NodeType::EQ:
				/* x == false -> !x when x is bool */
					if (left->type_kind == DataType::BOOL && is_constant_value<bool, DataType::BOOL>(right, false))
						return create_logical_not(region, left, node);
			/* false == x -> !x when x is bool */
			if (right->type_kind == DataType::BOOL && is_constant_value<bool, DataType::BOOL>(left, false))
				return create_logical_not(region, right, node);
			/* x == 0 -> !x when x is bool */
			if (left->type_kind == DataType::BOOL && is_zero_constant(right))
				return create_logical_not(region, left, node);
			break;

			case NodeType::NEQ:
				/* x != false -> x when x is bool */
					if (left->type_kind == DataType::BOOL && is_constant_value<bool, DataType::BOOL>(right, false))
						return left;
			/* false != x -> x when x is bool */
			if (right->type_kind == DataType::BOOL && is_constant_value<bool, DataType::BOOL>(left, false))
				return right;
			/* x != 0 -> x when x is bool */
			if (left->type_kind == DataType::BOOL && is_zero_constant(right))
				return left;
			break;

			default:
				break;
		}

		return nullptr;
	}

	Node *InstcombinePass::simplify_unary_operation(Node *node, Node *input, Region *region)
	{
		if (node->ir_type == NodeType::BNOT)
		{
			/* ~(~x) = x */
			if (input->ir_type == NodeType::BNOT && !input->inputs.empty())
				return input->inputs[0];

			/* ~(x ^ const) = x ^ ~const */
			if (input->ir_type == NodeType::BXOR && input->inputs.size() == 2 && is_constant(input->inputs[1]))
			{
				Node *not_const = create_bitwise_not_constant(region, input->inputs[1], node);
				if (not_const)
					return create_binary_op(region, NodeType::BXOR, input->inputs[0], not_const, node);
			}
		}

		return nullptr;
	}

	Node *InstcombinePass::create_binary_op(Region *region, NodeType op_type,
	                                                    Node *left, Node *right, Node *insert_before)
	{
		Node *op_node = region->get_module().get_context().create<Node>();
		op_node->ir_type = op_type;
		op_node->type_kind = (op_type == NodeType::EQ || op_type == NodeType::NEQ ||
		                      op_type == NodeType::LT || op_type == NodeType::GT ||
		                      op_type == NodeType::LTE || op_type == NodeType::GTE)
			                     ? DataType::BOOL
			                     : left->type_kind;
		region->insert_node_before(insert_before, op_node);
		connect_nodes(op_node, { left, right });
		return op_node;
	}

	Node *InstcombinePass::create_shift(Region *region, NodeType shift_type,
	                                                Node *value, std::uint32_t amount, Node *insert_before)
	{
		Node *shift_const = create_constant_for_shift(region, amount, insert_before);
		return create_binary_op(region, shift_type, value, shift_const, insert_before);
	}

	Node *InstcombinePass::create_shift_add(Region *region, Node *var,
	                                                    std::uint32_t shift_amount, Node *addend, Node *insert_before)
	{
		Node *shift = create_shift_left(region, var, shift_amount, insert_before);
		return create_binary_op(region, NodeType::ADD, shift, addend, insert_before);
	}

	Node *InstcombinePass::create_shift_sub(Region *region, Node *var,
	                                                    std::uint32_t shift_amount, Node *subtrahend,
	                                                    Node *insert_before)
	{
		Node *shift = create_shift_left(region, var, shift_amount, insert_before);
		return create_binary_op(region, NodeType::SUB, shift, subtrahend, insert_before);
	}

	/* pattern recognition helpers */
	bool InstcombinePass::is_negation(Node *node) const
	{
		return node->ir_type == NodeType::SUB && node->inputs.size() == 2 &&
		       is_zero_constant(node->inputs[0]);
	}

	Node *InstcombinePass::get_negated_value(Node *negation) const
	{
		return is_negation(negation) ? negation->inputs[1] : nullptr;
	}

	bool InstcombinePass::is_constant(Node *node)
	{
		return node && node->ir_type == NodeType::LIT;
	}

	bool InstcombinePass::is_power_of_two_constant(Node *node)
	{
		if (!is_constant(node))
			return false;
		std::uint64_t value = get_constant_value(node);
		return value > 0 && (value & (value - 1)) == 0;
	}

	bool InstcombinePass::is_power_of_two_plus_one(std::uint64_t value)
	{
		return value > 2 && is_power_of_two_constant_value(value - 1);
	}

	bool InstcombinePass::is_power_of_two_minus_one(std::uint64_t value)
	{
		return value > 2 && is_power_of_two_constant_value(value + 1);
	}

	bool InstcombinePass::is_power_of_two_constant_value(std::uint64_t value)
	{
		return value > 0 && (value & (value - 1)) == 0;
	}

	bool InstcombinePass::is_unsigned_type(DataType type)
	{
		return type == DataType::UINT8 || type == DataType::UINT16 ||
		       type == DataType::UINT32 || type == DataType::UINT64;
	}

	std::uint32_t InstcombinePass::get_log2_of_constant(Node *node)
	{
		return get_log2_of_constant_value(get_constant_value(node));
	}

	std::uint32_t InstcombinePass::get_log2_of_constant_minus_one(std::uint64_t value)
	{
		return get_log2_of_constant_value(value - 1);
	}

	std::uint32_t InstcombinePass::get_log2_of_constant_plus_one(std::uint64_t value)
	{
		return get_log2_of_constant_value(value + 1);
	}

	std::uint32_t InstcombinePass::get_log2_of_constant_value(std::uint64_t value)
	{
		std::uint32_t log2 = 0;
		while (value > 1)
		{
			value >>= 1;
			log2++;
		}
		return log2;
	}

	std::uint64_t InstcombinePass::get_constant_value(Node *node)
	{
		if (!is_constant(node))
			return 0;

		switch (node->type_kind)
		{
			case DataType::INT8:
				return static_cast<std::uint64_t>(node->as<DataType::INT8>());
			case DataType::INT16:
				return static_cast<std::uint64_t>(node->as<DataType::INT16>());
			case DataType::INT32:
				return static_cast<std::uint64_t>(node->as<DataType::INT32>());
			case DataType::INT64:
				return static_cast<std::uint64_t>(node->as<DataType::INT64>());
			case DataType::UINT8:
				return node->as<DataType::UINT8>();
			case DataType::UINT16:
				return node->as<DataType::UINT16>();
			case DataType::UINT32:
				return node->as<DataType::UINT32>();
			case DataType::UINT64:
				return node->as<DataType::UINT64>();
			default:
				return 0;
		}
	}

	/* constant checking helpers */
	bool InstcombinePass::is_zero_constant(Node *node) const
	{
		return is_constant_value<std::int8_t, DataType::INT8>(node, 0) ||
		       is_constant_value<std::int16_t, DataType::INT16>(node, 0) ||
		       is_constant_value<std::int32_t, DataType::INT32>(node, 0) ||
		       is_constant_value<std::int64_t, DataType::INT64>(node, 0) ||
		       is_constant_value<std::uint8_t, DataType::UINT8>(node, 0) ||
		       is_constant_value<std::uint16_t, DataType::UINT16>(node, 0) ||
		       is_constant_value<std::uint32_t, DataType::UINT32>(node, 0) ||
		       is_constant_value<std::uint64_t, DataType::UINT64>(node, 0);
	}

	bool InstcombinePass::is_one_constant(Node *node) const
	{
		return is_constant_value<std::int8_t, DataType::INT8>(node, 1) ||
		       is_constant_value<std::int16_t, DataType::INT16>(node, 1) ||
		       is_constant_value<std::int32_t, DataType::INT32>(node, 1) ||
		       is_constant_value<std::int64_t, DataType::INT64>(node, 1) ||
		       is_constant_value<std::uint8_t, DataType::UINT8>(node, 1) ||
		       is_constant_value<std::uint16_t, DataType::UINT16>(node, 1) ||
		       is_constant_value<std::uint32_t, DataType::UINT32>(node, 1) ||
		       is_constant_value<std::uint64_t, DataType::UINT64>(node, 1);
	}

	bool InstcombinePass::is_minus_one_constant(Node *node) const
	{
		return is_constant_value<std::int8_t, DataType::INT8>(node, -1) ||
		       is_constant_value<std::int16_t, DataType::INT16>(node, -1) ||
		       is_constant_value<std::int32_t, DataType::INT32>(node, -1) ||
		       is_constant_value<std::int64_t, DataType::INT64>(node, -1);
	}

	/* node creation helpers */
	Node *InstcombinePass::create_negation(Region *region, Node *value, Node *insert_before)
	{
		Node *zero = create_zero_literal(region, value->type_kind, insert_before);
		return create_binary_op(region, NodeType::SUB, zero, value, insert_before);
	}

	Node *InstcombinePass::create_bitwise_not(Region *region, Node *value, Node *insert_before)
	{
		Node *not_node = region->get_module().get_context().create<Node>();
		not_node->ir_type = NodeType::BNOT;
		not_node->type_kind = value->type_kind;
		region->insert_node_before(insert_before, not_node);
		connect_nodes(not_node, { value });
		return not_node;
	}

	Node *InstcombinePass::create_logical_not(Region *region, Node *value, Node *insert_before)
	{
		Node *true_const = find_or_create_literal<bool, DataType::BOOL>(region, true, insert_before);
		return create_binary_op(region, NodeType::BXOR, value, true_const, insert_before);
	}

	Node *InstcombinePass::create_shift_left(Region *region, Node *value, std::uint32_t amount,
	                                                     Node *insert_before)
	{
		return create_shift(region, NodeType::BSHL, value, amount, insert_before);
	}

	Node *InstcombinePass::create_bitwise_not_constant(Region *region, Node *constant, Node *insert_before)
	{
		if (!is_constant(constant))
			return nullptr;

		std::uint64_t inverted_value;
		switch (constant->type_kind)
		{
			case DataType::INT8:
				inverted_value = ~static_cast<std::uint8_t>(constant->as<DataType::INT8>());
				break;
			case DataType::INT16:
				inverted_value = ~static_cast<std::uint16_t>(constant->as<DataType::INT16>());
				break;
			case DataType::INT32:
				inverted_value = ~static_cast<std::uint32_t>(constant->as<DataType::INT32>());
				break;
			case DataType::INT64:
				inverted_value = ~static_cast<std::uint64_t>(constant->as<DataType::INT64>());
				break;
			case DataType::UINT8:
				inverted_value = ~constant->as<DataType::UINT8>();
				break;
			case DataType::UINT16:
				inverted_value = ~constant->as<DataType::UINT16>();
				break;
			case DataType::UINT32:
				inverted_value = ~constant->as<DataType::UINT32>();
				break;
			case DataType::UINT64:
				inverted_value = ~constant->as<DataType::UINT64>();
				break;
			default:
				return nullptr;
		}

		return create_constant_with_value(region, inverted_value, constant->type_kind, insert_before);
	}

	Node *InstcombinePass::create_zero_literal(Region *region, DataType type, Node *insert_before)
	{
		switch (type)
		{
			case DataType::INT8:
				return find_or_create_literal<std::int8_t, DataType::INT8>(region, 0, insert_before);
			case DataType::INT16:
				return find_or_create_literal<std::int16_t, DataType::INT16>(region, 0, insert_before);
			case DataType::INT32:
				return find_or_create_literal<std::int32_t, DataType::INT32>(region, 0, insert_before);
			case DataType::INT64:
				return find_or_create_literal<std::int64_t, DataType::INT64>(region, 0, insert_before);
			case DataType::UINT8:
				return find_or_create_literal<std::uint8_t, DataType::UINT8>(region, 0, insert_before);
			case DataType::UINT16:
				return find_or_create_literal<std::uint16_t, DataType::UINT16>(region, 0, insert_before);
			case DataType::UINT32:
				return find_or_create_literal<std::uint32_t, DataType::UINT32>(region, 0, insert_before);
			case DataType::UINT64:
				return find_or_create_literal<std::uint64_t, DataType::UINT64>(region, 0, insert_before);
			default:
				return nullptr;
		}
	}

	Node *InstcombinePass::create_one_literal(Region *region, DataType type, Node *insert_before)
	{
		switch (type)
		{
			case DataType::INT8:
				return find_or_create_literal<std::int8_t, DataType::INT8>(region, 1, insert_before);
			case DataType::INT16:
				return find_or_create_literal<std::int16_t, DataType::INT16>(region, 1, insert_before);
			case DataType::INT32:
				return find_or_create_literal<std::int32_t, DataType::INT32>(region, 1, insert_before);
			case DataType::INT64:
				return find_or_create_literal<std::int64_t, DataType::INT64>(region, 1, insert_before);
			case DataType::UINT8:
				return find_or_create_literal<std::uint8_t, DataType::UINT8>(region, 1, insert_before);
			case DataType::UINT16:
				return find_or_create_literal<std::uint16_t, DataType::UINT16>(region, 1, insert_before);
			case DataType::UINT32:
				return find_or_create_literal<std::uint32_t, DataType::UINT32>(region, 1, insert_before);
			case DataType::UINT64:
				return find_or_create_literal<std::uint64_t, DataType::UINT64>(region, 1, insert_before);
			default:
				return nullptr;
		}
	}

	Node *InstcombinePass::create_minus_one_literal(Region *region, DataType type, Node *insert_before)
	{
		switch (type)
		{
			case DataType::INT8:
				return find_or_create_literal<std::int8_t, DataType::INT8>(region, -1, insert_before);
			case DataType::INT16:
				return find_or_create_literal<std::int16_t, DataType::INT16>(region, -1, insert_before);
			case DataType::INT32:
				return find_or_create_literal<std::int32_t, DataType::INT32>(region, -1, insert_before);
			case DataType::INT64:
				return find_or_create_literal<std::int64_t, DataType::INT64>(region, -1, insert_before);
			case DataType::UINT8:
				return find_or_create_literal<std::uint8_t, DataType::UINT8>(region, 0xFF, insert_before);
			case DataType::UINT16:
				return find_or_create_literal<std::uint16_t, DataType::UINT16>(region, 0xFFFF, insert_before);
			case DataType::UINT32:
				return find_or_create_literal<std::uint32_t, DataType::UINT32>(region, 0xFFFFFFFF, insert_before);
			case DataType::UINT64:
				return find_or_create_literal<std::uint64_t, DataType::UINT64>(
					region, 0xFFFFFFFFFFFFFFFF, insert_before);
			default:
				return nullptr;
		}
	}

	Node *InstcombinePass::create_constant_for_shift(Region *region, std::uint32_t shift_amount,
	                                                             Node *insert_before)
	{
		return find_or_create_literal<std::uint32_t, DataType::UINT32>(region, shift_amount, insert_before);
	}

	Node *InstcombinePass::create_constant_with_value(Region *region, std::uint64_t value, DataType type,
	                                                              Node *insert_before)
	{
		switch (type)
		{
			case DataType::INT8:
				return find_or_create_literal<std::int8_t, DataType::INT8>(
					region, static_cast<std::int8_t>(value), insert_before);
			case DataType::INT16:
				return find_or_create_literal<std::int16_t, DataType::INT16>(
					region, static_cast<std::int16_t>(value), insert_before);
			case DataType::INT32:
				return find_or_create_literal<std::int32_t, DataType::INT32>(
					region, static_cast<std::int32_t>(value), insert_before);
			case DataType::INT64:
				return find_or_create_literal<std::int64_t, DataType::INT64>(
					region, static_cast<std::int64_t>(value), insert_before);
			case DataType::UINT8:
				return find_or_create_literal<std::uint8_t, DataType::UINT8>(
					region, static_cast<std::uint8_t>(value), insert_before);
			case DataType::UINT16:
				return find_or_create_literal<std::uint16_t, DataType::UINT16>(
					region, static_cast<std::uint16_t>(value), insert_before);
			case DataType::UINT32:
				return find_or_create_literal<std::uint32_t, DataType::UINT32>(
					region, static_cast<std::uint32_t>(value), insert_before);
			case DataType::UINT64:
				return find_or_create_literal<std::uint64_t, DataType::UINT64>(region, value, insert_before);
			default:
				return nullptr;
		}
	}

	void InstcombinePass::replace_all_uses(Node *node_to_replace, Node *replacement_node)
	{
		if (!node_to_replace || !replacement_node || node_to_replace == replacement_node)
			return;

		std::vector<Node *> users_copy = node_to_replace->users;
		for (Node *user: users_copy)
		{
			for (size_t i = 0; i < user->inputs.size(); i++)
			{
				if (user->inputs[i] == node_to_replace)
				{
					user->inputs[i] = replacement_node;
					if (std::ranges::find(replacement_node->users, user) == replacement_node->users.end())
						replacement_node->users.push_back(user);
				}
			}
		}
		node_to_replace->users.clear();
	}

	void InstcombinePass::connect_nodes(Node *user, const std::vector<Node *> &inputs)
	{
		for (Node *input: inputs)
		{
			if (input)
			{
				user->inputs.push_back(input);
				input->users.push_back(user);
			}
		}
	}
}
