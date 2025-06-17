/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <functional>
#include <bloom/foundation/region.hpp>
#include <bloom/ipo/inlining.hpp>

namespace blm
{
	bool IPOInliningPass::run(std::vector<Module *> &modules, IPOPassContext &context)
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

		auto candidates = find_candidates(cg_result->get_call_graph(), modules);
		std::ranges::sort(candidates, [](const InlineCandidate &a, const InlineCandidate &b)
		{
			return a.benefit_score > b.benefit_score;
		});

		std::size_t total_optimized = 0;
		for (const auto &candidate: candidates)
		{
			if (!should_optimize(candidate))
				continue;

			/* try specialization first for constant args for SCCP in the next pass */
			Node *target_function = candidate.callee_function;
			if (candidate.has_constant_args && enable_specialization)
			{
				if (Node *specialized = try_specialize(candidate))
				{
					target_function = specialized;
					total_optimized++;
					if (candidate.function_size <= max_inline_size)
					{
						InlineCandidate inline_candidate = candidate;
						inline_candidate.callee_function = target_function;
						try_inline(inline_candidate);
					}
				}
				else if (candidate.function_size <= max_inline_size)
				{
					if (try_inline(candidate))
						total_optimized++;
				}
			}

			/* then try inlining the target function */
			if (candidate.function_size <= max_inline_size)
			{
				InlineCandidate inline_candidate = candidate;
				inline_candidate.callee_function = target_function;
				if (try_inline(inline_candidate))
					total_optimized++;
			}
		}

		context.update_stat("ipo_inlining.optimized_calls", total_optimized);
		return total_optimized > 0;
	}

	std::vector<IPOInliningPass::InlineCandidate> IPOInliningPass::find_candidates(
		const CallGraph &call_graph, std::vector<Module *> &modules)
	{
		std::vector<InlineCandidate> candidates;

		for (const CallGraphNode *cg_node: call_graph.get_nodes())
		{
			Node *caller = cg_node->get_function();
			if (!caller || caller->ir_type != NodeType::FUNCTION)
				continue;

			Module *caller_module = find_module_for_function(caller, modules);
			if (!caller_module)
				continue;

			for (const CallGraphNode *callee_node: cg_node->get_callees())
			{
				Node *callee = callee_node->get_function();
				if (!callee || callee->ir_type != NodeType::FUNCTION)
					continue;

				Module *callee_module = find_module_for_function(callee, modules);
				if (!callee_module)
					continue;

				for (Node *call_site: cg_node->get_call_sites())
				{
					if (!call_site || call_site->inputs.empty() || call_site->inputs[0] != callee)
						continue;

					InlineCandidate candidate;
					candidate.call_site = call_site;
					candidate.callee_function = callee;
					candidate.caller_module = caller_module;
					candidate.callee_module = callee_module;
					candidate.function_size = estimate_function_size(callee, modules);
					candidate.has_constant_args = has_constant_arguments(call_site);
					candidate.benefit_score = calculate_benefit(candidate);

					candidates.push_back(candidate);
				}
			}
		}

		return candidates;
	}

	bool IPOInliningPass::should_optimize(const InlineCandidate &candidate) const
	{
		if (!candidate.callee_function || !candidate.call_site)
			return false;

		if (candidate.benefit_score < min_benefit_threshold)
			return false;

		if (is_recursive_call(candidate))
			return false;

		std::vector modules = { candidate.caller_module, candidate.callee_module };
		const Region* fnr = find_function_region(candidate.callee_function, modules);
		if (!fnr)
			return false;

		if (!fnr->get_children().empty())
			return false;

		for (const Node *node : fnr->get_nodes())
		{
			/* invoke is already handled separately but just to be clear */
			switch (node->ir_type)
			{
				case NodeType::BRANCH:
				case NodeType::JUMP:
				case NodeType::INVOKE:
					return true;
				default:
					break;
			}
		}

		/* size doesn't matter as much for specialization */
		if (candidate.has_constant_args && enable_specialization)
			return true;

		/* keep functions small for inlining */
		return candidate.function_size <= max_inline_size;
	}

	bool IPOInliningPass::is_recursive_call(const InlineCandidate &candidate)
	{
		Region *current = candidate.call_site->parent_region;
		std::string_view callee_name = candidate.caller_module->get_context().get_string(
			candidate.callee_function->str_id);

		while (current)
		{
			if (current->get_name() == callee_name)
				return true;
			current = current->get_parent();
		}
		return false;
	}

	Node *IPOInliningPass::try_specialize(const InlineCandidate &candidate)
	{
		if (!enable_specialization || !candidate.has_constant_args)
			return nullptr;

		/* build specialization request */
		std::vector<std::pair<std::size_t, LatticeValue> > specialized_params;

		std::size_t arg_start = 1;
		std::size_t arg_end = candidate.call_site->inputs.size();
		if (candidate.call_site->ir_type == NodeType::INVOKE)
			arg_end -= 2;

		for (std::size_t i = arg_start; i < arg_end; ++i)
		{
			Node *arg = candidate.call_site->inputs[i];
			if (arg && arg->ir_type == NodeType::LIT)
			{
				LatticeValue constant_val;
				constant_val.state = LatticeValue::State::CONSTANT;
				constant_val.value = arg->data;
				specialized_params.emplace_back(i - 1, constant_val);
			}
		}

		if (specialized_params.empty())
			return nullptr;

		FunctionSpecializer::SpecializationRequest req;
		req.original_function = candidate.callee_function;
		req.specialized_params = specialized_params;
		req.call_sites = { candidate.call_site };
		req.benefit_score = static_cast<double>(specialized_params.size() * 2);

		/* specializer creates specialized function and redirects call */
		return specializer.specialize_function(req, *candidate.caller_module);
	}

	bool IPOInliningPass::try_inline(const InlineCandidate &candidate)
	{
		if (candidate.call_site->ir_type == NodeType::INVOKE)
			return false;

		if (candidate.function_size > max_inline_size)
			return false;

		std::unordered_map<Node *, Node *> node_mapping;
		std::vector<Module*> all_modules = { candidate.caller_module, candidate.callee_module };
		Region *inlined_region = clone_function_body(candidate.callee_function,
		                                             *candidate.caller_module,
		                                             all_modules
		                                             ,node_mapping);

		if (!inlined_region)
			return false;

		Node *return_value = extract_return_value(inlined_region);
		substitute_parameters(inlined_region, candidate.call_site);
		replace_call_with_body(candidate.call_site, inlined_region, return_value);
		return true;
	}

	Node *IPOInliningPass::extract_return_value(Region *inlined_region)
	{
		if (!inlined_region)
			return nullptr;

		for (Node *node: inlined_region->get_nodes())
		{
			if (node->ir_type == NodeType::RET && !node->inputs.empty())
				return node->inputs[0];
		}
		return nullptr;
	}

	std::size_t IPOInliningPass::calculate_benefit(const InlineCandidate &candidate)
	{
		std::size_t benefit = 2;
		if (candidate.has_constant_args)
			benefit += 5; /* SCCP possible */
		if (candidate.caller_module != candidate.callee_module)
			benefit += 2; /* cross-module optimization */
		if (candidate.function_size <= 5)
			benefit += 3; /* small functions are great candidates */
		/* diminishing returns for larger functions */
		if (candidate.function_size > 10)
			benefit = std::max(benefit - 2, static_cast<std::size_t>(1));

		return benefit;
	}

	std::size_t IPOInliningPass::estimate_function_size(Node *function, std::vector<Module *> &modules)
	{
		const Region *func_region = find_function_region(function, modules);
		if (!func_region)
			return 1000; /* unknown function = very expensive */

		std::size_t node_count = 0;
		std::function<void(const Region *)> count_nodes = [&](const Region *region)
		{
			if (!region)
				return;

			for (const Node *node: region->get_nodes())
			{
				/* don't count parameters and entry/exit */
				if (node->ir_type != NodeType::PARAM &&
				    node->ir_type != NodeType::ENTRY &&
				    node->ir_type != NodeType::EXIT)
				{
					node_count++;
				}
			}

			for (const Region *child: region->get_children())
				count_nodes(child);
		};

		count_nodes(func_region);
		return node_count;
	}

	Module *IPOInliningPass::find_module_for_function(Node *function, std::vector<Module *> &modules)
	{
		for (Module *mod: modules)
		{
			for (const Node *func: mod->get_functions())
			{
				if (func == function)
					return mod;
			}
		}
		return nullptr;
	}

	bool IPOInliningPass::has_constant_arguments(Node *call_site)
	{
		if (!call_site || call_site->inputs.size() <= 1)
			return false;

		std::size_t arg_start = 1;
		std::size_t arg_end = call_site->inputs.size();
		if (call_site->ir_type == NodeType::INVOKE)
			arg_end -= 2;

		for (std::size_t i = arg_start; i < arg_end; ++i)
		{
			if (call_site->inputs[i] && call_site->inputs[i]->ir_type == NodeType::LIT)
				return true;
		}
		return false;
	}

	Region *IPOInliningPass::find_function_region(Node *function, std::vector<Module *> &modules)
	{
		const Module* target_module = find_module_for_function(function, modules);
		if (!target_module)
			return nullptr;

		for (const Region* child : target_module->get_root_region()->get_children()) {
			for (Node* node : child->get_nodes())
			{
				if (node == function)
					return const_cast<Region*>(child);
			}
		}

		return nullptr;
	}

	/* note: this does NOT clone regions yet due to the complexity of the IR that comes with it */
	Region *IPOInliningPass::clone_function_body(Node *function, Module &target_module,
													std::vector<Module*>& all_modules,
	                                             std::unordered_map<Node *, Node *> &node_mapping)
	{
		const Region *original_region = find_function_region(function, all_modules);
		if (!original_region)
			return nullptr;

		const std::string inline_name = std::string(original_region->get_name()) + "_inlined";
		Region *cloned_region = target_module.create_region(inline_name);
		if (!cloned_region)
			return nullptr;

		/* clone all nodes except ENTRY/EXIT */
		for (Node *original_node: original_region->get_nodes())
		{
			if (original_node->ir_type == NodeType::ENTRY || original_node->ir_type == NodeType::EXIT)
				continue;

			if (Node *cloned_node = clone_node(original_node, target_module))
			{
				cloned_region->add_node(cloned_node);
				node_mapping[original_node] = cloned_node;
			}
		}

		fixup_connections(original_region, cloned_region, node_mapping);
		return cloned_region;
	}

	Node *IPOInliningPass::clone_node(Node *original, Module &target_module)
	{
		if (!original)
			return nullptr;

		Context &ctx = target_module.get_context();

		/* try to reuse existing ones for literals */
		if (original->ir_type == NodeType::LIT)
		{
			Region *root_region = target_module.get_root_region();
			for (Node *existing: root_region->get_nodes())
			{
				if (existing->ir_type != NodeType::LIT || existing->type_kind != original->type_kind)
					continue;

				auto data_matches = false;
				switch (original->type_kind)
				{
					case DataType::BOOL:
						data_matches = (existing->as<DataType::BOOL>() == original->as<DataType::BOOL>());
						break;
					case DataType::INT8:
						data_matches = (existing->as<DataType::INT8>() == original->as<DataType::INT8>());
						break;
					case DataType::INT16:
						data_matches = (existing->as<DataType::INT16>() == original->as<DataType::INT16>());
						break;
					case DataType::INT32:
						data_matches = (existing->as<DataType::INT32>() == original->as<DataType::INT32>());
						break;
					case DataType::INT64:
						data_matches = (existing->as<DataType::INT64>() == original->as<DataType::INT64>());
						break;
					case DataType::UINT8:
						data_matches = (existing->as<DataType::UINT8>() == original->as<DataType::UINT8>());
						break;
					case DataType::UINT16:
						data_matches = (existing->as<DataType::UINT16>() == original->as<DataType::UINT16>());
						break;
					case DataType::UINT32:
						data_matches = (existing->as<DataType::UINT32>() == original->as<DataType::UINT32>());
						break;
					case DataType::UINT64:
						data_matches = (existing->as<DataType::UINT64>() == original->as<DataType::UINT64>());
						break;
					case DataType::FLOAT32:
						data_matches = (existing->as<DataType::FLOAT32>() == original->as<DataType::FLOAT32>());
						break;
					case DataType::FLOAT64:
						data_matches = (existing->as<DataType::FLOAT64>() == original->as<DataType::FLOAT64>());
						break;
					default:
						break;
				}

				if (data_matches)
					return existing;
			}
		}

		/* create new node */
		Node *cloned = ctx.create<Node>();
		cloned->ir_type = original->ir_type;
		cloned->type_kind = original->type_kind;
		cloned->props = original->props;
		cloned->data = original->data;
		cloned->str_id = original->str_id;

		return cloned;
	}

	void IPOInliningPass::fixup_connections(const Region *original, Region *cloned,
	                                        const std::unordered_map<Node *, Node *> &mapping)
	{
		if (!original || !cloned)
			return;

		for (Node *original_node: original->get_nodes())
		{
			auto cloned_it = mapping.find(original_node);
			if (cloned_it == mapping.end())
				continue;

			Node *cloned_node = cloned_it->second;
			cloned_node->inputs.clear();
			cloned_node->users.clear();

			for (Node *original_input: original_node->inputs)
			{
				if (auto input_it = mapping.find(original_input);
					input_it != mapping.end())
				{
					Node *cloned_input = input_it->second;
					cloned_node->inputs.push_back(cloned_input);
					cloned_input->users.push_back(cloned_node);
				}
			}
		}
	}

	void IPOInliningPass::substitute_parameters(Region *inlined_region, Node *call_site)
	{
		if (!inlined_region || !call_site)
			return;

		std::vector<Node *> param_nodes;
		for (Node *node: inlined_region->get_nodes())
		{
			if (node->ir_type == NodeType::PARAM)
				param_nodes.push_back(node);
		}

		/* sort by position in region */
		std::ranges::sort(param_nodes, [inlined_region](const Node *a, const Node *b)
		{
			const auto &nodes = inlined_region->get_nodes();
			auto pos_a = std::ranges::find(nodes, a);
			auto pos_b = std::ranges::find(nodes, b);
			return pos_a < pos_b;
		});

		std::size_t arg_end = call_site->inputs.size();
		if (call_site->ir_type == NodeType::INVOKE)
			arg_end -= 2;

		for (std::size_t i = 0; i < param_nodes.size() && (i + 1) < arg_end; ++i)
		{
			Node *param = param_nodes[i];
			Node *arg = call_site->inputs[i + 1];
			if (!arg)
				continue;

			/* replace all uses of the parameter with the argument */
			for (Node *user: param->users)
			{
				for (Node *&input: user->inputs)
				{
					if (input == param)
					{
						input = arg;
						if (std::ranges::find(arg->users, user) == arg->users.end())
							arg->users.push_back(user);
					}
				}
			}

			/* remove the parameter node */
			inlined_region->remove_node(param);
		}
	}

	void IPOInliningPass::replace_call_with_body(Node *call_site, Region *inlined_region, Node *return_value)
	{
		if (!call_site || !inlined_region)
			return;

		Region *caller_region = call_site->parent_region;
		if (!caller_region)
			return;

		auto &caller_nodes = const_cast<std::vector<Node *> &>(caller_region->get_nodes());
		const auto call_pos = std::ranges::find(caller_nodes, call_site);
		if (call_pos == caller_nodes.end())
			return;

		std::vector<Node *> nodes_to_inline;
		for (Node *node: inlined_region->get_nodes())
		{
			if (node->ir_type != NodeType::ENTRY &&
			    node->ir_type != NodeType::EXIT &&
			    node->ir_type != NodeType::PARAM &&
			    node->ir_type != NodeType::RET)
			{
				node->parent_region = caller_region;
				nodes_to_inline.push_back(node);
			}
		}

		/* update users of the call site to use the return value */
		if (return_value && !call_site->users.empty())
		{
			for (Node *user: call_site->users)
			{
				for (Node *&input: user->inputs)
				{
					if (input == call_site)
					{
						input = return_value;
						return_value->users.push_back(user);
					}
				}
			}
		}

		/* remove the call site and insert inlined nodes and then
		 * clear call site connections */
		caller_nodes.erase(call_pos);
		caller_nodes.insert(call_pos, nodes_to_inline.begin(), nodes_to_inline.end());
		for (Node *input: call_site->inputs)
		{
			if (input)
			{
				auto &users = input->users;
				std::erase(users, call_site);
			}
		}
		call_site->inputs.clear();
		call_site->users.clear();
	}
}
