/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <bloom/ipo/specializer.hpp>

namespace blm
{
	Node *FunctionSpecializer::specialize_function(const SpecializationRequest &req, Module &target_module)
	{
		if (!req.original_function || req.original_function->ir_type != NodeType::FUNCTION)
			return nullptr;

		if (!should_specialize(req))
			return nullptr;

		std::uint64_t cache_key = compute_specialization_key(req.original_function, req.specialized_params);
		if (const auto it = specialization_cache.find(cache_key); it != specialization_cache.end())
		{
			Node* existing = it->second;
			if (!req.call_sites.empty())
				redirect_call_sites(req, req.call_sites, existing);
			return existing;
		}

		/* not cached */
		Node *cloned_func = clone_function_skeleton(req.original_function, target_module, req.specialized_params);
		if (!cloned_func)
			return nullptr;

		std::vector modules = { &target_module };
		Region *original_region = find_function_region(req.original_function, modules);
		if (!original_region)
			return nullptr;

		std::unordered_map<Node *, Node *> node_mapping;
		node_mapping[req.original_function] = cloned_func;
		Region *cloned_region = clone_region_hierarchy(original_region, target_module, node_mapping);
		if (!cloned_region)
			return nullptr;

		cloned_region->add_node(cloned_func);
		fixup_node_connections(original_region, cloned_region, node_mapping);
		substitute_parameters_with_constants(cloned_region, req.specialized_params);
		target_module.add_function(cloned_func);

		specialization_cache[cache_key] = cloned_func; /* cache it */
		if (!req.call_sites.empty())
			redirect_call_sites(req, req.call_sites, cloned_func);
		return cloned_func;
	}

	std::size_t FunctionSpecializer::redirect_call_sites(const SpecializationRequest &req,
	                                                     const std::vector<Node *> &call_sites,
	                                                     Node *specialized_func)
	{
		if (!specialized_func)
			return 0;

		std::size_t redirected = 0;
		for (Node *call_site: call_sites)
		{
			if (!call_site || call_site->inputs.empty())
				continue;

			if (call_site->ir_type != NodeType::CALL && call_site->ir_type != NodeType::INVOKE)
				continue;

			if (call_site->ir_type == NodeType::CALL)
			{
				std::vector new_inputs = { specialized_func };

				for (std::size_t i = 1; i < call_site->inputs.size(); ++i)
				{
					std::size_t param_idx = i - 1;
					if (!req.is_specialized_parameter(param_idx))
						new_inputs.push_back(call_site->inputs[i]);
				}

				for (Node *old_input: call_site->inputs)
				{
					if (old_input)
						std::erase(old_input->users, call_site);
				}

				call_site->inputs = new_inputs;

				for (Node *new_input: new_inputs)
				{
					if (new_input && std::ranges::find(new_input->users, call_site) == new_input->users.end())
						new_input->users.push_back(call_site);
				}
			}
			else if (call_site->ir_type == NodeType::INVOKE)
			{
				Node *normal_target = call_site->inputs[call_site->inputs.size() - 2];
				Node *exception_target = call_site->inputs[call_site->inputs.size() - 1];

				std::vector new_inputs = { specialized_func };
				for (std::size_t i = 1; i < call_site->inputs.size() - 2; ++i)
				{
					std::size_t param_idx = i - 1;
					if (!req.is_specialized_parameter(param_idx))
						new_inputs.push_back(call_site->inputs[i]);
				}

				new_inputs.push_back(normal_target);
				new_inputs.push_back(exception_target);
				for (Node *old_input: call_site->inputs)
				{
					if (old_input)
						std::erase(old_input->users, call_site);
				}

				call_site->inputs = new_inputs;

				for (Node *new_input: new_inputs)
				{
					if (new_input && std::ranges::find(new_input->users, call_site) == new_input->users.end())
						new_input->users.push_back(call_site);
				}
			}

			redirected++;
		}

		return redirected;
	}

	bool FunctionSpecializer::should_specialize(const SpecializationRequest &req) const
	{
		if (!req.original_function || req.call_sites.empty())
			return false;

		if (req.constant_parameter_count() < min_constant_args)
			return false;

		if (req.call_sites.size() > max_call_sites)
			return false;

		if (req.benefit_score < min_benefit_threshold)
			return false;
		return true;
	}

	double FunctionSpecializer::calculate_benefit_score(const SpecializationRequest &req)
	{
		if (!req.original_function || req.call_sites.empty())
			return 0.0;

		/* simple and stupid heuristic; should work until more complex cases */
		auto score = 0.0;
		score += static_cast<double>(req.constant_parameter_count()) * 2.0;
		score += static_cast<double>(req.call_sites.size()) * 1.5;
		score -= static_cast<double>(req.call_sites.size()) * 0.5;
		return std::max(score, 0.0);
	}

	Node *FunctionSpecializer::clone_function_skeleton(Node *original, Module &target_module, const std::vector<std::pair<std::size_t, LatticeValue>>& specialized_params)
	{
		if (!original || original->ir_type != NodeType::FUNCTION)
			return nullptr;

		Context &ctx = target_module.get_context();
		Node *cloned = ctx.create<Node>();
		cloned->ir_type = original->ir_type;
		cloned->type_kind = original->type_kind;
		cloned->props = original->props;
		cloned->data = original->data;
		cloned->str_id = generate_specialized_name(original, specialized_params, ctx);
		return cloned;
	}

	Region *FunctionSpecializer::clone_region_hierarchy(const Region *original_region, Module &target_module, // NOLINT(*-no-recursion)
	                                                    std::unordered_map<Node *, Node *> &node_mapping)
	{
		if (!original_region)
			return nullptr;

		Node* specialized_func = nullptr;
		for (const auto& [orig, cloned] : node_mapping) {
			if (orig->ir_type == NodeType::FUNCTION) {
				specialized_func = cloned;
				break;
			}
		}

		std::string specialized_name = specialized_func ?
			std::string(target_module.get_context().get_string(specialized_func->str_id)) :
			std::string(original_region->get_name()) + "_specialized";
		Region *cloned_region = target_module.create_region(specialized_name);
		if (!cloned_region)
			return nullptr;

		/* clone all nodes in this region to preserve order */
		const auto &original_nodes = original_region->get_nodes();
		for (std::size_t i = 0; i < original_nodes.size(); ++i)
		{
			Node *original_node = original_nodes[i];
			if (Node *cloned_node = clone_node(original_node, target_module))
			{
				if (i == 0)
					cloned_region->insert_at_beginning(cloned_node);
				else
				{
					/* subsequent nodes go after the previous cloned node */
					if (Node *prev_cloned = node_mapping[original_nodes[i - 1]])
						cloned_region->insert_node_after(prev_cloned, cloned_node);
					else
						cloned_region->add_node(cloned_node); /* fallback */
				}

				node_mapping[original_node] = cloned_node;
			}
		}

		for (const Region *child: original_region->get_children())
		{
			if (Region *cloned_child = clone_region_hierarchy(child, target_module, node_mapping))
				cloned_region->add_child(cloned_child);
		}

		return cloned_region;
	}

	Node *FunctionSpecializer::clone_node(Node *original, Module &target_module)
	{
		if (!original)
			return nullptr;

		Context &ctx = target_module.get_context();
		Node *cloned = ctx.create<Node>();
		cloned->ir_type = original->ir_type;
		cloned->type_kind = original->type_kind;
		cloned->props = original->props;
		cloned->data = original->data;
		cloned->str_id = original->str_id;
		/* note: inputs and users will be fixed up later in fixup_node_connections */
		return cloned;
	}

	template<typename T>
	Node *find_or_create_literal(Module &module, T value)
	{
		/* search for existing literal with same value in the root region */
		Region *root = module.get_root_region();
		for (Node *node : root->get_nodes())
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

		/* no existing literal found, create a new one */
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

	void FunctionSpecializer::substitute_parameters_with_constants(Region *cloned_region,
                                                               const std::vector<std::pair<std::size_t, LatticeValue>> &specialized_params)
	{
	    if (!cloned_region || specialized_params.empty())
	        return;

	    std::vector<Node *> param_nodes;
	    for (Node *node: cloned_region->get_nodes())
	    {
	        if (node && node->ir_type == NodeType::PARAM)
	            param_nodes.push_back(node);
	    }

	    /* sort parameters by declaration order */
	    std::ranges::sort(param_nodes,
	                      [&cloned_region](const Node *a, const Node *b)
	                      {
	                          const auto &nodes = cloned_region->get_nodes();
	                          const auto pos_a = std::ranges::find(nodes, a);
	                          const auto pos_b = std::ranges::find(nodes, b);
	                          return pos_a < pos_b;
	                      });

	    for (const auto &[param_idx, constant_val]: specialized_params)
	    {
	        if (param_idx >= param_nodes.size() || !constant_val.is_constant())
	            continue;

	        Node *param = param_nodes[param_idx];
	        Module &module = cloned_region->get_module();
	    	Node *literal = nullptr;
	        switch (param->type_kind)
	        {
	            case DataType::INT8:
	                literal = find_or_create_literal<std::int8_t>(module, constant_val.value.get<DataType::INT8>());
	                break;
	            case DataType::INT16:
	                literal = find_or_create_literal<std::int16_t>(module, constant_val.value.get<DataType::INT16>());
	                break;
	            case DataType::INT32:
	                literal = find_or_create_literal<std::int32_t>(module, constant_val.value.get<DataType::INT32>());
	                break;
	            case DataType::INT64:
	                literal = find_or_create_literal<std::int64_t>(module, constant_val.value.get<DataType::INT64>());
	                break;
	            case DataType::UINT8:
	                literal = find_or_create_literal<std::uint8_t>(module, constant_val.value.get<DataType::UINT8>());
	                break;
	            case DataType::UINT16:
	                literal = find_or_create_literal<std::uint16_t>(module, constant_val.value.get<DataType::UINT16>());
	                break;
	            case DataType::UINT32:
	                literal = find_or_create_literal<std::uint32_t>(module, constant_val.value.get<DataType::UINT32>());
	                break;
	            case DataType::UINT64:
	                literal = find_or_create_literal<std::uint64_t>(module, constant_val.value.get<DataType::UINT64>());
	                break;
	            case DataType::FLOAT32:
	                literal = find_or_create_literal<float>(module, constant_val.value.get<DataType::FLOAT32>());
	                break;
	            case DataType::FLOAT64:
	                literal = find_or_create_literal<double>(module, constant_val.value.get<DataType::FLOAT64>());
	                break;
	            case DataType::BOOL:
	                literal = find_or_create_literal<bool>(module, constant_val.value.get<DataType::BOOL>());
	                break;
	            default:
	                continue; /* unsupported type for specialization */
	        }
    		cloned_region->replace_node(param, literal, true);
	    }
	}

	void FunctionSpecializer::fixup_node_connections(const Region *original_region, Region *cloned_region, // NOLINT(*-no-recursion)
	                                                 const std::unordered_map<Node *, Node *> &node_mapping)
	{
		if (!original_region || !cloned_region)
			return;

		for (Node *original_node: original_region->get_nodes())
		{
			auto cloned_it = node_mapping.find(original_node);
			if (cloned_it == node_mapping.end())
				continue;

			Node *cloned_node = cloned_it->second;

			cloned_node->inputs.clear();
			cloned_node->users.clear();
			for (Node *original_input: original_node->inputs)
			{
				if (auto input_it = node_mapping.find(original_input);
					input_it != node_mapping.end())
				{
					Node *cloned_input = input_it->second;
					cloned_node->inputs.push_back(cloned_input);
					cloned_input->users.push_back(cloned_node);
				}
			}
		}

		const auto& original_children = original_region->get_children();
		const auto& cloned_children = cloned_region->get_children();
		for (std::size_t i = 0; i < std::min(original_children.size(), cloned_children.size()); ++i)
			fixup_node_connections(original_children[i], cloned_children[i], node_mapping);
	}

	StringTable::StringId FunctionSpecializer::generate_specialized_name(Node *original_func,
	                                                                     const std::vector<std::pair<std::size_t,
		                                                                     LatticeValue> > &specialized_params,
	                                                                     Context &ctx)
	{
		if (!original_func)
			return {};

		std::uint64_t spec_hash = compute_specialization_key(original_func, specialized_params);

		std::ostringstream name_stream;
		name_stream << "spec_" << std::hex << spec_hash;

		return ctx.intern_string(name_stream.str());
	}

	std::size_t FunctionSpecializer::estimate_function_size(Node *func, const std::vector<Module *> &modules)
	{
		const Region *func_region = find_function_region(func, modules);
		if (!func_region)
			return 0;

		std::size_t total_nodes = 0;
		std::function<void(const Region *)> count_nodes = [&](const Region *region)
		{
			if (!region)
				return;

			total_nodes += region->get_nodes().size();
			for (const Region *child: region->get_children())
				count_nodes(child);
		};

		count_nodes(func_region);
		return total_nodes;
	}

	Region *FunctionSpecializer::find_function_region(Node *func, const std::vector<Module *> &modules)
	{
		if (!func || func->ir_type != NodeType::FUNCTION)
			return nullptr;

		for (const Module *module : modules)
		{
			for (const Region *child : module->get_root_region()->get_children())
			{
				for (const Node* node : child->get_nodes())
				{
					if (node == func)
						return const_cast<Region *>(child);
				}
			}
		}

		return nullptr;
	}

	std::uint64_t FunctionSpecializer::compute_specialization_key(Node* func,
							const std::vector<std::pair<std::size_t, LatticeValue>>& specialized_params)
	{
	    std::uint64_t key = std::hash<void*>{}(func);
		for (const auto& [param_idx, constant_val] : specialized_params)
		{
	        if (constant_val.is_constant())
	        {
	            key ^= std::hash<std::size_t>{}(param_idx) << 1;
	        	switch (constant_val.value.type())
	            {
	                case DataType::INT8:
	                    key ^= std::hash<std::int8_t>{}(constant_val.value.get<DataType::INT8>()) << 2;
	                    break;
	                case DataType::INT16:
	                    key ^= std::hash<std::int16_t>{}(constant_val.value.get<DataType::INT16>()) << 2;
	                    break;
	                case DataType::INT32:
	                    key ^= std::hash<std::int32_t>{}(constant_val.value.get<DataType::INT32>()) << 2;
	                    break;
	                case DataType::INT64:
	                    key ^= std::hash<std::int64_t>{}(constant_val.value.get<DataType::INT64>()) << 2;
	                    break;
	                case DataType::UINT8:
	                    key ^= std::hash<std::uint8_t>{}(constant_val.value.get<DataType::UINT8>()) << 2;
	                    break;
	                case DataType::UINT16:
	                    key ^= std::hash<std::uint16_t>{}(constant_val.value.get<DataType::UINT16>()) << 2;
	                    break;
	                case DataType::UINT32:
	                    key ^= std::hash<std::uint32_t>{}(constant_val.value.get<DataType::UINT32>()) << 2;
	                    break;
	                case DataType::UINT64:
	                    key ^= std::hash<std::uint64_t>{}(constant_val.value.get<DataType::UINT64>()) << 2;
	                    break;
	                case DataType::FLOAT32:
	                {
	                    float val = constant_val.value.get<DataType::FLOAT32>();
	                    std::uint32_t bits;
	                    std::memcpy(&bits, &val, sizeof(bits));
	                    key ^= std::hash<std::uint32_t>{}(bits) << 2;
	                    break;
	                }
	                case DataType::FLOAT64:
	                {
	                    double val = constant_val.value.get<DataType::FLOAT64>();
	                    std::uint64_t bits;
	                    std::memcpy(&bits, &val, sizeof(bits));
	                    key ^= std::hash<std::uint64_t>{}(bits) << 2;
	                    break;
	                }
	                case DataType::BOOL:
	                    key ^= std::hash<bool>{}(constant_val.value.get<DataType::BOOL>()) << 2;
	                    break;
	                default:
	                    key ^= std::hash<int>{}(static_cast<int>(constant_val.value.type())) << 2;
	                    break;
	            }
	        }
	    }
	    return key;
	}
}
