/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/transform/sroa.hpp>

namespace blm
{
	std::string_view SROAPass::name() const
	{
		return "scalar-replacement-of-aggregates";
	}

	std::string_view SROAPass::description() const
	{
		return "replaces struct allocations with individual scalar allocations when safe";
	}

	std::vector<const std::type_info *> SROAPass::required_passes() const
	{
		return get_pass_types<LocalAliasAnalysisPass>();
	}

	bool SROAPass::run(Module &module, PassContext &context)
	{
		const auto *alias_result = context.get_result<LocalAliasResult>();
		std::unique_ptr<LocalAliasResult> local_result;
		if (!alias_result)
		{
			LocalAliasAnalysisPass laa;
			local_result = std::unique_ptr<LocalAliasResult>(
				dynamic_cast<LocalAliasResult *>(laa.analyze(module, context).release()));
			alias_result = local_result.get();
		}

		candidates.clear();
		field_accesses.clear();

		find_candidates(module, *alias_result);

		std::size_t promoted_allocations = 0;
		std::size_t scalar_replacements = 0;
		for (auto &[alloc_node, info]: candidates)
		{
			if (analyze_struct_uses(alloc_node, info, *alias_result))
			{
				if (transform_allocation(info, module))
				{
					promoted_allocations++;
					scalar_replacements += std::ranges::count_if(info.scalar_allocs,
					                                             [](Node *n)
					                                             {
						                                             return n != nullptr;
					                                             });
				}
			}
		}

		context.update_stat("sroa.promoted_allocations", promoted_allocations);
		context.update_stat("sroa.scalar_replacements", scalar_replacements);
		return promoted_allocations > 0;
	}

	void SROAPass::find_candidates(Module &module, const LocalAliasResult &alias_result)
	{
		find_candidates_in_region(module.get_root_region(), alias_result);

		for (Node *func: module.get_functions())
		{
			if (func->ir_type != NodeType::FUNCTION)
				continue;

			for (const Region *child: module.get_root_region()->get_children())
			{
				if (child->get_name() == module.get_context().get_string(func->str_id))
				{
					find_candidates_in_region(child, alias_result);
					break;
				}
			}
		}
	}

	void SROAPass::find_candidates_in_region(const Region *region, const LocalAliasResult &alias_result) // NOLINT(*-no-recursion)
	{
		if (!region)
			return;

		for (Node *node: region->get_nodes())
		{
			if (node->ir_type == NodeType::STACK_ALLOC && is_pointer_type(node->type_kind))
			{
				const auto &type_data = region->get_module().get_context().get_type(node->type_kind);
				const auto &ptr_data = type_data.get<DataType::POINTER>();
				DataType pointee_type = ptr_data.pointee_type;
				if (is_struct_type(pointee_type))
				{
					if (!alias_result.has_escaped(node))
					{
						AllocationInfo info;
						info.alloc_node = node;
						info.struct_type = pointee_type;
						info.fully_promotable = true;

						const auto &struct_data = region->get_module().get_context().get_type(pointee_type);
						info.fields = struct_data.get<DataType::STRUCT>().fields;
						candidates[node] = std::move(info);
					}
				}
			}
		}

		for (const Region *child: region->get_children())
			find_candidates_in_region(child, alias_result);
	}

	bool SROAPass::analyze_struct_uses(Node *alloc, AllocationInfo &info, const LocalAliasResult &alias_result)
	{
		Module &module = alloc->parent_region->get_module();
		if (!analyze_uses_in_region(module.get_root_region(), alloc, info, alias_result))
			return false;

		for (Node *func: module.get_functions())
		{
			if (func->ir_type != NodeType::FUNCTION)
				continue;

			for (const Region *child: module.get_root_region()->get_children())
			{
				if (child->get_name() == module.get_context().get_string(func->str_id))
				{
					if (!analyze_uses_in_region(child, alloc, info, alias_result))
						return false;
					break;
				}
			}
		}

		return true;
	}

	bool SROAPass::analyze_uses_in_region(const Region *region, Node *alloc, AllocationInfo &info, // NOLINT(*-no-recursion)
	                                      const LocalAliasResult &alias_result)
	{
		if (!region)
			return true;

		for (Node *node: region->get_nodes())
		{
			bool uses_alloc = false;
			for (const Node *input: node->inputs)
			{
				if (input == alloc)
				{
					uses_alloc = true;
					break;
				}
			}

			bool check_call = (node->ir_type == NodeType::CALL || node->ir_type == NodeType::INVOKE);

			if (!uses_alloc && !check_call)
				continue;

			switch (node->ir_type)
			{
				case NodeType::ADDR_OF:
				{
					for (Node *addr_user: node->users)
					{
						if (addr_user->ir_type == NodeType::PTR_ADD)
						{
							if (addr_user->inputs.size() >= 2 &&
							    addr_user->inputs[1]->ir_type == NodeType::LIT)
							{
								std::size_t field_index;
								if (is_field_access(addr_user, alloc, field_index))
								{
									for (Node *field_user: addr_user->users)
									{
										if (field_user->ir_type == NodeType::LOAD ||
										    field_user->ir_type == NodeType::PTR_LOAD)
										{
											auto access = FieldAccess();											access.access_node = field_user;
											access.field_index = field_index;
											access.is_store = false;
											field_accesses[alloc].push_back(access);
										}
										else if (field_user->ir_type == NodeType::STORE ||
										         field_user->ir_type == NodeType::PTR_STORE)
										{
											auto access = FieldAccess();											access.access_node = field_user;
											access.field_index = field_index;
											access.is_store = true;
											field_accesses[alloc].push_back(access);
										}
										else
										{
											info.escaped_fields.insert(field_index);
											info.fully_promotable = false;
										}
									}
								}
								else
								{
									return false;
								}
							}
							else
							{
								return false;
							}
						}
						else if (addr_user->ir_type == NodeType::LOAD ||
						         addr_user->ir_type == NodeType::PTR_LOAD)
						{
							/* direct load from struct address; offset 0, first field */
							if (!info.fields.empty())
							{
								auto access = FieldAccess();
								access.access_node = addr_user;
								access.field_index = 0;
								access.is_store = false;
								field_accesses[alloc].push_back(access);
							}
						}
						else if (addr_user->ir_type == NodeType::STORE ||
						         addr_user->ir_type == NodeType::PTR_STORE)
						{
							/* direct store to struct address; offset 0, first field */
							if (!info.fields.empty())
							{
								auto access = FieldAccess();
								access.access_node = addr_user;
								access.field_index = 0;
								access.is_store = true;
								field_accesses[alloc].push_back(access);
							}
						}
						else
						{
							return false;
						}
					}
					break;
				}

				case NodeType::CALL:
				case NodeType::INVOKE:
				{
					for (std::size_t i = 1; i < node->inputs.size(); ++i) /* skip function at index 0 */
					{
						Node *arg = node->inputs[i];

						if (arg && arg->ir_type == NodeType::PTR_ADD)
						{
							if (arg->inputs.size() >= 2 &&
							    arg->inputs[0] && arg->inputs[0]->ir_type == NodeType::ADDR_OF &&
							    !arg->inputs[0]->inputs.empty() && arg->inputs[0]->inputs[0] == alloc)
							{
								std::size_t field_index;
								if (is_field_access(arg, alloc, field_index))
								{
									info.escaped_fields.insert(field_index);
									info.fully_promotable = false;
								}
								else
								{
									return false;
								}
							}
						}
						else if (arg == alloc)
						{
							return false;
						}
					}
					break;
				}

				case NodeType::RET:
				default:
					return false;
			}
		}

		for (const Region *child: region->get_children())
		{
			if (!analyze_uses_in_region(child, alloc, info, alias_result))
				return false;
		}

		return true;
	}

	bool SROAPass::is_field_access(Node *node, Node *alloc, std::size_t &field_index)
	{
		if (!node || node->ir_type != NodeType::PTR_ADD || node->inputs.size() < 2)
			return false;

		Node *base = node->inputs[0];
		if (!base || base->ir_type != NodeType::ADDR_OF ||
		    base->inputs.empty() || base->inputs[0] != alloc)
			return false;

		/* get the offset */
		Node *offset_node = node->inputs[1];
		if (!offset_node || offset_node->ir_type != NodeType::LIT)
			return false;

		std::int64_t offset = 0;
		switch (offset_node->type_kind)
		{
			case DataType::INT32:
				offset = static_cast<std::int64_t>(offset_node->as<DataType::INT32>());
				break;
			case DataType::INT64:
				offset = offset_node->as<DataType::INT64>();
				break;
			case DataType::UINT32:
				offset = static_cast<std::int64_t>(offset_node->as<DataType::UINT32>());
				break;
			case DataType::UINT64:
				offset = static_cast<std::int64_t>(offset_node->as<DataType::UINT64>());
				break;
			default:
				return false;
		}

		const auto it = candidates.find(alloc);
		if (it == candidates.end())
			return false;

		field_index = get_field_index_from_offset(offset, it->second.fields);
		return field_index < it->second.fields.size();
	}

	std::size_t SROAPass::get_field_index_from_offset(std::int64_t offset,
	                                                  const std::vector<std::pair<std::string, DataType> > &fields)
	{
		std::vector<std::uint64_t> offsets = calculate_field_offsets(fields);
		for (std::size_t i = 0; i < offsets.size(); ++i)
		{
			if (static_cast<std::int64_t>(offsets[i]) == offset)
				return i;
		}

		return fields.size(); /* invalid index */
	}

	bool SROAPass::transform_allocation(AllocationInfo &info, Module &module)
	{
		if (info.fully_promotable)
		{
			create_scalar_allocations(info, module);
			replace_field_accesses(info);
			if (info.alloc_node->parent_region)
				info.alloc_node->parent_region->remove_node(info.alloc_node);
			return true;
		}
		if (!info.escaped_fields.empty() && info.escaped_fields.size() < info.fields.size())
		{
			const DataType reduced_type = create_reduced_struct_type(info, module);
			info.alloc_node->type_kind = reduced_type;
			create_scalar_allocations(info, module);
			replace_field_accesses(info);
			return true;
		}

		return false;
	}

	void SROAPass::create_scalar_allocations(AllocationInfo &info, Module &module)
	{
		Context &ctx = module.get_context();
		Region *alloc_region = info.alloc_node->parent_region;

		info.scalar_allocs.resize(info.fields.size());
		Node *insert_point = info.alloc_node;

		for (std::size_t i = 0; i < info.fields.size(); ++i)
		{
			if (info.escaped_fields.contains(i))
				continue;

			const DataType field_type = info.fields[i].second;
			std::uint64_t field_size = get_type_size(field_type);
			std::uint32_t field_align = 0;
			switch (field_type)
			{
				case DataType::BOOL:
				case DataType::INT8:
				case DataType::UINT8:
					field_align = 1;
					break;
				case DataType::INT16:
				case DataType::UINT16:
					field_align = 2;
					break;
				case DataType::INT32:
				case DataType::UINT32:
				case DataType::FLOAT32:
					field_align = 4;
					break;
				case DataType::INT64:
				case DataType::UINT64:
				case DataType::FLOAT64:
				case DataType::POINTER:
					field_align = 8;
					break;
				default:
					field_align = static_cast<std::uint32_t>(field_size);
					if (field_align > 8)
						field_align = 8;
					break;
			}

			Node *size_node = ctx.create<Node>();
			size_node->ir_type = NodeType::LIT;
			size_node->type_kind = DataType::UINT64;
			size_node->data.set<std::uint64_t, DataType::UINT64>(field_size);
			alloc_region->insert_node_after(insert_point, size_node);

			Node *align_node = ctx.create<Node>();
			align_node->ir_type = NodeType::LIT;
			align_node->type_kind = DataType::UINT32;
			align_node->data.set<std::uint32_t, DataType::UINT32>(field_align);
			alloc_region->insert_node_after(size_node, align_node);

			Node *scalar_alloc = ctx.create<Node>();
			scalar_alloc->ir_type = NodeType::STACK_ALLOC;
			scalar_alloc->type_kind = ctx.create_pointer_type(field_type);
			scalar_alloc->inputs.push_back(size_node);
			scalar_alloc->inputs.push_back(align_node);
			size_node->users.push_back(scalar_alloc);
			align_node->users.push_back(scalar_alloc);
			alloc_region->insert_node_after(align_node, scalar_alloc);

			info.scalar_allocs[i] = scalar_alloc;
			insert_point = scalar_alloc;
		}
	}

	void SROAPass::replace_field_accesses(AllocationInfo &info)
	{
		auto access_it = field_accesses.find(info.alloc_node);
		if (access_it == field_accesses.end())
			return;

		for (const FieldAccess &access: access_it->second)
		{
			std::size_t field_idx = access.field_index;

			if (info.escaped_fields.contains(field_idx))
				continue;

			if (field_idx >= info.scalar_allocs.size() || !info.scalar_allocs[field_idx])
				continue;

			Node *scalar_alloc = info.scalar_allocs[field_idx];
			Node *access_node = access.access_node;
			if (access.is_store)
			{
				/* replace store to field with store to scalar */
				if (access_node->inputs.size() >= 2)
				{
					if (Node *field_addr = access_node->inputs[1];
						field_addr && (field_addr->ir_type == NodeType::PTR_ADD ||
						               field_addr->ir_type == NodeType::ADDR_OF))
					{
						access_node->inputs[1] = scalar_alloc;

						auto &field_users = field_addr->users;
						std::erase(field_users, access_node);
						scalar_alloc->users.push_back(access_node);
					}
				}
			}
			else
			{
				/* replace load from field with load from scalar */
				if (!access_node->inputs.empty())
				{
					Node *field_addr = access_node->inputs[0];
					if (field_addr && (field_addr->ir_type == NodeType::PTR_ADD ||
					                   field_addr->ir_type == NodeType::ADDR_OF))
					{
						access_node->inputs[0] = scalar_alloc;
						auto &field_users = field_addr->users;
						std::erase(field_users, access_node);
						scalar_alloc->users.push_back(access_node);
					}
				}
			}
		}
	}

	DataType SROAPass::create_reduced_struct_type(const AllocationInfo &info, Module &module)
	{
		std::vector<std::pair<std::string, DataType> > reduced_fields;

		for (std::size_t i = 0; i < info.fields.size(); ++i)
		{
			if (info.escaped_fields.contains(i))
				reduced_fields.push_back(info.fields[i]);
		}

		if (reduced_fields.empty())
			return module.get_context().create_struct_type({}, 0, 1);

		const std::vector<std::uint64_t> field_offsets = calculate_field_offsets(reduced_fields);
		std::uint64_t last_field_size = get_type_size(reduced_fields.back().second);
		auto total_size = static_cast<std::uint32_t>(field_offsets.back() + last_field_size);
		std::uint32_t max_align = 1;
		for (const auto &[name, type]: reduced_fields)
		{
			std::uint32_t field_align = 0;
			switch (type)
			{
				case DataType::BOOL:
				case DataType::INT8:
				case DataType::UINT8:
					field_align = 1;
					break;
				case DataType::INT16:
				case DataType::UINT16:
					field_align = 2;
					break;
				case DataType::INT32:
				case DataType::UINT32:
				case DataType::FLOAT32:
					field_align = 4;
					break;
				case DataType::INT64:
				case DataType::UINT64:
				case DataType::FLOAT64:
				case DataType::POINTER:
					field_align = 8;
					break;
				default:
					field_align = static_cast<std::uint32_t>(get_type_size(type));
					if (field_align > 8)
						field_align = 8;
					break;
			}

			if (field_align > max_align)
				max_align = field_align;
		}

		if (max_align > 1)
			total_size = total_size + max_align - 1 & ~(max_align - 1);
		return module.get_context().create_struct_type(reduced_fields, total_size, max_align);
	}

	std::uint64_t SROAPass::get_type_size(DataType type)
	{
		switch (type)
		{
			case DataType::BOOL:
			case DataType::INT8:
			case DataType::UINT8:
				return 1;
			case DataType::INT16:
			case DataType::UINT16:
				return 2;
			case DataType::INT32:
			case DataType::UINT32:
			case DataType::FLOAT32:
				return 4;
			case DataType::INT64:
			case DataType::UINT64:
			case DataType::FLOAT64:
			case DataType::POINTER:
				return 8;
			default:
				return 0; /* unknown or complex type */
		}
	}

	std::vector<std::uint64_t> SROAPass::calculate_field_offsets(
		const std::vector<std::pair<std::string, DataType> > &fields)
	{
		std::vector<std::uint64_t> offsets;
		std::uint64_t current_offset = 0;

		for (const auto &[name, type]: fields)
		{
			const std::uint64_t field_size = get_type_size(type);
			if (std::uint64_t alignment = field_size;
				alignment > 1)
			{
				current_offset = (current_offset + alignment - 1) & ~(alignment - 1);
			}

			offsets.push_back(current_offset);
			current_offset += field_size;
		}

		return offsets;
	}
}
