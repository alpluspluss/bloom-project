/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <stdexcept>
#include <bloom/ir/builder.hpp>

namespace blm
{
	FunctionBuilder::FunctionBuilder(Builder &builder, Node *function_node, Region *function_region) : builder(builder),
		function(function_node), region(function_region) {}

	Node *FunctionBuilder::add_parameter(const std::string_view name, const DataType type)
	{
		const Region *old_region = builder.get_current_region();
		builder.set_insertion_point(region);

		Node *param = builder.create_node(NodeType::PARAM, type);
		param->str_id = builder.get_context().intern_string(name);
		parameters.push_back(param);

		builder.set_insertion_point(const_cast<Region *>(old_region));
		return param;
	}

	void FunctionBuilder::body(const std::function<void()> &body_func)
	{
		const Region *old_region = builder.get_current_region();
		builder.set_insertion_point(region);

		body_func();

		builder.set_insertion_point(const_cast<Region *>(old_region));
	}

	BlockBuilder::BlockBuilder(Builder &builder, Region *block_region) : builder(builder), region(block_region) {}

	void BlockBuilder::operator()(const std::function<void()> &block_func)
	{
		const Region *old_region = builder.get_current_region();
		builder.set_insertion_point(region);

		block_func();

		builder.set_insertion_point(const_cast<Region *>(old_region));
	}

	void BlockBuilder::ret(Node *value)
	{
		const Region *old_region = builder.get_current_region();
		builder.set_insertion_point(region);

		builder.ret(value);

		builder.set_insertion_point(const_cast<Region *>(old_region));
	}

	Builder::Builder(Context &ctx) : ctx(ctx) {}

	Module *Builder::create_module(const std::string_view name)
	{
		current_module = ctx.create_module(name);
		current_region = current_module->get_root_region();
		return current_module;
	}

	void Builder::set_current_module(Module *module)
	{
		current_module = module;
		current_region = module ? module->get_root_region() : nullptr;
	}

	FunctionBuilder Builder::create_function(const std::string_view name,
	                                         const std::vector<DataType> &param_types,
	                                         const DataType return_type,
	                                         const bool is_vararg)
	{
		if (!current_module)
			throw std::runtime_error("no current module");

		const DataType func_type = function_type(return_type, param_types, is_vararg);

		Node *func = create_node(NodeType::FUNCTION, func_type);
		func->str_id = ctx.intern_string(name);

		current_module->add_function(func);

		Region *func_region = create_region_with_entry(name, current_region);
		func_region->add_node(func);
		return { *this, func, func_region };
	}

	DataType Builder::function_type(const DataType return_type,
	                                const std::vector<DataType> &param_types,
	                                const bool is_vararg)
	{
		return ctx.create_function_type(return_type, param_types, is_vararg);
	}

	DataType Builder::pointer_type(const DataType pointee, const std::uint32_t addr_space)
	{
		return ctx.create_pointer_type(pointee, addr_space);
	}

	DataType Builder::array_type(const DataType element_type, const std::uint64_t count)
	{
		return ctx.create_array_type(element_type, count);
	}

	DataType Builder::struct_type(const std::vector<std::pair<std::string, DataType> > &fields,
	                              const std::uint32_t size, const std::uint32_t align)
	{
		return ctx.create_struct_type(fields, size, align);
	}

	template Node *Builder::literal<std::int8_t>(std::int8_t);

	template Node *Builder::literal<std::int16_t>(std::int16_t);

	template Node *Builder::literal<std::int32_t>(std::int32_t);

	template Node *Builder::literal<std::int64_t>(std::int64_t);

	template Node *Builder::literal<std::uint8_t>(std::uint8_t);

	template Node *Builder::literal<std::uint16_t>(std::uint16_t);

	template Node *Builder::literal<std::uint32_t>(std::uint32_t);

	template Node *Builder::literal<std::uint64_t>(std::uint64_t);

	Node *Builder::literal(std::string_view str)
	{
		return current_module->intern_string_literal(str);
	}

	Node *Builder::literal(const char *str)
	{
		return current_module->intern_string_literal(str);
	}

	Node *Builder::literal(const bool value)
	{
		Node *lit = create_node(NodeType::LIT, DataType::BOOL);
		lit->data.set<bool, DataType::BOOL>(value);
		return lit;
	}

	Node *Builder::literal(const float value)
	{
		Node *lit = create_node(NodeType::LIT, DataType::FLOAT32);
		lit->data.set<float, DataType::FLOAT32>(value);
		return lit;
	}

	Node *Builder::literal(const double value)
	{
		Node *lit = create_node(NodeType::LIT, DataType::FLOAT64);
		lit->data.set<double, DataType::FLOAT64>(value);
		return lit;
	}

	Node *Builder::add(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::ADD,
		                         result_type.value_or(infer_binary_result_type(NodeType::ADD, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::sub(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::SUB,
		                         result_type.value_or(infer_binary_result_type(NodeType::SUB, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::mul(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::MUL,
		                         result_type.value_or(infer_binary_result_type(NodeType::MUL, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::div(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::DIV,
		                         result_type.value_or(infer_binary_result_type(NodeType::DIV, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::mod(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::MOD,
		                         result_type.value_or(infer_binary_result_type(NodeType::MOD, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::band(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::BAND,
		                         result_type.value_or(infer_binary_result_type(NodeType::BAND, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::bor(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::BOR,
		                         result_type.value_or(infer_binary_result_type(NodeType::BOR, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::bxor(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::BXOR,
		                         result_type.value_or(infer_binary_result_type(NodeType::BXOR, lhs, rhs)));
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::bnot(Node *value)
	{
		Node *node = create_node(NodeType::BNOT, value->type_kind);
		connect_nodes(node, { value });
		return node;
	}

	Node *Builder::bshl(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::BSHL,
		                         result_type.value_or(lhs->type_kind)); // shift result is usually left operand type
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::bshr(Node *lhs, Node *rhs, const std::optional<DataType> result_type)
	{
		Node *node = create_node(NodeType::BSHR,
		                         result_type.value_or(lhs->type_kind)); // shift result is usually left operand type
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::eq(Node *lhs, Node *rhs)
	{
		Node *node = create_node(NodeType::EQ, DataType::BOOL);
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::neq(Node *lhs, Node *rhs)
	{
		Node *node = create_node(NodeType::NEQ, DataType::BOOL);
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::lt(Node *lhs, Node *rhs)
	{
		Node *node = create_node(NodeType::LT, DataType::BOOL);
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::lte(Node *lhs, Node *rhs)
	{
		Node *node = create_node(NodeType::LTE, DataType::BOOL);
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::gt(Node *lhs, Node *rhs)
	{
		Node *node = create_node(NodeType::GT, DataType::BOOL);
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::gte(Node *lhs, Node *rhs)
	{
		Node *node = create_node(NodeType::GTE, DataType::BOOL);
		connect_nodes(node, { lhs, rhs });
		return node;
	}

	Node *Builder::load(Node *address, const DataType result_type)
	{
		Node *node = create_node(NodeType::LOAD, result_type);
		connect_nodes(node, { address });
		return node;
	}

	Node *Builder::store(Node *value, Node *address)
	{
		Node *node = create_node(NodeType::STORE);
		connect_nodes(node, { value, address }); /* convention: value, address */
		return node;
	}

	Node *Builder::ptr_load(Node *ptr, const DataType result_type)
	{
		Node *node = create_node(NodeType::PTR_LOAD, result_type);
		connect_nodes(node, { ptr });
		return node;
	}

	Node *Builder::ptr_store(Node *value, Node *ptr)
	{
		Node *node = create_node(NodeType::PTR_STORE);
		connect_nodes(node, { value, ptr }); /* convention: value, pointer */
		return node;
	}

	Node *Builder::ptr_add(Node *base_ptr, Node *offset)
	{
		Node *node = create_node(NodeType::PTR_ADD, base_ptr->type_kind);
		connect_nodes(node, { base_ptr, offset }); /* convention: base, offset */
		return node;
	}

	Node *Builder::atomic_load(Node *address, DataType result_type, Node *ordering)
	{
		Node *node = create_node(NodeType::ATOMIC_LOAD, result_type);
		std::vector<Node *> inputs = { address };
		if (ordering)
			inputs.push_back(ordering);
		connect_nodes(node, inputs);
		return node;
	}

	Node *Builder::atomic_store(Node *value, Node *address, Node *ordering)
	{
		Node *node = create_node(NodeType::ATOMIC_STORE);
		std::vector<Node *> inputs = { value, address };
		if (ordering)
			inputs.push_back(ordering);
		connect_nodes(node, inputs);
		return node;
	}

	Node *Builder::atomic_cas(Node *address, Node *expected, Node *new_value, Node *ordering)
	{
		Node *node = create_node(NodeType::ATOMIC_CAS, expected->type_kind);
		std::vector<Node *> inputs = { address, expected, new_value };
		if (ordering)
			inputs.push_back(ordering);
		connect_nodes(node, inputs);
		return node;
	}

	Node *Builder::atomic_ordering(AtomicOrdering ordering)
	{
		return literal(static_cast<std::uint8_t>(ordering));
	}

	Node *Builder::addr_of(Node *variable)
	{
		Node *node = create_node(NodeType::ADDR_OF, pointer_type(variable->type_kind));
		connect_nodes(node, { variable });
		return node;
	}

	Node *Builder::stack_alloc(Node *size, const DataType type, const std::uint32_t alignment)
	{
		Node *node = create_node(NodeType::STACK_ALLOC, pointer_type(type));
		std::vector inputs = { size };
		if (alignment > 0)
			inputs.push_back(literal(alignment));
		connect_nodes(node, inputs);
		return node;
	}

	Node *Builder::heap_alloc(Node *function, Node *size, const DataType type, const std::uint32_t alignment)
	{
		Node *node = create_node(NodeType::HEAP_ALLOC, pointer_type(type));
		std::vector inputs = { function, size };
		if (alignment > 0)
			inputs.push_back(literal(alignment));

		connect_nodes(node, inputs);
		return node;
	}

	Node *Builder::free(Node *ptr)
	{
		Node *node = create_node(NodeType::FREE);
		connect_nodes(node, { ptr });
		return node;
	}

	Node *Builder::call(Node *function, const std::vector<Node *> &args)
	{
		const auto &func_type_data = ctx.get_type(function->type_kind);
		const auto &func_data = func_type_data.get<DataType::FUNCTION>();

		Node *node = create_node(NodeType::CALL, func_data.return_type);

		std::vector inputs = { function };
		inputs.insert(inputs.end(), args.begin(), args.end());
		connect_nodes(node, inputs);
		return node;
	}

	Node *Builder::ret(Node *value)
	{
		Node *node = create_node(NodeType::RET);
		if (value)
			connect_nodes(node, { value });
		return node;
	}

	Node *Builder::branch(Node *condition, Node *true_target, Node *false_target)
	{
		Node *node = create_node(NodeType::BRANCH);
		connect_nodes(node, { condition, true_target, false_target });
		return node;
	}

	Node *Builder::jump(Node *target)
	{
		Node *node = create_node(NodeType::JUMP);
		connect_nodes(node, { target });
		return node;
	}

	Node *Builder::invoke(Node *function, const std::vector<Node *> &args,
	                      Node *normal_target, Node *except_target)
	{
		const auto &func_type_data = ctx.get_type(function->type_kind);
		const auto &func_data = func_type_data.get<DataType::FUNCTION>();

		Node *node = create_node(NodeType::INVOKE, func_data.return_type);

		/* convention: function, args..., normal_target, except_target */
		std::vector<Node *> inputs = { function };
		inputs.insert(inputs.end(), args.begin(), args.end());
		inputs.push_back(normal_target);
		inputs.push_back(except_target);
		connect_nodes(node, inputs);

		return node;
	}

	InvokeBlocks Builder::create_invoke_blocks(const std::string_view normal_name,
	                                           const std::string_view except_name)
	{
		Region *normal_region = create_region_with_entry(normal_name);
		Region *except_region = create_region_with_entry(except_name);
		return { *this, normal_region, except_region };
	}

	std::pair<BlockBuilder, BlockBuilder> Builder::create_if(Node *condition,
	                                                         const std::string_view true_name,
	                                                         const std::string_view false_name)
	{
		Region *true_region = create_region_with_entry(true_name);
		Region *false_region = create_region_with_entry(false_name);

		Node *true_entry = true_region->get_nodes().empty() ? nullptr : true_region->get_nodes()[0];
		Node *false_entry = false_region->get_nodes().empty() ? nullptr : false_region->get_nodes()[0];

		if (true_entry && false_entry)
			branch(condition, true_entry, false_entry);

		return { BlockBuilder(*this, true_region), BlockBuilder(*this, false_region) };
	}

	LoopStructure Builder::create_while_loop(const std::string_view header_name,
	                                         const std::string_view body_name,
	                                         const std::string_view exit_name)
	{
		Region *header_region = create_region_with_entry(header_name);
		Region *body_region = create_region_with_entry(body_name, header_region);
		Region *exit_region = create_region_with_entry(exit_name);

		return { *this, header_region, body_region, exit_region };
	}

	BlockBuilder Builder::create_block(const std::string_view name, Region *parent)
	{
		Region *block_region = create_region_with_entry(name, parent);
		return { *this, block_region };
	}

	void Builder::set_insertion_point(Region *region)
	{
		current_region = region;
	}

	Node *Builder::create_entry()
	{
		return create_node(NodeType::ENTRY);
	}

	Node *Builder::create_exit()
	{
		return create_node(NodeType::EXIT);
	}

	Node *Builder::create_node(const NodeType type, const DataType result_type)
	{
		if (!current_region)
			throw std::runtime_error("no current region set");
		Node *node = ctx.create<Node>();
		node->ir_type = type;
		node->type_kind = result_type;
		current_region->add_node(node);
		return node;
	}

	void Builder::connect_nodes(Node *user, const std::vector<Node *> &inputs)
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

	DataType Builder::infer_binary_result_type(const NodeType op_type, Node *lhs, Node *rhs)
	{
		if (op_type == NodeType::EQ || op_type == NodeType::NEQ ||
		    op_type == NodeType::LT || op_type == NodeType::LTE ||
		    op_type == NodeType::GT || op_type == NodeType::GTE)
		{
			return DataType::BOOL;
		}

		if (lhs->type_kind == rhs->type_kind)
			return lhs->type_kind;

		const bool lhs_is_float = lhs->type_kind == DataType::FLOAT32 || lhs->type_kind == DataType::FLOAT64;
		const bool rhs_is_float = rhs->type_kind == DataType::FLOAT32 || rhs->type_kind == DataType::FLOAT64;

		if (lhs_is_float || rhs_is_float)
		{
			if (lhs->type_kind == DataType::FLOAT64 || rhs->type_kind == DataType::FLOAT64)
				return DataType::FLOAT64;
			return DataType::FLOAT32;
		}

		auto is_small_int = [](const DataType t)
		{
			return t == DataType::BOOL || t == DataType::INT8 ||
			       t == DataType::UINT8 || t == DataType::INT16 ||
			       t == DataType::UINT16;
		};

		if (is_small_int(lhs->type_kind) && is_small_int(rhs->type_kind))
			return DataType::INT32;

		auto is_unsigned = [](const DataType t)
		{
			return t == DataType::UINT8 || t == DataType::UINT16 ||
			       t == DataType::UINT32 || t == DataType::UINT64;
		};

		const bool lhs_unsigned = is_unsigned(lhs->type_kind);
		const bool rhs_unsigned = is_unsigned(rhs->type_kind);

		if (lhs_unsigned != rhs_unsigned)
		{
			auto get_type_rank = [](const DataType t) -> int
			{
				switch (t)
				{
					case DataType::INT8:
					case DataType::UINT8:
						return 1;
					case DataType::INT16:
					case DataType::UINT16:
						return 2;
					case DataType::INT32:
					case DataType::UINT32:
						return 3;
					case DataType::INT64:
					case DataType::UINT64:
						return 4;
					default:
						return 0;
				}
			};

			const int lhs_rank = get_type_rank(lhs->type_kind);
			const int rhs_rank = get_type_rank(rhs->type_kind);

			if (lhs_unsigned && lhs_rank >= rhs_rank)
				return lhs->type_kind;
			if (rhs_unsigned && rhs_rank >= lhs_rank)
				return rhs->type_kind;
		}

		auto get_rank = [](const DataType t) -> int
		{
			switch (t)
			{
				case DataType::BOOL:
					return 0;
				case DataType::INT8:
				case DataType::UINT8:
					return 1;
				case DataType::INT16:
				case DataType::UINT16:
					return 2;
				case DataType::INT32:
				case DataType::UINT32:
					return 3;
				case DataType::INT64:
				case DataType::UINT64:
					return 4;
				default:
					return -1;
			}
		};

		const int lhs_rank = get_rank(lhs->type_kind);
		const int rhs_rank = get_rank(rhs->type_kind);

		if (lhs_rank >= 0 && rhs_rank >= 0)
			return lhs_rank >= rhs_rank ? lhs->type_kind : rhs->type_kind;

		return DataType::INT64;
	}

	Region *Builder::create_region_with_entry(const std::string_view name, Region *parent)
	{
		if (!current_module)
			throw std::runtime_error("no current module");

		if (!parent)
			parent = current_region ? current_region : current_module->get_root_region();

		Region *region = current_module->create_region(std::string(name), parent);

		Region *old_region = current_region;
		set_insertion_point(region);
		create_entry();
		set_insertion_point(old_region);

		return region;
	}

	Node *Builder::name_node(Node *node, std::string_view name)
	{
		node->str_id = ctx.intern_string(name);
		return node;
	}
}
