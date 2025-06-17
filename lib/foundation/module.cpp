/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>

namespace blm
{
	Module::Module(Context &ctx, const std::string_view name) : context(ctx), root_region(nullptr),
	                                                            rodata_region(nullptr), name_id(ctx.intern_string(name))
	{
		root_region = create_region(name);
		regions.push_back(std::make_unique<Region>(context, *this, ".__rodata", nullptr));
		rodata_region = regions.back().get();
	}

	Module::~Module()
	{
		/* clear references to the dangling pointers */
		functions.clear();
		/* note: regions are destroyed automatically by `std::unique_ptr<Region>` */
	}

	std::string_view Module::get_name() const
	{
		return context.get_string(name_id);
	}

	Region *Module::get_root_region() const
	{
		return root_region;
	}

	Region *Module::create_region(std::string_view name, Region *parent)
	{
		if (!root_region)
		{
			regions.push_back(std::make_unique<Region>(context, *this, name, nullptr));
			return regions.back().get();
		}

		if (!parent)
			parent = root_region;

		regions.push_back(std::make_unique<Region>(context, *this, name, parent));
		Region *result = regions.back().get();
		parent->add_child(result);
		return result;
	}

	Node *Module::find_function(const std::string_view name) const
	{
		const StringTable::StringId id = context.intern_string(name);
		for (Node *func: functions)
		{
			if (func->ir_type == NodeType::FUNCTION && func->str_id == id)
				return func;
		}

		return nullptr;
	}

	void Module::add_function(Node *func)
	{
		if (func && func->ir_type == NodeType::FUNCTION)
			functions.push_back(func);
	}

	Node *Module::intern_string_literal(std::string_view str)
	{
		for (Node* node : rodata_region->get_nodes())
		{
			if (node->ir_type == NodeType::LIT &&
				node->type_kind == DataType::STRING &&
				node->as<DataType::STRING>() == str)
			{
				return node;
			}
		}

		Node* str_lit = context.create<Node>();
		str_lit->ir_type = NodeType::LIT;
		str_lit->type_kind = DataType::STRING;
		str_lit->data.set<std::string, DataType::STRING>(std::string(str));
		str_lit->props |= NodeProps::READONLY;

		rodata_region->add_node(str_lit);
		return str_lit;
	}
}
