/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <cmath>
#include <iostream>
#include <bloom/foundation/region.hpp>
#include <bloom/ipo/experimental/sccp.hpp>

namespace blm
{
	template<typename T>
	Node *find_or_create_literal(Module &module, T value)
	{
		/* search for existing literal with same value in the root region */
		Region *root = module.get_root_region();
		for (Node *node: root->get_nodes())
		{
			if (node->ir_type != NodeType::LIT)
				continue;

			/* check if this literal has the same type and value */
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

		/* no existing literal found; create a new one */
		Node *lit = module.get_context().create<Node>();
		lit->ir_type = NodeType::LIT;
		lit->parent_region = module.get_root_region();

		/* set type and value based on template parameter */
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

		/* add the new literal to the root region */
		module.get_root_region()->add_node(lit);
		return lit;
	}

	template<typename T, DataType DT>
	LatticeValue evaluate_bitwise_typed(Node *node, const LatticeValue &lhs, const LatticeValue &rhs)
	{
		T a = lhs.value.get<DT>();
		T b = rhs.value.get<DT>();
		switch (node->ir_type)
		{
			case NodeType::BAND:
				return LatticeValue::make_constant<T, DT>(a & b);
			case NodeType::BOR:
				return LatticeValue::make_constant<T, DT>(a | b);
			case NodeType::BXOR:
				return LatticeValue::make_constant<T, DT>(a ^ b);
			case NodeType::BSHL:
				return LatticeValue::make_constant<T, DT>(a << b);
			case NodeType::BSHR:
				return LatticeValue::make_constant<T, DT>(a >> b);
			default:
				return LatticeValue::make_bottom();
		}
	}

	template<typename T, DataType DT>
	LatticeValue evaluate_arithmetic_typed(Node *node, const LatticeValue &lhs, const LatticeValue &rhs)
	{
		T a = lhs.value.get<DT>();
		T b = rhs.value.get<DT>();
		switch (node->ir_type)
		{
			case NodeType::ADD:
				return LatticeValue::make_constant<T, DT>(a + b);
			case NodeType::SUB:
				return LatticeValue::make_constant<T, DT>(a - b);
			case NodeType::MUL:
				return LatticeValue::make_constant<T, DT>(a * b);
			case NodeType::DIV:
				if (b == 0)
					return LatticeValue::make_bottom();
				return LatticeValue::make_constant<T, DT>(a / b);
			case NodeType::MOD:
				if (b == 0)
					return LatticeValue::make_bottom();
				if constexpr (std::is_floating_point_v<T>)
					return LatticeValue::make_constant<T, DT>(std::fmod(a, b));
				else
					return LatticeValue::make_constant<T, DT>(a % b);
			default:
				return LatticeValue::make_bottom();
		}
	}

	template<typename T, DataType DT>
	LatticeValue evaluate_comparison_typed(Node *node, const LatticeValue &lhs, const LatticeValue &rhs)
	{
		T a = lhs.value.get<DT>();
		T b = rhs.value.get<DT>();
		switch (node->ir_type)
		{
			case NodeType::GT:
				return LatticeValue::make_constant<bool, DataType::BOOL>(a > b);
			case NodeType::GTE:
				return LatticeValue::make_constant<bool, DataType::BOOL>(a >= b);
			case NodeType::LT:
				return LatticeValue::make_constant<bool, DataType::BOOL>(a < b);
			case NodeType::LTE:
				return LatticeValue::make_constant<bool, DataType::BOOL>(a <= b);
			case NodeType::EQ:
				return LatticeValue::make_constant<bool, DataType::BOOL>(a == b);
			case NodeType::NEQ:
				return LatticeValue::make_constant<bool, DataType::BOOL>(a != b);
			default:
				return LatticeValue::make_bottom();
		}
	}

	bool IPOSCCPResult::invalidated_by(const std::type_info &transform_type) const
	{
		return transform_type != typeid(IPOSCCPPass);
	}

	bool IPOSCCPPass::run(std::vector<Module *> &modules, IPOPassContext &context)
	{
		const auto *cg_result = context.get_result<CallGraphResult>();
		if (!cg_result)
		{
			auto cg_pass = CallGraphAnalysisPass();
			cg_pass.run(modules, context);
			cg_result = context.get_result<CallGraphResult>();
			if (!cg_result)
				return false;
		}

		auto result = std::make_unique<IPOSCCPResult>();
		for (Module *module: modules)
			result->analyzed_modules.insert(module);

		initialize_lattice(modules, *result);
		run_sccp(modules, cg_result->get_call_graph(), *result);

		const std::size_t constant_count = result->get_constant_nodes().size();
		std::size_t replaced_count = 0;
		for (Module* module : modules)
			replaced_count += apply_constant_folding(*module, *result);

		context.store_result<IPOSCCPResult>(std::move(result));
		context.update_stat("ipo_sccp.constants_found", constant_count);
		context.update_stat("ipo_sccp.replaced_found", replaced_count);
		return constant_count > 0;
	}

	void IPOSCCPPass::initialize_lattice(std::vector<Module *> &modules, IPOSCCPResult &result)
	{
		for (Module *module: modules)
		{
			/* initialize all nodes to TOP */
			std::function<void(const Region *)> init_region = [&](const Region *region)
			{
				if (!region)
					return;

				for (Node *node: region->get_nodes())
				{
					if (node->ir_type == NodeType::LIT)
					{
						/* literals are immediately constant */
						LatticeValue lit_val;
						lit_val.state = LatticeValue::State::CONSTANT;
						lit_val.value = node->data;
						result.set_lattice_value(node, lit_val);
					}
					else
					{
						/* everything else starts as TOP */
						result.set_lattice_value(node, LatticeValue {});
					}
				}

				for (const Region *child: region->get_children())
					init_region(child);
			};

			init_region(module->get_root_region());
		}
	}

	void IPOSCCPPass::run_sccp(std::vector<Module *> &modules,
	                           const CallGraph &call_graph,
	                           IPOSCCPResult &result)
	{
		std::queue<Node *> worklist;

		/* add all literals to start propagation */
		for (Module *module: modules)
		{
			std::function<void(const Region *)> add_literals = [&](const Region *region)
			{
				if (!region)
					return;

				for (Node *node: region->get_nodes())
				{
					if (node->ir_type == NodeType::LIT)
					{
						for (Node *user : node->users)
							worklist.push(user);
					}
				}

				for (const Region *child: region->get_children())
					add_literals(child);
			};

			add_literals(module->get_root_region());
		}

		/* main fixpoint iteration */
		while (!worklist.empty())
		{
			Node *node = worklist.front();
			worklist.pop();
			process_node(node, result, worklist, call_graph);
		}
	}

	std::size_t IPOSCCPPass::apply_constant_folding(Module& module, const IPOSCCPResult& result)
	{
		std::size_t replaced_count = 0;

		std::function<void(Region*)> replace_in_region = [&](Region* region)
		{
			if (!region)
				return;
			std::vector<Node*> nodes_copy = region->get_nodes();
			for (Node* node : nodes_copy)
			{
				if (node->ir_type == NodeType::LIT ||
					node->ir_type == NodeType::ENTRY ||
					node->ir_type == NodeType::EXIT ||
					node->ir_type == NodeType::FUNCTION ||
					node->ir_type == NodeType::PARAM ||
					node->ir_type == NodeType::RET ||
					node->ir_type == NodeType::JUMP ||
					node->ir_type == NodeType::INVOKE)
				{
					continue;
				}

				if (node->ir_type == NodeType::CALL || node->ir_type == NodeType::INVOKE)
				{
					LatticeValue call_val = result.get_lattice_value(node);
					if (call_val.is_constant())
					{
						Node* literal = create_literal_from_lattice(call_val, module);
						if (literal && region->replace_node(node, literal, true))
						{
							replaced_count++;
						}
					}
					continue;
				}

				if (node->ir_type == NodeType::BRANCH)
				{
					LatticeValue cond_val = result.get_lattice_value(node->inputs[0]);
					if (cond_val.is_constant() && cond_val.value.type() == DataType::BOOL)
					{
						bool condition = cond_val.value.get<DataType::BOOL>();
						Node* target = condition ? node->inputs[1] : node->inputs[2];
						Node* jump = module.get_context().create<Node>();
						jump->ir_type = NodeType::JUMP;
						jump->type_kind = DataType::VOID;
						jump->inputs.push_back(target);
						target->users.push_back(jump);

						if (region->replace_node(node, jump, true))
						{
							replaced_count++;
						}
					}
					continue;
				}

				if (LatticeValue lattice_val = result.get_lattice_value(node);
					lattice_val.is_constant())
				{
					if (Node* literal = create_literal_from_lattice(lattice_val, module);
						literal && region->replace_node(node, literal, true))
					{
						replaced_count++;
					}
				}
			}

			for (Region* child : region->get_children())
				replace_in_region(child);
		};

		replace_in_region(module.get_root_region());
		return replaced_count;
	}


	bool IPOSCCPPass::replace_node_with_literal(Node* old_node, Node* new_literal)
	{
		if (!old_node || !new_literal)
			return false;

		std::vector<Node*> users_copy = old_node->users;
		for (Node* user : users_copy)
		{
			for (size_t i = 0; i < user->inputs.size(); ++i)
			{
				if (user->inputs[i] == old_node)
				{
					user->inputs[i] = new_literal;
					if (std::ranges::find(new_literal->users, user) == new_literal->users.end())
						new_literal->users.push_back(user);
				}
			}
		}

		old_node->users.clear();
		for (Node* input : old_node->inputs)
		{
			if (input)
			{
				auto& input_users = input->users;
				std::erase(input_users, old_node);
			}
		}
		old_node->inputs.clear();
		if (old_node->parent_region)
			old_node->parent_region->remove_node(old_node);
		return true;
	}

	void IPOSCCPPass::process_node(Node *node, IPOSCCPResult &result,
	                               std::queue<Node *> &worklist, const CallGraph &call_graph)
	{
		if (!node)
			return;

		switch (node->ir_type)
		{
			case NodeType::CALL:
			case NodeType::INVOKE:
				handle_function_call(node, call_graph, result, worklist);
				break;

			case NodeType::RET:
			case NodeType::PARAM:
				/* these don't produce values; handled by interprocedural propagation */
				break;

			default:
				const LatticeValue new_value = evaluate_node(node, result);
				update_lattice_value(node, new_value, result, worklist);
				break;
		}
	}

	LatticeValue IPOSCCPPass::evaluate_node(Node *node, const IPOSCCPResult &result)
	{
		if (!node)
			return LatticeValue {};

		/* check if all inputs are known and constant */
		std::vector<LatticeValue> input_values;
		for (Node *input: node->inputs)
		{
			LatticeValue input_val = result.get_lattice_value(input);
			if (input_val.is_top())
				return LatticeValue {}; /* TOP; default initialization is TOP */

			if (input_val.is_bottom())
				return LatticeValue::make_bottom(); /* BOTTOM propagates */

			input_values.push_back(input_val);
		}

		/* dispatch to appropriate evaluation function */
		switch (node->ir_type)
		{
			case NodeType::ADD:
			case NodeType::SUB:
			case NodeType::MUL:
			case NodeType::DIV:
			case NodeType::MOD:
				return evaluate_arithmetic(node, input_values);

			case NodeType::GT:
			case NodeType::GTE:
			case NodeType::LT:
			case NodeType::LTE:
			case NodeType::EQ:
			case NodeType::NEQ:
				return evaluate_comparison(node, input_values);

			case NodeType::BAND:
			case NodeType::BOR:
			case NodeType::BXOR:
			case NodeType::BSHL:
			case NodeType::BSHR:
				return evaluate_bitwise(node, input_values);

			case NodeType::BNOT:
				return evaluate_unary(node, input_values);

			case NodeType::BRANCH:
				return evaluate_branch(input_values);

			default:
				/* unknown/unsupported operation */
				return LatticeValue::make_bottom();
		}
	}

	LatticeValue IPOSCCPPass::evaluate_arithmetic(Node *node, const std::vector<LatticeValue> &inputs)
	{
		if (inputs.size() != 2)
			return LatticeValue::make_bottom();

		const LatticeValue &lhs = inputs[0];
		const LatticeValue &rhs = inputs[1];

		if (!lhs.is_constant() || !rhs.is_constant())
			return LatticeValue::make_bottom();

		/* dispatch based on type; covers all types like constfold */
		switch (lhs.value.type())
		{
			case DataType::INT8:
				return evaluate_arithmetic_typed<std::int8_t, DataType::INT8>(node, lhs, rhs);
			case DataType::INT16:
				return evaluate_arithmetic_typed<std::int16_t, DataType::INT16>(node, lhs, rhs);
			case DataType::INT32:
				return evaluate_arithmetic_typed<std::int32_t, DataType::INT32>(node, lhs, rhs);
			case DataType::INT64:
				return evaluate_arithmetic_typed<std::int64_t, DataType::INT64>(node, lhs, rhs);
			case DataType::UINT8:
				return evaluate_arithmetic_typed<std::uint8_t, DataType::UINT8>(node, lhs, rhs);
			case DataType::UINT16:
				return evaluate_arithmetic_typed<std::uint16_t, DataType::UINT16>(node, lhs, rhs);
			case DataType::UINT32:
				return evaluate_arithmetic_typed<std::uint32_t, DataType::UINT32>(node, lhs, rhs);
			case DataType::UINT64:
				return evaluate_arithmetic_typed<std::uint64_t, DataType::UINT64>(node, lhs, rhs);
			case DataType::FLOAT32:
				return evaluate_arithmetic_typed<float, DataType::FLOAT32>(node, lhs, rhs);
			case DataType::FLOAT64:
				return evaluate_arithmetic_typed<double, DataType::FLOAT64>(node, lhs, rhs);
			default:
				return LatticeValue::make_bottom();
		}
	}

	LatticeValue IPOSCCPPass::evaluate_comparison(Node *node, const std::vector<LatticeValue> &inputs)
	{
		if (inputs.size() != 2)
			return LatticeValue::make_bottom();

		const LatticeValue &lhs = inputs[0];
		const LatticeValue &rhs = inputs[1];

		if (!lhs.is_constant() || !rhs.is_constant())
			return LatticeValue::make_bottom();

		/* comparison result is always bool */
		switch (lhs.value.type())
		{
			case DataType::INT8:
				return evaluate_comparison_typed<std::int8_t, DataType::INT8>(node, lhs, rhs);
			case DataType::INT16:
				return evaluate_comparison_typed<std::int16_t, DataType::INT16>(node, lhs, rhs);
			case DataType::INT32:
				return evaluate_comparison_typed<std::int32_t, DataType::INT32>(node, lhs, rhs);
			case DataType::INT64:
				return evaluate_comparison_typed<std::int64_t, DataType::INT64>(node, lhs, rhs);
			case DataType::UINT8:
				return evaluate_comparison_typed<std::uint8_t, DataType::UINT8>(node, lhs, rhs);
			case DataType::UINT16:
				return evaluate_comparison_typed<std::uint16_t, DataType::UINT16>(node, lhs, rhs);
			case DataType::UINT32:
				return evaluate_comparison_typed<std::uint32_t, DataType::UINT32>(node, lhs, rhs);
			case DataType::UINT64:
				return evaluate_comparison_typed<std::uint64_t, DataType::UINT64>(node, lhs, rhs);
			case DataType::FLOAT32:
				return evaluate_comparison_typed<float, DataType::FLOAT32>(node, lhs, rhs);
			case DataType::FLOAT64:
				return evaluate_comparison_typed<double, DataType::FLOAT64>(node, lhs, rhs);
			case DataType::BOOL:
			{
				bool a = lhs.value.get<DataType::BOOL>();
				bool b = rhs.value.get<DataType::BOOL>();
				switch (node->ir_type)
				{
					case NodeType::EQ:
						return LatticeValue::make_constant<bool, DataType::BOOL>(a == b);
					case NodeType::NEQ:
						return LatticeValue::make_constant<bool, DataType::BOOL>(a != b);
					default:
						return LatticeValue::make_bottom();
				}
			}
			default:
				return LatticeValue::make_bottom();
		}
	}

	LatticeValue IPOSCCPPass::evaluate_bitwise(Node *node, const std::vector<LatticeValue> &inputs)
	{
		if (inputs.size() != 2)
			return LatticeValue::make_bottom();

		const LatticeValue &lhs = inputs[0];
		const LatticeValue &rhs = inputs[1];

		if (!lhs.is_constant() || !rhs.is_constant())
			return LatticeValue::make_bottom();

		switch (lhs.value.type())
		{
			case DataType::INT8:
				return evaluate_bitwise_typed<std::int8_t, DataType::INT8>(node, lhs, rhs);
			case DataType::INT16:
				return evaluate_bitwise_typed<std::int16_t, DataType::INT16>(node, lhs, rhs);
			case DataType::INT32:
				return evaluate_bitwise_typed<std::int32_t, DataType::INT32>(node, lhs, rhs);
			case DataType::INT64:
				return evaluate_bitwise_typed<std::int64_t, DataType::INT64>(node, lhs, rhs);
			case DataType::UINT8:
				return evaluate_bitwise_typed<std::uint8_t, DataType::UINT8>(node, lhs, rhs);
			case DataType::UINT16:
				return evaluate_bitwise_typed<std::uint16_t, DataType::UINT16>(node, lhs, rhs);
			case DataType::UINT32:
				return evaluate_bitwise_typed<std::uint32_t, DataType::UINT32>(node, lhs, rhs);
			case DataType::UINT64:
				return evaluate_bitwise_typed<std::uint64_t, DataType::UINT64>(node, lhs, rhs);
			default:
				return LatticeValue::make_bottom();
		}
	}

	LatticeValue IPOSCCPPass::evaluate_unary(Node *node, const std::vector<LatticeValue> &inputs)
	{
		if (inputs.size() != 1)
			return LatticeValue::make_bottom();

		const LatticeValue &operand = inputs[0];
		if (!operand.is_constant())
			return LatticeValue::make_bottom();

		switch (node->ir_type)
		{
			case NodeType::BNOT:
				switch (operand.value.type())
				{
					case DataType::INT8:
					{
						const std::int8_t val = operand.value.get<DataType::INT8>();
						return LatticeValue::make_constant<std::int8_t, DataType::INT8>(~val);
					}
					case DataType::INT16:
					{
						const std::int16_t val = operand.value.get<DataType::INT16>();
						return LatticeValue::make_constant<std::int16_t, DataType::INT16>(~val);
					}
					case DataType::INT32:
					{
						std::int32_t val = operand.value.get<DataType::INT32>();
						return LatticeValue::make_constant<std::int32_t, DataType::INT32>(~val);
					}
					case DataType::INT64:
					{
						std::int64_t val = operand.value.get<DataType::INT64>();
						return LatticeValue::make_constant<std::int64_t, DataType::INT64>(~val);
					}
					case DataType::UINT8:
					{
						const std::uint8_t val = operand.value.get<DataType::UINT8>();
						return LatticeValue::make_constant<std::uint8_t, DataType::UINT8>(~val);
					}
					case DataType::UINT16:
					{
						const std::uint16_t val = operand.value.get<DataType::UINT16>();
						return LatticeValue::make_constant<std::uint16_t, DataType::UINT16>(~val);
					}
					case DataType::UINT32:
					{
						const std::uint32_t val = operand.value.get<DataType::UINT32>();
						return LatticeValue::make_constant<std::uint32_t, DataType::UINT32>(~val);
					}
					case DataType::UINT64:
					{
						const std::uint64_t val = operand.value.get<DataType::UINT64>();
						return LatticeValue::make_constant<std::uint64_t, DataType::UINT64>(~val);
					}
					default:
						return LatticeValue::make_bottom();
				}
			default:
				return LatticeValue::make_bottom();
		}
	}

	LatticeValue IPOSCCPPass::evaluate_branch(const std::vector<LatticeValue> &inputs)
	{
		if (inputs.size() < 3)
			return LatticeValue::make_bottom();

		const LatticeValue &condition = inputs[0];
		if (!condition.is_constant() || condition.value.type() != DataType::BOOL)
			return LatticeValue::make_bottom();

		/* branch with constant condition indicates control flow simplification opportunity */
		/* we don't need to store the target here; just mark that this branch is constant */
		const bool cond_value = condition.value.get<DataType::BOOL>();
		return LatticeValue::make_constant<bool, DataType::BOOL>(cond_value);
	}

	bool IPOSCCPPass::update_lattice_value(Node *node, const LatticeValue &new_value,
	                                       IPOSCCPResult &result, std::queue<Node *> &worklist)
	{
		const LatticeValue current = result.get_lattice_value(node);


		/* check if value changed */
		if (const LatticeValue merged = meet(current, new_value);
			(merged.state != current.state) ||
			(merged.is_constant() && current.is_constant() &&
			 merged.value.type() != current.value.type()))
		{
			result.set_lattice_value(node, merged);
			for (Node *user: node->users)
				worklist.push(user);

			return true;
		}

		return false;
	}

	LatticeValue IPOSCCPPass::meet(const LatticeValue &a, const LatticeValue &b)
	{
		/* TOP meet X = X */
		if (a.is_top())
			return b;
		if (b.is_top())
			return a;

		/* BOTTOM meet X = BOTTOM */
		if (a.is_bottom() || b.is_bottom())
			return LatticeValue::make_bottom();

		/* both constant; check if they're the same */
		if (a.is_constant() && b.is_constant())
		{
			if (a.value.type() == b.value.type())
			{
				switch (a.value.type())
				{
					case DataType::BOOL:
						if (a.value.get<DataType::BOOL>() == b.value.get<DataType::BOOL>())
							return a;
						break;
					case DataType::INT8:
						if (a.value.get<DataType::INT8>() == b.value.get<DataType::INT8>())
							return a;
						break;
					case DataType::INT16:
						if (a.value.get<DataType::INT16>() == b.value.get<DataType::INT16>())
							return a;
						break;
					case DataType::INT32:
						if (a.value.get<DataType::INT32>() == b.value.get<DataType::INT32>())
							return a;
						break;
					case DataType::INT64:
						if (a.value.get<DataType::INT64>() == b.value.get<DataType::INT64>())
							return a;
						break;
					case DataType::UINT8:
						if (a.value.get<DataType::UINT8>() == b.value.get<DataType::UINT8>())
							return a;
						break;
					case DataType::UINT16:
						if (a.value.get<DataType::UINT16>() == b.value.get<DataType::UINT16>())
							return a;
						break;
					case DataType::UINT32:
						if (a.value.get<DataType::UINT32>() == b.value.get<DataType::UINT32>())
							return a;
						break;
					case DataType::UINT64:
						if (a.value.get<DataType::UINT64>() == b.value.get<DataType::UINT64>())
							return a;
						break;
					case DataType::FLOAT32:
						if (a.value.get<DataType::FLOAT32>() == b.value.get<DataType::FLOAT32>())
							return a;
						break;
					case DataType::FLOAT64:
						if (a.value.get<DataType::FLOAT64>() == b.value.get<DataType::FLOAT64>())
							return a;
						break;
					default:
						break;
				}
			}
			return LatticeValue::make_bottom(); /* different constants = BOTTOM */
		}
		return LatticeValue::make_bottom();
	}

	void IPOSCCPPass::handle_function_call(Node *call_node, const CallGraph &call_graph,
                                       IPOSCCPResult &result, std::queue<Node *> &worklist)
	{
	    if (!call_node || processed_calls.contains(call_node))
	        return;

	    processed_calls.insert(call_node);
	    if (call_node->inputs.empty())
	        return;

	    Node *callee = call_node->inputs[0];
	    if (callee && callee->ir_type == NodeType::FUNCTION) /* direct call */
	    {
	        const CallGraphNode *callee_cg_node = call_graph.get_node(callee);
	        if (!callee_cg_node)
	            return;

	        propagate_arguments(call_node, callee, result, worklist);
	    	std::vector modules(result.analyzed_modules.begin(), result.analyzed_modules.end());
	        for (const std::vector<Node *> return_nodes = find_return_nodes(callee, modules);
	             const Node *ret_node : return_nodes)
	        {
	            if (!ret_node->inputs.empty())
	            {
		            if (LatticeValue ret_val = result.get_lattice_value(ret_node->inputs[0]);
			            ret_val.is_constant())
	                {
	                    update_lattice_value(call_node, ret_val, result, worklist);
	                }
	            }
	        }
	    }
	    else /* indirect */
	    {
	        /* find caller function and check its callees */
	        const Region *current_region = call_node->parent_region;
	        Node *caller_func = nullptr;
	        while (current_region && !caller_func)
	        {
	            for (Node *node : current_region->get_nodes())
	            {
	                if (node->ir_type == NodeType::FUNCTION)
	                {
	                    caller_func = node;
	                    break;
	                }
	            }
	            current_region = current_region->get_parent();
	        }

	        if (caller_func)
	        {
		        if (const CallGraphNode *caller_cg_node = call_graph.get_node(caller_func))
	            {
	                for (const CallGraphNode *callee_node : caller_cg_node->get_callees())
	                {
		                if (const auto &call_sites = callee_node->get_call_sites();
			                std::ranges::find(call_sites, call_node) != call_sites.end())
	                    {
	                        /* propagate to this target */
	                        propagate_arguments(call_node, callee_node->get_function(), result, worklist);
	                        std::vector modules(result.analyzed_modules.begin(), result.analyzed_modules.end());
	                        for (std::vector<Node *> return_nodes = find_return_nodes(callee_node->get_function(), modules);
	                             const Node *ret_node : return_nodes)
	                        {
	                            if (!ret_node->inputs.empty())
	                            {
		                            if (LatticeValue ret_val = result.get_lattice_value(ret_node->inputs[0]);
			                            ret_val.is_constant())
	                                {
	                                    update_lattice_value(call_node, ret_val, result, worklist);
	                                }
	                            }
	                        }
	                    }
	                }
	            }
	        }
	    }
	}

	void IPOSCCPPass::propagate_arguments(Node *call_node, Node *callee_func,
	                                      IPOSCCPResult &result, std::queue<Node *> &worklist)
	{
		if (!call_node || !callee_func)
			return;

		std::vector<Module *> modules(result.analyzed_modules.begin(), result.analyzed_modules.end());
		std::vector<Node *> params = find_function_parameters(callee_func, modules);

		std::size_t arg_start = 1; /* skip function pointer */
		std::size_t arg_end = call_node->inputs.size();

		/* INVOKE nodes have exception targets at the end */
		if (call_node->ir_type == NodeType::INVOKE)
			arg_end -= 2;

		for (std::size_t i = 0; i < params.size() && (arg_start + i) < arg_end; ++i)
		{
			Node *arg = call_node->inputs[arg_start + i];
			Node *param = params[i];

			if (arg && param)
			{
				LatticeValue arg_val = result.get_lattice_value(arg);
				if (arg_val.is_constant())
				{
					update_lattice_value(param, arg_val, result, worklist);
				}
			}
		}
	}

	void IPOSCCPPass::propagate_return_value(Node *return_node, Node *caller_func,
	                                         const CallGraph &call_graph,
	                                         IPOSCCPResult &result, std::queue<Node *> &worklist)
	{
		if (!return_node || !caller_func || return_node->inputs.empty())
			return;

		LatticeValue ret_val = result.get_lattice_value(return_node->inputs[0]);
		if (!ret_val.is_constant())
			return;

		/* find all call sites that call this function */
		const CallGraphNode *cg_node = call_graph.get_node(caller_func);
		if (!cg_node)
			return;

		for (const CallGraphNode *caller_node: cg_node->get_callers())
		{
			for (Node *call_site: caller_node->get_call_sites())
			{
				if (call_site && !call_site->inputs.empty() &&
				    call_site->inputs[0] == caller_func)
				{
					update_lattice_value(call_site, ret_val, result, worklist);
				}
			}
		}
	}

	std::vector<Node *> IPOSCCPPass::find_function_parameters(Node *func_node,
	                                                          std::vector<Module *> &modules)
	{
		std::vector<Node *> params;
		Region *func_region = find_function_region(func_node, modules);
		if (!func_region)
			return params;

		/* collect parameter nodes */
		for (Node *node: func_region->get_nodes())
		{
			if (node && node->ir_type == NodeType::PARAM)
				params.push_back(node);
		}

		/* sort parameters by declaration order */
		std::ranges::sort(params, [func_region](const Node *a, const Node *b)
		{
			const auto &nodes = func_region->get_nodes();
			auto pos_a = std::ranges::find(nodes, a);
			auto pos_b = std::ranges::find(nodes, b);
			return pos_a < pos_b;
		});

		return params;
	}

	std::vector<Node *> IPOSCCPPass::find_return_nodes(Node *func_node,
	                                                   std::vector<Module *> &modules)
	{
		std::vector<Node *> returns;
		Region *func_region = find_function_region(func_node, modules);
		if (!func_region)
			return returns;

		std::function<void(const Region *)> collect_returns = [&](const Region *region)
		{
			if (!region)
				return;

			for (Node *node: region->get_nodes())
			{
				if (node && node->ir_type == NodeType::RET)
					returns.push_back(node);
			}

			for (const Region *child: region->get_children())
				collect_returns(child);
		};

		collect_returns(func_region);
		return returns;
	}

	Region *IPOSCCPPass::find_function_region(Node *func, std::vector<Module *> &modules)
	{
		if (!func || func->ir_type != NodeType::FUNCTION)
			return nullptr;

		for (Module *module: modules)
		{
			for (const Region *child: module->get_root_region()->get_children())
			{
				for (Node *node: child->get_nodes())
				{
					if (node == func)
						return const_cast<Region *>(child);
				}
			}
		}

		return nullptr;
	}

	Node *IPOSCCPPass::create_literal_from_lattice(const LatticeValue &lattice_val, Module &module)
	{
		if (!lattice_val.is_constant())
			return nullptr;

		switch (lattice_val.value.type())
		{
			case DataType::BOOL:
				return find_or_create_literal<bool>(module, lattice_val.value.get<DataType::BOOL>());
			case DataType::INT8:
				return find_or_create_literal<std::int8_t>(module, lattice_val.value.get<DataType::INT8>());
			case DataType::INT16:
				return find_or_create_literal<std::int16_t>(module, lattice_val.value.get<DataType::INT16>());
			case DataType::INT32:
				return find_or_create_literal<std::int32_t>(module, lattice_val.value.get<DataType::INT32>());
			case DataType::INT64:
				return find_or_create_literal<std::int64_t>(module, lattice_val.value.get<DataType::INT64>());
			case DataType::UINT8:
				return find_or_create_literal<std::uint8_t>(module, lattice_val.value.get<DataType::UINT8>());
			case DataType::UINT16:
				return find_or_create_literal<std::uint16_t>(module, lattice_val.value.get<DataType::UINT16>());
			case DataType::UINT32:
				return find_or_create_literal<std::uint32_t>(module, lattice_val.value.get<DataType::UINT32>());
			case DataType::UINT64:
				return find_or_create_literal<std::uint64_t>(module, lattice_val.value.get<DataType::UINT64>());
			case DataType::FLOAT32:
				return find_or_create_literal<float>(module, lattice_val.value.get<DataType::FLOAT32>());
			case DataType::FLOAT64:
				return find_or_create_literal<double>(module, lattice_val.value.get<DataType::FLOAT64>());
			default:
				return nullptr;
		}
	}
}
