/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <bloom/ir/print.hpp>

namespace blm
{
	IRPrinter::IRPrinter(std::ostream &os) : os(os) {}

	IRPrinter::IRPrinter(std::ostream &os, const PrintOptions &options) : os(os), options(options) {}

	void IRPrinter::print_module(const Module &module)
	{
		current_module = &module;
		reset_names();
		build_name_mappings(module);

		os << "#! module: " << module.get_name() << "\n\n";

		print_type_declarations(module);
		print_rodata_section(module);
		print_globals_section(module);
		for (Node *func: module.get_functions())
		{
			if (func->ir_type == NodeType::FUNCTION)
				print_function(func, module);
		}
		current_module = nullptr;
	}

	void IRPrinter::print_function(Node *func, const Module &module)
	{
		if (!func || func->ir_type != NodeType::FUNCTION)
			return;

		const Region *func_region = find_function_region(func, module);
		const Context &ctx = module.get_context();

		print_node_attributes(func);
		print_function_signature(func, func_region, ctx);

		if (options.include_debug_info && func_region)
			print_debug_comment(func, func_region);

		os << "\n{\n";

		if (func_region)
			print_region(*func_region, 1);

		os << "}\n\n";
	}

	void IRPrinter::print_region(const Region &region, std::size_t indent_level)
	{
		std::string region_name = get_block_name(&region);
		os << indent(indent_level) << region_name << ":";

		if (options.include_debug_info)
		{
			os << " /* region: " << region.get_name();
			const auto &debug_info = region.get_debug_info();
			if (Node *control_node = region.get_control_dependency())
			{
				if (const auto location = debug_info.get_node_location(control_node);
					location.has_value())
				{
					const Context &ctx = region.get_module().get_context();
					std::string file = std::string(ctx.get_string(location->file_id));
					os << ", src: " << file << ":" << location->line;
					if (location->column > 0)
						os << ":" << location->column;
				}
			}
			os << " */";
		}

		os << "\n";

		for (Node *node: region.get_nodes())
		{
			if (node->ir_type != NodeType::FUNCTION && node->ir_type != NodeType::PARAM)
				print_instruction(node, indent_level + 1);
		}

		/* print child regions with proper nesting based on dominance */
		for (const Region *child: region.get_children())
		{
			/* use region dominance to determine proper indentation */
			std::size_t child_indent = indent_level;
			if (region.dominates(child))
				child_indent++;

			print_region(*child, child_indent);
		}
	}

	void IRPrinter::print_instruction(Node *node, std::size_t indent_level)
	{
		if (!node)
			return;

		std::string ind = indent(indent_level);

		switch (node->ir_type)
		{
			case NodeType::ENTRY:
				/* entry nodes are implicit in region headers */
				break;

			case NodeType::EXIT:
				os << ind << "exit";
				if ((node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE)
					os << " /* no_optimize */";
				os << ";\n";
				break;

			case NodeType::RET:
				os << ind << "return";
				if (!node->inputs.empty())
					os << " " << get_node_name(node->inputs[0]);
				os << ";\n";
				break;

			case NodeType::LIT:
				if (is_value_producing(node))
				{
					os << ind << get_node_name(node) << " = ";
					if (options.include_type_annotations)
					{
						print_type(node->type_kind, current_module->get_context());
						os << " ";
					}
					print_literal_value(node);
					os << ";\n";
				}
				break;

			case NodeType::ADD:
				print_binary_op(node, "+", indent_level);
				break;
			case NodeType::SUB:
				print_binary_op(node, "-", indent_level);
				break;
			case NodeType::MUL:
				print_binary_op(node, "*", indent_level);
				break;
			case NodeType::DIV:
				print_binary_op(node, "/", indent_level);
				break;
			case NodeType::MOD:
				print_binary_op(node, "%", indent_level);
				break;

			case NodeType::BAND:
				print_binary_op(node, "&", indent_level);
				break;
			case NodeType::BOR:
				print_binary_op(node, "|", indent_level);
				break;
			case NodeType::BXOR:
				print_binary_op(node, "^", indent_level);
				break;
			case NodeType::BSHL:
				print_binary_op(node, "<<", indent_level);
				break;
			case NodeType::BSHR:
				print_binary_op(node, ">>", indent_level);
				break;

			case NodeType::BNOT:
				print_unary_op(node, "~", indent_level);
				break;

			case NodeType::EQ:
				print_comparison_op(node, "==", indent_level);
				break;
			case NodeType::NEQ:
				print_comparison_op(node, "!=", indent_level);
				break;
			case NodeType::LT:
				print_comparison_op(node, "<", indent_level);
				break;
			case NodeType::LTE:
				print_comparison_op(node, "<=", indent_level);
				break;
			case NodeType::GT:
				print_comparison_op(node, ">", indent_level);
				break;
			case NodeType::GTE:
				print_comparison_op(node, ">=", indent_level);
				break;

			case NodeType::LOAD:
			case NodeType::STORE:
			case NodeType::PTR_LOAD:
			case NodeType::PTR_STORE:
			case NodeType::ATOMIC_LOAD:
			case NodeType::ATOMIC_STORE:
			case NodeType::ATOMIC_CAS:
				print_memory_op(node, indent_level);
				break;

			case NodeType::CALL:
			case NodeType::INVOKE:
				print_call_op(node, indent_level);
				break;

			case NodeType::BRANCH:
			case NodeType::JUMP:
				print_control_flow_op(node, indent_level);
				break;

			case NodeType::STACK_ALLOC:
				os << ind << get_node_name(node) << " = stack_alloc ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, current_module->get_context());
					os << " ";
				}
				os << get_node_name(node->inputs[0]);
				if (node->inputs.size() > 1)
					os << ", " << get_node_name(node->inputs[1]);
				os << ";\n";
				break;

			case NodeType::HEAP_ALLOC:
				os << ind << get_node_name(node) << " = heap_alloc ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, current_module->get_context());
					os << " ";
				}
				os << get_node_name(node->inputs[0]) << ", " << get_node_name(node->inputs[1]);
				if (node->inputs.size() > 2)
					os << ", " << get_node_name(node->inputs[2]);
				os << ";\n";
				break;

			case NodeType::FREE:
				os << ind << "free " << get_node_name(node->inputs[0]) << ";\n";
				break;

			case NodeType::ADDR_OF:
				os << ind << get_node_name(node) << " = addr_of ";
				if (!node->inputs.empty() && node->inputs[0]->str_id != 0)
					os << current_module->get_context().get_string(node->inputs[0]->str_id);
				os << ";\n";
				break;

			case NodeType::PTR_ADD:
				os << ind << get_node_name(node) << " = ptr_add ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, current_module->get_context());
					os << " ";
				}
				os << get_node_name(node->inputs[0]) << ", " << get_node_name(node->inputs[1]) << ";\n";
				break;

			case NodeType::REINTERPRET_CAST:
				os << ind << get_node_name(node) << " = reinterpret_cast ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, current_module->get_context());
					os << " ";
				}
				os << get_node_name(node->inputs[0]) << ";\n";
				break;

			case NodeType::VECTOR_BUILD:
				os << ind << get_node_name(node) << " = vector_build ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, current_module->get_context());
					os << " ";
				}
				print_operand_list(node->inputs);
				os << ";\n";
				break;

			case NodeType::VECTOR_EXTRACT:
				os << ind << get_node_name(node) << " = vector_extract ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, current_module->get_context());
					os << " ";
				}
				os << get_node_name(node->inputs[0]);
				if (node->inputs.size() > 1)
					os << ", " << get_node_name(node->inputs[1]);
				os << ";\n";
				break;

			case NodeType::VECTOR_SPLAT:
				os << ind << get_node_name(node) << " = vector_splat ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, current_module->get_context());
					os << " ";
				}
				os << get_node_name(node->inputs[0]) << ";\n";
				break;

			default:
				os << ind << "/* unknown instruction: " << static_cast<int>(node->ir_type) << " */\n";
				break;
		}

		if (options.include_debug_info)
			print_debug_comment(node, node->parent_region);
	}

	void IRPrinter::print_type(DataType type, const Context &ctx)
	{
		switch (type)
		{
			case DataType::VOID:
				os << "void";
				break;
			case DataType::BOOL:
				os << "bool";
				break;
			case DataType::INT8:
				os << "i8";
				break;
			case DataType::INT16:
				os << "i16";
				break;
			case DataType::INT32:
				os << "i32";
				break;
			case DataType::INT64:
				os << "i64";
				break;
			case DataType::UINT8:
				os << "u8";
				break;
			case DataType::UINT16:
				os << "u16";
				break;
			case DataType::UINT32:
				os << "u32";
				break;
			case DataType::UINT64:
				os << "u64";
				break;
			case DataType::FLOAT32:
				os << "f32";
				break;
			case DataType::FLOAT64:
				os << "f64";
				break;
			case DataType::STRING:
				os << "string";
				break;
			case DataType::POINTER:
			{
				const auto &type_data = ctx.get_type(type);
				const auto &ptr_data = type_data.get<DataType::POINTER>();
				os << "ptr<";
				print_type(ptr_data.pointee_type, ctx);
				if (ptr_data.addr_space != 0)
					os << ", " << ptr_data.addr_space;
				os << ">";
				break;
			}
			case DataType::ARRAY:
			{
				const auto &type_data = ctx.get_type(type);
				const auto &array_data = type_data.get<DataType::ARRAY>();
				os << "array<";
				print_type(array_data.elem_type, ctx);
				os << ", " << array_data.count << ">";
				break;
			}
			case DataType::STRUCT:
			{
				const auto &type_data = ctx.get_type(type);
				const auto &struct_data = type_data.get<DataType::STRUCT>();

				/* check if it's a user-defined struct with a name */
				if (!struct_data.fields.empty() && !struct_data.fields[0].first.empty())
				{
					os << "S" << static_cast<std::uint32_t>(get_base_type_id(type));
				}
				else
				{
					os << "struct<";
					for (std::size_t i = 0; i < struct_data.fields.size(); ++i)
					{
						if (i > 0)
							os << ", ";
						os << struct_data.fields[i].first << ": ";
						print_type(struct_data.fields[i].second, ctx);
					}
					os << ">";
				}
				break;
			}
			case DataType::FUNCTION:
			{
				const auto &type_data = ctx.get_type(type);
				const auto &func_data = type_data.get<DataType::FUNCTION>();
				os << "fn<";
				print_type(func_data.return_type, ctx);
				os << "(";
				for (std::size_t i = 0; i < func_data.param_types.size(); ++i)
				{
					if (i > 0)
						os << ", ";
					print_type(func_data.param_types[i], ctx);
				}
				if (func_data.is_vararg)
				{
					if (!func_data.param_types.empty())
						os << ", ";
					os << "...";
				}
				os << ")>";
				break;
			}
			case DataType::VECTOR:
			{
				if (is_vector_type(type))
				{
					const auto &type_data = ctx.get_type(type);
					const auto &vec_data = type_data.get<DataType::VECTOR>();
					os << "vec<";
					print_type(vec_data.elem_type, ctx);
					os << " x " << vec_data.count << ">";
				}
				break;
			}
			default:
				os << "unknown_type<" << static_cast<int>(type) << ">";
				break;
		}
	}

	std::string IRPrinter::get_node_name(Node *node)
	{
		if (!node)
			return "null";

		if (const auto it = node_names.find(node);
			it != node_names.end())
			return it->second;

		/* generate name on demand if not pre-populated */
		std::string name;
		switch (node->ir_type)
		{
			case NodeType::FUNCTION:
				name = generate_function_name(node);
				break;
			case NodeType::PARAM:
				if (node->str_id != 0 && current_module)
				{
					name = "%" + std::string(current_module->get_context().get_string(node->str_id));
				}
				else
				{
					name = "%p" + std::to_string(next_ssa_id++);
				}
				break;
			default:
				name = generate_ssa_name(node);
				break;
		}

		node_names[node] = name;
		return name;
	}

	std::string IRPrinter::get_block_name(const Region *region)
	{
		if (!region)
			return "null_block";

		if (const auto it = region_names.find(region);
			it != region_names.end())
			return it->second;

		std::string name = generate_block_name(region);
		region_names[region] = name;
		return name;
	}

	void IRPrinter::reset_names()
	{
		node_names.clear();
		region_names.clear();
		printed_types.clear();
		next_ssa_id = 0;
		next_block_id = 0;
		next_temp_func_id = 0;
	}

	void IRPrinter::build_name_mappings(const Module &module)
	{
		for (Node *func: module.get_functions())
		{
			if (func->ir_type == NodeType::FUNCTION)
				node_names[func] = generate_function_name(func);
		}

		std::function<void(const Region *)> map_regions = [&](const Region *region)
		{
			region_names[region] = generate_block_name(region);
			for (const Region *child: region->get_children())
				map_regions(child);
		};
		map_regions(module.get_root_region());
	}

	void IRPrinter::print_type_declarations(const Module &module)
	{
		const Context &ctx = module.get_context();
		std::vector<DataType> types_to_declare;
		std::function<void(DataType)> collect_type = [&](DataType type)
		{
			if (needs_type_declaration(type, ctx) && !printed_types.contains(type))
			{
				types_to_declare.push_back(type);
				printed_types.insert(type);
			}
		};

		for (Node *func: module.get_functions())
		{
			if (func->ir_type != NodeType::FUNCTION)
				continue;

			const Region *func_region = find_function_region(func, module);
			if (!func_region)
				continue;

			std::function<void(const Region *)> scan_region = [&](const Region *region)
			{
				for (Node *node: region->get_nodes())
				{
					collect_type(node->type_kind);
					for (Node *input: node->inputs)
						if (input)
							collect_type(input->type_kind);
				}
				for (const Region *child: region->get_children())
					scan_region(child);
			};
			scan_region(func_region);
		}

		for (DataType type: types_to_declare)
		{
			if (is_struct_type(type))
			{
				const auto &type_data = ctx.get_type(type);
				const auto &struct_data = type_data.get<DataType::STRUCT>();

				os << "type S" << static_cast<std::uint32_t>(get_base_type_id(type));
				os << " = struct {\n";
				for (const auto &[field_name, field_type]: struct_data.fields)
				{
					os << "    " << field_name << ": ";
					print_type(field_type, ctx);
					os << ";\n";
				}
				os << "}; /* size=" << struct_data.size << ", align=" << struct_data.alignment << " */\n\n";
			}
		}
	}

	std::string IRPrinter::generate_ssa_name(Node *node)
	{
		if (node->str_id != 0 && current_module)
		{
			std::string user_name = std::string(current_module->get_context().get_string(node->str_id));
			return "%" + user_name;
		}
		return "%" + std::to_string(next_ssa_id++);
	}

	std::string IRPrinter::generate_function_name(Node *node)
	{
		if (node->str_id != 0 && current_module)
		{
			std::string func_name = std::string(current_module->get_context().get_string(node->str_id));
			return "$" + func_name;
		}
		return "$func" + std::to_string(next_temp_func_id++);
	}

	std::string IRPrinter::generate_block_name(const Region *region)
	{
		if (region->get_parent() == nullptr)
			return "root";

		std::string region_name = std::string(region->get_name());
		if (!region_name.empty())
		{
			for (char &c: region_name)
			{
				if (!std::isalnum(c) && c != '_')
					c = '_';
			}
			return region_name;
		}

		return "block" + std::to_string(next_block_id++);
	}

	void IRPrinter::print_function_signature(Node *func, const Region *func_region, const Context &ctx)
	{
		os << "fn " << get_node_name(func) << "(";
		print_function_parameters(func_region, ctx);
		os << ") -> ";

		/* determine return type */
		if (func->data.type() == DataType::FUNCTION)
		{
			const auto &func_data = func->data.get<DataType::FUNCTION>();
			print_type(func_data.return_type, ctx);
		}
		else if (is_function_type(func->type_kind))
		{
			const auto &type_data = ctx.get_type(func->type_kind);
			const auto &func_data = type_data.get<DataType::FUNCTION>();
			print_type(func_data.return_type, ctx);
		}
		else
		{
			os << "void";
		}
	}

	void IRPrinter::print_function_parameters(const Region *func_region, const Context &ctx)
	{
		if (!func_region)
			return;

		std::vector<Node *> params;
		for (Node *node: func_region->get_nodes())
		{
			if (node->ir_type == NodeType::PARAM && !node->users.empty())
				params.push_back(node);
		}

		bool first = true;
		for (Node *param: params)
		{
			if (!first)
				os << ", ";
			first = false;

			print_type(param->type_kind, ctx);
			os << " " << get_node_name(param);
		}
	}

	void IRPrinter::print_literal_value(Node *node)
	{
		if (node->ir_type != NodeType::LIT)
			return;

		switch (node->type_kind)
		{
			case DataType::BOOL:
				os << (node->as<DataType::BOOL>() ? "true" : "false");
				break;
			case DataType::INT8:
				os << static_cast<int>(node->as<DataType::INT8>());
				break;
			case DataType::INT16:
				os << node->as<DataType::INT16>();
				break;
			case DataType::INT32:
				os << node->as<DataType::INT32>();
				break;
			case DataType::INT64:
				os << node->as<DataType::INT64>() << "L";
				break;
			case DataType::UINT8:
				os << static_cast<unsigned>(node->as<DataType::UINT8>()) << "u";
				break;
			case DataType::UINT16:
				os << node->as<DataType::UINT16>() << "u";
				break;
			case DataType::UINT32:
				os << node->as<DataType::UINT32>() << "u";
				break;
			case DataType::UINT64:
				os << node->as<DataType::UINT64>() << "uL";
				break;
			case DataType::FLOAT32:
			{
				float f = node->as<DataType::FLOAT32>();
				os << std::fixed << std::setprecision(6) << f;
				if (std::floor(f) == f)
					os << ".0";
				os << "f";
				break;
			}
			case DataType::FLOAT64:
			{
				double d = node->as<DataType::FLOAT64>();
				os << std::fixed << std::setprecision(15) << d;
				if (std::floor(d) == d)
					os << ".0";
				break;
			}
			case DataType::POINTER:
				os << "null";
				break;
			default:
				os << "unknown_literal";
				break;
		}
	}

	void IRPrinter::print_node_attributes(Node *node)
	{
		if ((node->props & NodeProps::EXTERN) != NodeProps::NONE)
			os << "extern ";
		if ((node->props & NodeProps::EXPORT) != NodeProps::NONE)
			os << "export ";
		if ((node->props & NodeProps::STATIC) != NodeProps::NONE)
			os << "static ";
	}

	void IRPrinter::print_debug_comment(Node *node, const Region *region)
	{
		if (!options.include_debug_info || !region)
			return;

		const auto &debug_info = region->get_debug_info();
		if (const auto location = debug_info.get_node_location(node);
			location.has_value())
		{
			const Context &ctx = region->get_module().get_context();
			std::string file = std::string(ctx.get_string(location->file_id));
			os << " /* " << file << ":" << location->line;
			if (location->column > 0)
				os << ":" << location->column;
			os << " */";
		}
	}

	std::string IRPrinter::indent(std::size_t level) const
	{
		std::string result;
		std::size_t total_indent = level * options.indent_size;

		if (options.use_spaces)
			result.resize(total_indent, ' ');
		else
			result.resize(level, '\t');
		return result;
	}

	bool IRPrinter::needs_type_declaration(DataType type, const Context &ctx)
	{
		if (!is_struct_type(type))
			return false;

		const auto &type_data = ctx.get_type(type);
		const auto &struct_data = type_data.get<DataType::STRUCT>();
		/* only declare user-defined structs with named fields */
		return !struct_data.fields.empty() && !struct_data.fields[0].first.empty();
	}

	const Region *IRPrinter::find_function_region(Node *func, const Module &module)
	{
		if (!func || func->ir_type != NodeType::FUNCTION)
			return nullptr;

		const Context &ctx = module.get_context();
		std::string_view func_name = ctx.get_string(func->str_id);

		for (const Region *child: module.get_root_region()->get_children())
		{
			if (child->get_name() == func_name)
				return child;
		}

		return nullptr;
	}

	bool IRPrinter::is_value_producing(Node *node)
	{
		switch (node->ir_type)
		{
			case NodeType::ENTRY:
			case NodeType::EXIT:
			case NodeType::RET:
			case NodeType::STORE:
			case NodeType::PTR_STORE:
			case NodeType::ATOMIC_STORE:
			case NodeType::FREE:
			case NodeType::BRANCH:
			case NodeType::JUMP:
				return false;
			default:
				return true;
		}
	}

	void IRPrinter::print_operand_list(const std::vector<Node *> &operands, std::size_t start_index)
	{
		for (std::size_t i = start_index; i < operands.size(); ++i)
		{
			if (i > start_index)
				os << ", ";
			os << get_node_name(operands[i]);
		}
	}

	void IRPrinter::print_binary_op(Node *node, const std::string &op_symbol, std::size_t indent_level)
	{
		if (node->inputs.size() < 2)
			return;

		os << indent(indent_level) << get_node_name(node) << " = ";
		if (options.include_type_annotations)
		{
			print_type(node->type_kind, current_module->get_context());
			os << " ";
		}
		os << get_node_name(node->inputs[0]) << " " << op_symbol << " " << get_node_name(node->inputs[1]) << ";\n";
	}

	void IRPrinter::print_unary_op(Node *node, const std::string &op_symbol, std::size_t indent_level)
	{
		if (node->inputs.empty())
			return;

		os << indent(indent_level) << get_node_name(node) << " = ";
		if (options.include_type_annotations)
		{
			print_type(node->type_kind, current_module->get_context());
			os << " ";
		}
		os << op_symbol << get_node_name(node->inputs[0]) << ";\n";
	}

	void IRPrinter::print_comparison_op(Node *node, const std::string &op_symbol, std::size_t indent_level)
	{
		if (node->inputs.size() < 2)
			return;

		os << indent(indent_level) << get_node_name(node) << " = ";
		os << get_node_name(node->inputs[0]) << " " << op_symbol << " " << get_node_name(node->inputs[1]) << ";\n";
	}

	void IRPrinter::print_memory_op(Node *node, std::size_t indent_level)
	{
	    std::string ind = indent(indent_level);

	    switch (node->ir_type)
	    {
	        case NodeType::LOAD:
	            os << ind << get_node_name(node) << " = load ";
	            if (options.include_type_annotations)
	            {
	                print_type(node->type_kind, current_module->get_context());
	                os << " ";
	            }
	            if (node->str_id != 0)
	                os << current_module->get_context().get_string(node->str_id);
	            os << ";\n";
	            break;

	        case NodeType::STORE:
	            os << ind << "store ";
	            if (options.include_type_annotations && !node->inputs.empty())
	            {
	                print_type(node->inputs[0]->type_kind, current_module->get_context());
	                os << " ";
	            }
	            if (node->inputs.size() >= 2)
	                os << get_node_name(node->inputs[0]) << ", ";
	            if (node->str_id != 0)
	                os << current_module->get_context().get_string(node->str_id);
	            os << ";\n";
	            break;

	        case NodeType::PTR_LOAD:
	            os << ind << get_node_name(node) << " = load ";
	            if (options.include_type_annotations)
	            {
	                print_type(node->type_kind, current_module->get_context());
	                os << " ";
	            }
	            if (!node->inputs.empty())
	                os << get_node_name(node->inputs[0]);
	            os << ";\n";
	            break;

	        case NodeType::PTR_STORE:
	            os << ind << "store ";
	            if (options.include_type_annotations && !node->inputs.empty())
	            {
	                print_type(node->inputs[0]->type_kind, current_module->get_context());
	                os << " ";
	            }
	            if (node->inputs.size() >= 2)
	                os << get_node_name(node->inputs[0]) << ", " << get_node_name(node->inputs[1]);
	            os << ";\n";
	            break;

	        case NodeType::ATOMIC_LOAD:
	            os << ind << get_node_name(node) << " = atomic load ";
	            if (options.include_type_annotations)
	            {
	                print_type(node->type_kind, current_module->get_context());
	                os << " ";
	            }
	            if (!node->inputs.empty())
	                os << get_node_name(node->inputs[0]);
	            if (node->inputs.size() > 1)
	                os << ", " << get_node_name(node->inputs[1]);
	            os << ";\n";
	            break;

	        case NodeType::ATOMIC_STORE:
	            os << ind << "atomic store ";
	            if (options.include_type_annotations && !node->inputs.empty())
	            {
	                print_type(node->inputs[0]->type_kind, current_module->get_context());
	                os << " ";
	            }
	            if (node->inputs.size() >= 2)
	                os << get_node_name(node->inputs[0]) << ", " << get_node_name(node->inputs[1]);
	            if (node->inputs.size() > 2)
	                os << ", " << get_node_name(node->inputs[2]);
	            os << ";\n";
	            break;

	        case NodeType::ATOMIC_CAS:
	            os << ind << get_node_name(node) << " = atomic cas ";
	            if (options.include_type_annotations)
	            {
	                print_type(node->type_kind, current_module->get_context());
	                os << " ";
	            }
	            if (node->inputs.size() >= 3)
	            {
	                os << get_node_name(node->inputs[0]) << ", "
	                   << get_node_name(node->inputs[1]) << ", "
	                   << get_node_name(node->inputs[2]);
	            }
	            if (node->inputs.size() > 3)
	                os << ", " << get_node_name(node->inputs[3]);
	            os << ";\n";
	            break;

	        default:
	            break;
	    }
	}

	void IRPrinter::print_control_flow_op(Node *node, std::size_t indent_level)
	{
		std::string ind = indent(indent_level);

		switch (node->ir_type)
		{
			case NodeType::BRANCH:
				os << ind << "branch " << get_node_name(node->inputs[0]) << " ? ";
				if (node->inputs.size() >= 2)
				{
					Node *true_target = node->inputs[1];
					if (true_target->ir_type == NodeType::ENTRY && true_target->parent_region)
						os << get_block_name(true_target->parent_region);
					else
						os << get_node_name(true_target);
				}
				os << " : ";
				if (node->inputs.size() >= 3)
				{
					Node *false_target = node->inputs[2];
					if (false_target->ir_type == NodeType::ENTRY && false_target->parent_region)
						os << get_block_name(false_target->parent_region);
					else
						os << get_node_name(false_target);
				}
				os << ";\n";
				break;

			case NodeType::JUMP:
				os << ind << "jump ";
				if (!node->inputs.empty())
				{
					Node *target = node->inputs[0];
					if (target->ir_type == NodeType::ENTRY && target->parent_region)
						os << get_block_name(target->parent_region);
					else
						os << get_node_name(target);
				}
				os << ";\n";
				break;

			default:
				break;
		}
	}

	void IRPrinter::print_call_op(Node *node, std::size_t indent_level)
	{
		std::string ind = indent(indent_level);

		switch (node->ir_type)
		{
			case NodeType::CALL:
				os << ind;
				if (is_value_producing(node))
					os << get_node_name(node) << " = ";
				os << "call " << get_node_name(node->inputs[0]) << "(";
				print_operand_list(node->inputs, 1);
				os << ");\n";
				break;

			case NodeType::INVOKE:
				os << ind;
				if (is_value_producing(node))
					os << get_node_name(node) << " = ";
				os << "invoke " << get_node_name(node->inputs[0]) << "(";
			/* arguments are inputs[1] to inputs[n-2] */
				for (std::size_t i = 1; i < node->inputs.size() - 2; ++i)
				{
					if (i > 1)
						os << ", ";
					os << get_node_name(node->inputs[i]);
				}
				os << ") normal ";
				if (node->inputs.size() >= 2)
				{
					Node *normal_target = node->inputs[node->inputs.size() - 2];
					if (normal_target->ir_type == NodeType::ENTRY && normal_target->parent_region)
						os << get_block_name(normal_target->parent_region);
					else
						os << get_node_name(normal_target);
				}
				os << " exception ";
				if (!node->inputs.empty())
				{
					Node *exception_target = node->inputs[node->inputs.size() - 1];
					if (exception_target->ir_type == NodeType::ENTRY && exception_target->parent_region)
						os << get_block_name(exception_target->parent_region);
					else
						os << get_node_name(exception_target);
				}
				os << ";\n";
				break;

			default:
				break;
		}
	}

	void IRPrinter::print_rodata_section(const Module &module)
	{
		Region *rodata_region = module.get_rodata_region();
		if (!rodata_region || rodata_region->get_nodes().empty())
			return;

		os << "section .__rodata:\n";

		for (Node *node: rodata_region->get_nodes())
		{
			if (node->ir_type == NodeType::LIT && node->type_kind == DataType::STRING)
			{
				os << "    " << get_node_name(node) << " = ";
				if (options.include_type_annotations)
				{
					print_type(node->type_kind, module.get_context());
					os << " ";
				}
				if (node->type_kind != DataType::STRING)
					return;

				const std::string &str = node->as<DataType::STRING>();
				os << "\"";

				for (char c: str)
				{
					switch (c)
					{
						case '\n':
							os << "\\n";
							break;
						case '\t':
							os << "\\t";
							break;
						case '\r':
							os << "\\r";
							break;
						case '\\':
							os << "\\\\";
							break;
						case '\"':
							os << "\\\"";
							break;
						default:
							os << c;
							break;
					}
				}

				os << "\"" << ";\n";
			}
		}

		os << ".__rodata_end:\n" << "\n";
	}

	void IRPrinter::print_globals_section(const Module &module)
	{
		Region* root_region = module.get_root_region();
		if (!root_region || root_region->get_nodes().empty())
			return;

		auto has_globals = false;
		for (const Node* node : root_region->get_nodes())
		{
			/* functions get printed separately */
			if (node->ir_type != NodeType::FUNCTION)
			{
				has_globals = true;
				break;
			}
		}

		if (!has_globals)
			return;

		os << "section .__global:\n";
		for (Node* node : root_region->get_nodes())
		{
			if (node->ir_type == NodeType::FUNCTION)
				continue;

			os << "    ";
			print_instruction(node, 0);
		}

		os << ".__global_end:\n\n";
	}
}
