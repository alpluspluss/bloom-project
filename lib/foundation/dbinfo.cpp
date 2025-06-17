/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/context.hpp>
#include <bloom/foundation/dbinfo.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>

namespace blm
{
	DebugInfo::DebugInfo(Region &reg) : region(reg) {}

	StringTable::StringId DebugInfo::add_source_file(const std::string_view path)
	{
		auto &ctx = region.get_module().get_context();
		const StringTable::StringId id = ctx.intern_string(path);
		if (std::ranges::find(source_files, id) == source_files.end())
			source_files.push_back(id);
		return id;
	}

	std::string_view DebugInfo::get_source_file(const StringTable::StringId file_id) const
	{
		const auto &ctx = region.get_module().get_context();
		return ctx.get_string(file_id);
	}

	void DebugInfo::set_node_location(Node *node, const StringTable::StringId file_id,
	                                  const std::uint32_t line, const std::uint32_t column)
	{
		if (!node)
			return;

		const SourceLocation loc(file_id, line, column);
		node_locations[node] = loc;
		location_to_nodes[loc].push_back(node);
	}

	std::optional<SourceLocation> DebugInfo::get_node_location(Node *node) const
	{
		if (!node)
			return std::nullopt;

		if (const auto it = node_locations.find(node);
			it != node_locations.end())
		{
			return it->second;
		}

		return std::nullopt;
	}

	std::vector<Node *> DebugInfo::find_nodes_at_location(const StringTable::StringId file_id,
	                                                      const std::uint32_t line,
	                                                      const std::uint32_t column) const
	{
		const SourceLocation loc(file_id, line, column);
		if (const auto it = location_to_nodes.find(loc);
			it != location_to_nodes.end())
		{
			return it->second;
		}

		return {};
	}

	void DebugInfo::add_variable(Node *node, const std::string_view name, const std::string_view type_name,
	                             const bool is_param, const std::int32_t frame_offset)
	{
		if (!node)
			return;

		auto &ctx = region.get_module().get_context();
		const StringTable::StringId name_id = ctx.intern_string(name);
		const StringTable::StringId type_id = ctx.intern_string(type_name);

		variables[node] = { name_id, type_id, is_param, frame_offset };
	}

	std::tuple<std::string_view, std::string_view, bool, std::int32_t> DebugInfo::get_variable_info(Node *node) const
	{
		const auto &ctx = region.get_module().get_context();
		if (const auto it = variables.find(node);
			it != variables.end())
		{
			const auto &[name_id, type_id, is_param, frame_offset] = it->second;
			return {
				ctx.get_string(name_id),
				ctx.get_string(type_id),
				is_param,
				frame_offset
			};
		}

		return { "", "", false, 0 };
	}

	void DebugInfo::add_function(Node *node, const std::string_view name)
	{
		if (!node)
			return;

		auto &ctx = region.get_module().get_context();
		const StringTable::StringId name_id = ctx.intern_string(name);
		if (const auto it = functions.find(node);
			it == functions.end())
		{
			functions[node] = { name_id, {}, {} };
		}
		else
		{
			it->second.name_id = name_id;
		}
	}

	void DebugInfo::add_parameter_to_function(Node *func_node, Node *param_node)
	{
		if (!func_node || !param_node)
			return;

		if (const auto it = functions.find(func_node);
			it != functions.end())
		{
			if (auto &params = it->second.parameters;
				std::ranges::find(params, param_node) == params.end())
				params.push_back(param_node);
		}
	}

	void DebugInfo::add_local_var_to_function(Node *func_node, Node *var_node)
	{
		if (!func_node || !var_node)
			return;

		auto it = functions.find(func_node);
		if (it != functions.end())
		{
			auto &locals = it->second.local_vars;
			if (std::ranges::find(locals, var_node) == locals.end())
				locals.push_back(var_node);
		}
	}

	std::vector<Node *> DebugInfo::get_function_parameters(Node *func_node) const
	{
		if (!func_node)
			return {};

		auto it = functions.find(func_node);
		if (it != functions.end())
		{
			return it->second.parameters;
		}

		return {};
	}

	std::vector<Node *> DebugInfo::get_function_local_vars(Node *func_node) const
	{
		if (!func_node)
			return {};

		if (const auto it = functions.find(func_node);
			it != functions.end())
		{
			return it->second.local_vars;
		}

		return {};
	}

	StringTable::StringId DebugInfo::add_type(const std::string_view name,
	                                          const std::uint32_t size,
	                                          const std::uint32_t alignment)
	{
		auto &ctx = region.get_module().get_context();
		const StringTable::StringId name_id = ctx.intern_string(name);

		types[name_id] = { name_id, size, alignment };

		return name_id;
	}

	std::tuple<std::string_view, std::uint32_t, std::uint32_t> DebugInfo::get_type_info(
		const StringTable::StringId type_id) const
	{
		const auto &ctx = region.get_module().get_context();
		if (const auto it = types.find(type_id);
			it != types.end())
		{
			const auto &[name_id, size, alignment] = it->second;
			return {
				ctx.get_string(name_id),
				size,
				alignment
			};
		}

		return { ctx.get_string(type_id), 0, 0 };
	}
}
