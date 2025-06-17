/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <cmath>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/transform/constfold.hpp>

namespace blm
{
	template<typename T>
	Node *find_or_create_literal(Module &module, T value, Region *target_region, Node *insert_before = nullptr)
	{
		if (!target_region)
			target_region = module.get_root_region();

		for (Node *node: target_region->get_nodes())
		{
			if (node->ir_type != NodeType::LIT)
				continue;

			if constexpr (std::is_same_v<T, bool>)
			{
				if (node->type_kind == DataType::BOOL && node->as<DataType::BOOL>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::int8_t>)
			{
				if (node->type_kind == DataType::INT8 && node->as<DataType::INT8>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::int16_t>)
			{
				if (node->type_kind == DataType::INT16 && node->as<DataType::INT16>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::int32_t>)
			{
				if (node->type_kind == DataType::INT32 && node->as<DataType::INT32>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::int64_t>)
			{
				if (node->type_kind == DataType::INT64 && node->as<DataType::INT64>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::uint8_t>)
			{
				if (node->type_kind == DataType::UINT8 && node->as<DataType::UINT8>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::uint16_t>)
			{
				if (node->type_kind == DataType::UINT16 && node->as<DataType::UINT16>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::uint32_t>)
			{
				if (node->type_kind == DataType::UINT32 && node->as<DataType::UINT32>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, std::uint64_t>)
			{
				if (node->type_kind == DataType::UINT64 && node->as<DataType::UINT64>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				if (node->type_kind == DataType::FLOAT32 && node->as<DataType::FLOAT32>() == value)
					return node;
			}
			else if constexpr (std::is_same_v<T, double>)
			{
				if (node->type_kind == DataType::FLOAT64 && node->as<DataType::FLOAT64>() == value)
					return node;
			}
		}

		Node *lit = module.get_context().create<Node>();
		lit->ir_type = NodeType::LIT;

		if constexpr (std::is_same_v<T, bool>)
		{
			lit->type_kind = DataType::BOOL;
			lit->data.set<bool, DataType::BOOL>(value);
		}
		else if constexpr (std::is_same_v<T, std::int8_t>)
		{
			lit->type_kind = DataType::INT8;
			lit->data.set<std::int8_t, DataType::INT8>(value);
		}
		else if constexpr (std::is_same_v<T, std::int16_t>)
		{
			lit->type_kind = DataType::INT16;
			lit->data.set<std::int16_t, DataType::INT16>(value);
		}
		else if constexpr (std::is_same_v<T, std::int32_t>)
		{
			lit->type_kind = DataType::INT32;
			lit->data.set<std::int32_t, DataType::INT32>(value);
		}
		else if constexpr (std::is_same_v<T, std::int64_t>)
		{
			lit->type_kind = DataType::INT64;
			lit->data.set<std::int64_t, DataType::INT64>(value);
		}
		else if constexpr (std::is_same_v<T, std::uint8_t>)
		{
			lit->type_kind = DataType::UINT8;
			lit->data.set<std::uint8_t, DataType::UINT8>(value);
		}
		else if constexpr (std::is_same_v<T, std::uint16_t>)
		{
			lit->type_kind = DataType::UINT16;
			lit->data.set<std::uint16_t, DataType::UINT16>(value);
		}
		else if constexpr (std::is_same_v<T, std::uint32_t>)
		{
			lit->type_kind = DataType::UINT32;
			lit->data.set<std::uint32_t, DataType::UINT32>(value);
		}
		else if constexpr (std::is_same_v<T, std::uint64_t>)
		{
			lit->type_kind = DataType::UINT64;
			lit->data.set<std::uint64_t, DataType::UINT64>(value);
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			lit->type_kind = DataType::FLOAT32;
			lit->data.set<float, DataType::FLOAT32>(value);
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			lit->type_kind = DataType::FLOAT64;
			lit->data.set<double, DataType::FLOAT64>(value);
		}

		return lit;
	}

	template<typename T, DataType DT>
	Node *fold_bitwise_typed(const Node *node, const Node *lhs, const Node *rhs,
	                         Module &module, Region *target_region, Node *insert_before)
	{
		const T lval = lhs->as<DT>();
		const T rval = rhs->as<DT>();
		switch (node->ir_type)
		{
			case NodeType::BAND:
				return find_or_create_literal<T>(module, lval & rval, target_region, insert_before);
			case NodeType::BOR:
				return find_or_create_literal<T>(module, lval | rval, target_region, insert_before);
			case NodeType::BXOR:
				return find_or_create_literal<T>(module, lval ^ rval, target_region, insert_before);
			case NodeType::BSHL:
				return find_or_create_literal<T>(module, lval << rval, target_region, insert_before);
			case NodeType::BSHR:
				return find_or_create_literal<T>(module, lval >> rval, target_region, insert_before);
			default:
				return nullptr;
		}
	}

	template<typename T, DataType DT>
	Node *fold_arithmetic_typed(const Node *node, const Node *lhs, const Node *rhs,
	                            Module &module, Region *target_region, Node *insert_before)
	{
		const T lval = lhs->as<DT>();
		const T rval = rhs->as<DT>();

		switch (node->ir_type)
		{
			case NodeType::ADD:
				return find_or_create_literal<T>(module, lval + rval, target_region, insert_before);
			case NodeType::SUB:
				return find_or_create_literal<T>(module, lval - rval, target_region, insert_before);
			case NodeType::MUL:
				return find_or_create_literal<T>(module, lval * rval, target_region, insert_before);
			case NodeType::DIV:
				if (rval == 0)
					return nullptr;
				return find_or_create_literal<T>(module, lval / rval, target_region, insert_before);
			case NodeType::MOD:
				if (rval == 0)
					return nullptr;
				if constexpr (std::is_floating_point_v<T>)
					return find_or_create_literal<T>(module, std::fmod(lval, rval), target_region, insert_before);
				else
					return find_or_create_literal<T>(module, lval % rval, target_region, insert_before);
			default:
				return nullptr;
		}
	}

	template<typename T, DataType DT>
	Node *fold_comparison_typed(const Node *node, const Node *lhs, const Node *rhs,
	                            Module &module, Region *target_region, Node *insert_before)
	{
		const T lval = lhs->as<DT>();
		const T rval = rhs->as<DT>();

		switch (node->ir_type)
		{
			case NodeType::GT:
				return find_or_create_literal<bool>(module, lval > rval, target_region, insert_before);
			case NodeType::GTE:
				return find_or_create_literal<bool>(module, lval >= rval, target_region, insert_before);
			case NodeType::LT:
				return find_or_create_literal<bool>(module, lval < rval, target_region, insert_before);
			case NodeType::LTE:
				return find_or_create_literal<bool>(module, lval <= rval, target_region, insert_before);
			case NodeType::EQ:
				return find_or_create_literal<bool>(module, lval == rval, target_region, insert_before);
			case NodeType::NEQ:
				return find_or_create_literal<bool>(module, lval != rval, target_region, insert_before);
			default:
				return nullptr;
		}
	}

	std::string_view ConstantFoldingPass::name() const
	{
		return "constant-folding";
	}

	std::string_view ConstantFoldingPass::description() const
	{
		return "evaluates and replaces constant expressions with their computed values";
	}

	bool ConstantFoldingPass::run(Module &module, PassContext &context)
	{
		current_module = &module;
		std::size_t folded = process_region(module.get_root_region());
		for (const Node *fn: module.get_functions())
		{
			if (fn->ir_type != NodeType::FUNCTION)
				continue;

			for (Region *region: module.get_root_region()->get_children())
			{
				if (region->get_name() == module.get_context().get_string(fn->str_id))
				{
					folded += process_region(region);
					break;
				}
			}
		}

		context.update_stat("constant_folding.folded_nodes", folded);
		return folded > 0;
	}

	std::size_t ConstantFoldingPass::process_region(Region *region)
	{
		if (!region)
			return 0;

		std::size_t total_folded = 0;
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (std::vector<Node *> nodes = region->get_nodes();
			     Node *node: nodes)
			{
				if (!is_constant(node) ||
				    node->ir_type == NodeType::LIT ||
				    (node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE)
				{
					continue;
				}

				Node *folded_node = nullptr;
				if (has_global_inputs(node) && (node->props & NodeProps::EXPORT) != NodeProps::NONE)
				{
					folded_node = create_copy_propagation_node(node);
				}
				else
				{
					folded_node = create_folded_node(node);
				}

				if (!folded_node)
					continue;

				if (node->parent_region)
				{
					if (folded_node->parent_region == nullptr)
					{
						node->parent_region->insert_node_before(node, folded_node);
					}

					for (Node *user: node->users)
					{
						for (std::size_t i = 0; i < user->inputs.size(); i++)
						{
							if (user->inputs[i] == node)
							{
								user->inputs[i] = folded_node;
								if (std::ranges::find(folded_node->users, user) == folded_node->users.end())
									folded_node->users.push_back(user);
							}
						}
					}

					for (Node *input: node->inputs)
					{
						auto &input_users = input->users;
						std::erase(input_users, node);
					}

					node->parent_region->remove_node(node);
					total_folded++;
					changed = true;
				}
			}
		}

		for (Region *child: region->get_children())
			total_folded += process_region(child);

		return total_folded;
	}

	bool ConstantFoldingPass::has_global_inputs(const Node *node) const
	{
		for (const Node *input: node->inputs)
		{
			if (input->parent_region == current_module->get_root_region())
				return true;
		}
		return false;
	}

	Node *ConstantFoldingPass::create_copy_propagation_node(const Node *node) const
	{
		if (node->inputs.size() == 1)
			return node->inputs[0];
		return nullptr;
	}

	bool ConstantFoldingPass::is_constant(const Node *node) const
	{
		switch (node->ir_type)
		{
			case NodeType::BRANCH:
				return !node->inputs.empty() && node->inputs[0]->ir_type == NodeType::LIT;

			case NodeType::BNOT:
				return node->inputs.size() == 1 && node->inputs[0]->ir_type == NodeType::LIT;

			case NodeType::RET:
				return node->inputs.empty() ||
				       (node->inputs.size() == 1 && node->inputs[0]->ir_type == NodeType::LIT);
			default:
				break;
		}

		for (const Node *input: node->inputs)
		{
			if (input->ir_type != NodeType::LIT)
				return false;
		}

		switch (node->ir_type)
		{
			case NodeType::ADD:
			case NodeType::SUB:
			case NodeType::MUL:
			case NodeType::DIV:
			case NodeType::MOD:
			case NodeType::GT:
			case NodeType::GTE:
			case NodeType::LT:
			case NodeType::LTE:
			case NodeType::EQ:
			case NodeType::NEQ:
			case NodeType::BAND:
			case NodeType::BOR:
			case NodeType::BXOR:
			case NodeType::BSHL:
			case NodeType::BSHR:
				return true;
			default:
				return false;
		}
	}

	Node *ConstantFoldingPass::create_folded_node(const Node *node) const
	{
		if (!current_module)
			return nullptr;

		Region *target_region = node->parent_region;

		if (node->ir_type == NodeType::BRANCH)
		{
			if (node->inputs.size() < 3 || node->inputs[0]->ir_type != NodeType::LIT)
				return nullptr;

			const Node *condition = node->inputs[0];
			if (condition->type_kind != DataType::BOOL)
				return nullptr;

			const bool cond_value = condition->as<DataType::BOOL>();
			Node *target = cond_value ? node->inputs[1] : node->inputs[2];

			Node *jump = current_module->get_context().create<Node>();
			jump->ir_type = NodeType::JUMP;
			jump->type_kind = DataType::VOID;
			jump->inputs.push_back(target);
			if (std::ranges::find(target->users, jump) == target->users.end())
				target->users.push_back(jump);

			return jump;
		}

		if (node->ir_type == NodeType::RET)
			return nullptr;

		if (node->ir_type == NodeType::BNOT)
		{
			if (node->inputs.size() != 1 || node->inputs[0]->ir_type != NodeType::LIT)
				return nullptr;

			const Node *input = node->inputs[0];
			switch (input->type_kind)
			{
				case DataType::INT8:
					return find_or_create_literal<std::int8_t>(*current_module, ~input->as<DataType::INT8>(),
					                                           target_region, const_cast<Node *>(node));
				case DataType::INT16:
					return find_or_create_literal<std::int16_t>(*current_module, ~input->as<DataType::INT16>(),
					                                            target_region, const_cast<Node *>(node));
				case DataType::INT32:
					return find_or_create_literal<std::int32_t>(*current_module, ~input->as<DataType::INT32>(),
					                                            target_region, const_cast<Node *>(node));
				case DataType::INT64:
					return find_or_create_literal<std::int64_t>(*current_module, ~input->as<DataType::INT64>(),
					                                            target_region, const_cast<Node *>(node));
				case DataType::UINT8:
					return find_or_create_literal<std::uint8_t>(*current_module, ~input->as<DataType::UINT8>(),
					                                            target_region, const_cast<Node *>(node));
				case DataType::UINT16:
					return find_or_create_literal<std::uint16_t>(*current_module, ~input->as<DataType::UINT16>(),
					                                             target_region, const_cast<Node *>(node));
				case DataType::UINT32:
					return find_or_create_literal<std::uint32_t>(*current_module, ~input->as<DataType::UINT32>(),
					                                             target_region, const_cast<Node *>(node));
				case DataType::UINT64:
					return find_or_create_literal<std::uint64_t>(*current_module, ~input->as<DataType::UINT64>(),
					                                             target_region, const_cast<Node *>(node));
				default:
					return nullptr;
			}
		}

		if (node->inputs.size() != 2)
			return nullptr;

		const Node *lhs = node->inputs[0];
		const Node *rhs = node->inputs[1];
		if (lhs->ir_type != NodeType::LIT || rhs->ir_type != NodeType::LIT || lhs->type_kind != rhs->type_kind)
			return nullptr;

		switch (node->ir_type)
		{
			case NodeType::ADD:
			case NodeType::SUB:
			case NodeType::MUL:
			case NodeType::DIV:
				return fold_arithmetic(node, lhs, rhs);

			case NodeType::GT:
			case NodeType::GTE:
			case NodeType::LT:
			case NodeType::LTE:
			case NodeType::EQ:
			case NodeType::NEQ:
				return fold_comparison(node, lhs, rhs);

			case NodeType::BAND:
			case NodeType::BOR:
			case NodeType::BXOR:
			case NodeType::BSHL:
			case NodeType::BSHR:
				return fold_bitwise(node, lhs, rhs);

			default:
				return nullptr;
		}
	}

	Node *ConstantFoldingPass::fold_arithmetic(const Node *node, const Node *lhs, const Node *rhs) const
	{
		Region *target_region = node->parent_region;

		switch (lhs->type_kind)
		{
			case DataType::INT8:
				return fold_arithmetic_typed<std::int8_t, DataType::INT8>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT16:
				return fold_arithmetic_typed<std::int16_t, DataType::INT16>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT32:
				return fold_arithmetic_typed<std::int32_t, DataType::INT32>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT64:
				return fold_arithmetic_typed<std::int64_t, DataType::INT64>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT8:
				return fold_arithmetic_typed<std::uint8_t, DataType::UINT8>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT16:
				return fold_arithmetic_typed<std::uint16_t, DataType::UINT16>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT32:
				return fold_arithmetic_typed<std::uint32_t, DataType::UINT32>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT64:
				return fold_arithmetic_typed<std::uint64_t, DataType::UINT64>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::FLOAT32:
				return fold_arithmetic_typed<float, DataType::FLOAT32>(node, lhs, rhs, *current_module, target_region,
				                                                       const_cast<Node *>(node));
			case DataType::FLOAT64:
				return fold_arithmetic_typed<double, DataType::FLOAT64>(node, lhs, rhs, *current_module, target_region,
				                                                        const_cast<Node *>(node));
			default:
				return nullptr;
		}
	}

	Node *ConstantFoldingPass::fold_comparison(const Node *node, const Node *lhs, const Node *rhs) const
	{
		Region *target_region = node->parent_region;

		switch (lhs->type_kind)
		{
			case DataType::INT8:
				return fold_comparison_typed<std::int8_t, DataType::INT8>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT16:
				return fold_comparison_typed<std::int16_t, DataType::INT16>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT32:
				return fold_comparison_typed<std::int32_t, DataType::INT32>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT64:
				return fold_comparison_typed<std::int64_t, DataType::INT64>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT8:
				return fold_comparison_typed<std::uint8_t, DataType::UINT8>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT16:
				return fold_comparison_typed<std::uint16_t, DataType::UINT16>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT32:
				return fold_comparison_typed<std::uint32_t, DataType::UINT32>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT64:
				return fold_comparison_typed<std::uint64_t, DataType::UINT64>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::FLOAT32:
				return fold_comparison_typed<float, DataType::FLOAT32>(node, lhs, rhs, *current_module, target_region,
				                                                       const_cast<Node *>(node));
			case DataType::FLOAT64:
				return fold_comparison_typed<double, DataType::FLOAT64>(node, lhs, rhs, *current_module, target_region,
				                                                        const_cast<Node *>(node));
			case DataType::BOOL:
			{
				Region *target = node->parent_region;
				const bool lval = lhs->as<DataType::BOOL>();
				const bool rval = rhs->as<DataType::BOOL>();
				switch (node->ir_type)
				{
					case NodeType::EQ:
						return find_or_create_literal<bool>(*current_module, lval == rval, target,
						                                    const_cast<Node *>(node));
					case NodeType::NEQ:
						return find_or_create_literal<bool>(*current_module, lval != rval, target,
						                                    const_cast<Node *>(node));
					default:
						return nullptr;
				}
			}
			default:
				return nullptr;
		}
	}

	Node *ConstantFoldingPass::fold_bitwise(const Node *node, const Node *lhs, const Node *rhs) const
	{
		Region *target_region = node->parent_region;

		switch (lhs->type_kind)
		{
			case DataType::INT8:
				return fold_bitwise_typed<std::int8_t, DataType::INT8>(node, lhs, rhs, *current_module, target_region,
				                                                       const_cast<Node *>(node));
			case DataType::INT16:
				return fold_bitwise_typed<std::int16_t,
					DataType::INT16>(node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT32:
				return fold_bitwise_typed<std::int32_t,
					DataType::INT32>(node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::INT64:
				return fold_bitwise_typed<std::int64_t,
					DataType::INT64>(node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT8:
				return fold_bitwise_typed<std::uint8_t,
					DataType::UINT8>(node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT16:
				return fold_bitwise_typed<std::uint16_t, DataType::UINT16>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT32:
				return fold_bitwise_typed<std::uint32_t, DataType::UINT32>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			case DataType::UINT64:
				return fold_bitwise_typed<std::uint64_t, DataType::UINT64>(
					node, lhs, rhs, *current_module, target_region, const_cast<Node *>(node));
			default:
				return nullptr;
		}
	}
}
