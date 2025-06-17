/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>

namespace blm
{
	Context::Context() = default;

	Context::~Context()
	{
		modules.clear();
		module_map.clear();
	}

	Module *Context::create_module(std::string_view name)
	{
		const StringTable::StringId name_id = intern_string(name);
		if (const auto it = module_map.find(name_id);
			it != module_map.end())
		{
			return it->second;
		}

		modules.push_back(std::make_unique<Module>(*this, name));
		Module *module_ptr = modules.back().get();
		module_map[name_id] = module_ptr;
		return module_ptr;
	}

	Module *Context::find_module(const std::string_view name)
	{
		const StringTable::StringId name_id = intern_string(name);
		if (const auto it = module_map.find(name_id);
			it != module_map.end())
		{
			return it->second;
		}
		return nullptr;
	}

	StringTable::StringId Context::intern_string(const std::string_view str)
	{
		return string_table.intern(str);
	}

	std::string_view Context::get_string(const StringTable::StringId id) const
	{
		return string_table.get(id);
	}
}
