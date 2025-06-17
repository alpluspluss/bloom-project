/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/support/relation.hpp>

namespace blm
{
	bool LocalAliasResult::invalidated_by(const std::type_info &) const
	{
		return true;
	}

	void LocalAliasResult::add_location(Node *ptr, const MemoryLocation loc)
	{
		memory_locations[ptr] = loc;
	}

	const MemoryLocation *LocalAliasResult::get_location(Node *ptr) const
	{
		const auto it = memory_locations.find(ptr);
		if (it != memory_locations.end())
			return &it->second;
		return nullptr;
	}

	void LocalAliasResult::mark_escaped(Node *ptr)
	{
		escaped_pointers.insert(ptr);
	}

	bool LocalAliasResult::has_escaped(Node *ptr) const
	{
		return escaped_pointers.contains(ptr);
	}

	void LocalAliasResult::add_pointer_copy(Node *dest, Node *src)
	{
		pointer_copies[dest] = src;
	}

	Node *LocalAliasResult::get_pointer_source(Node *ptr) const
	{
		/* we try to follow the chain of pointer copies to find the ultimate source */
		Node *current = ptr;
		std::unordered_set<Node *> visited;
		while (current && !visited.contains(current))
		{
			visited.insert(current);
			if (auto it = pointer_copies.find(current);
				it != pointer_copies.end())
				current = it->second;
			else
				break;
		}

		return current;
	}

	void LocalAliasResult::add_store_operation(Node *store)
	{
		all_stores.insert(store);
	}

	void LocalAliasResult::add_load_operation(Node *load)
	{
		all_loads.insert(load);
	}

	void LocalAliasResult::add_store_load_relation(Node *store, Node *load)
	{
		store_to_loads[store].insert(load);
		load_to_stores[load].insert(store);
	}

	std::vector<Node *> LocalAliasResult::get_affecting_stores(Node *load) const
	{
		std::vector<Node*> stores;
		if (const auto it = load_to_stores.find(load); it != load_to_stores.end())
		{
			stores.reserve(it->second.size());
			for (Node* store : it->second)
				stores.push_back(store);
		}
		return stores;
	}

	std::vector<Node *> LocalAliasResult::get_affected_loads(Node *store) const
	{
		std::vector<Node*> loads;
		if (auto it = store_to_loads.find(store); it != store_to_loads.end())
		{
			loads.reserve(it->second.size());
			for (Node* load : it->second)
				loads.push_back(load);
		}
		return loads;
	}

	bool LocalAliasResult::maybe_modified_by(Node *load, Node *store) const
	{
		if (auto it = load_to_stores.find(load); it != load_to_stores.end())
			return it->second.contains(store);
		return false;
	}

	AliasResult LocalAliasResult::alias(Node *a, Node *b) const
	{
		/* quick check for identity */
		if (a == b)
			return AliasResult::MUST_ALIAS;

		/* follow pointer copies to get ultimate sources */
		Node *ultimate_a = get_pointer_source(a);
		Node *ultimate_b = get_pointer_source(b);
		if (ultimate_a == ultimate_b)
			return AliasResult::MUST_ALIAS;

		if (has_escaped(ultimate_a) || has_escaped(ultimate_b))
			return AliasResult::MAY_ALIAS;

		const auto *loc_a = get_location(ultimate_a);
		const auto *loc_b = get_location(ultimate_b);
		if (!loc_a || !loc_b)
			return AliasResult::MAY_ALIAS;

		if (loc_a->base != loc_b->base)
			return AliasResult::NO_ALIAS;

		if (loc_a->offset != -1 && loc_b->offset != -1)
		{
			const auto a_end = loc_a->offset + static_cast<int64_t>(loc_a->size);
			const auto b_end = loc_b->offset + static_cast<int64_t>(loc_b->size);
			if (loc_a->offset >= b_end || loc_b->offset >= a_end)
				return AliasResult::NO_ALIAS;
			if (loc_a->offset == loc_b->offset && loc_a->size == loc_b->size)
				return AliasResult::MUST_ALIAS;
			return AliasResult::PARTIAL_ALIAS;
		}
		return AliasResult::MAY_ALIAS;
	}

	void LocalAliasResult::add_allocation_site(Node *node, uint64_t size)
	{
		allocation_sites.insert(node);
		MemoryLocation loc;
		loc.base = node;
		loc.offset = 0;
		loc.size = size;
		add_location(node, loc);
	}

	bool LocalAliasResult::is_allocation_site(Node *node) const
	{
		return allocation_sites.contains(node);
	}

	std::unique_ptr<AnalysisResult> LocalAliasAnalysisPass::analyze(Module &module, PassContext &)
	{
		auto result = std::make_unique<LocalAliasResult>();
		for (Node *func: module.get_functions())
			analyze_function(*result, func, module);

		perform_escape_analysis(*result, module);
		analyze_store_load_relations(*result);
		return result;
	}

	void LocalAliasAnalysisPass::handle_store(LocalAliasResult &result, Node *node)
	{
		if (node->inputs.size() < 2)
			return;

		Node* stored_value = node->inputs[0];
		Node* store_address = node->inputs[1];
		if (stored_value->type_kind == DataType::POINTER)
		{
			if (Node* source = result.get_pointer_source(stored_value);
				!result.has_escaped(source))
			{
				result.mark_escaped(source);
			}
		}

		for (Node* load : result.get_all_loads())
		{
			if (load->inputs.empty())
				continue;
			if (Node* load_address = load->inputs[0];
				addresses_may_alias(result, store_address, load_address))
			{
				result.add_store_load_relation(node, load);
			}
		}
	}

	void LocalAliasAnalysisPass::analyze_store_load_relations(LocalAliasResult &result)
	{
		for (Node* store : result.get_all_stores())
		{
			if (store->inputs.size() < 2)
				continue;

			Node* store_address = store->inputs[1];
			for (Node* load : result.get_all_loads())
			{
				if (load->inputs.empty())
					continue;

				Node* load_address = load->inputs[0];
				if (addresses_may_alias(result, store_address, load_address))
				{
					result.add_store_load_relation(store, load);
				}
			}
		}
	}

	bool LocalAliasAnalysisPass::addresses_may_alias(LocalAliasResult &result, Node *addr1, Node *addr2) const
	{
		if (addr1 == addr2)
			return true;
		const AliasResult alias_result = result.alias(addr1, addr2);
		return alias_result != AliasResult::NO_ALIAS;
	}

	void LocalAliasAnalysisPass::analyze_function(LocalAliasResult &result, Node *func, Module &module)
	{
		if (func->ir_type != NodeType::FUNCTION)
			return;

		for (const Region *child: module.get_root_region()->get_children())
		{
			if (child->get_name() == module.get_context().get_string(func->str_id))
			{
				analyze_region(result, child);
				break;
			}
		}
	}

	void LocalAliasAnalysisPass::analyze_region(LocalAliasResult &result, const Region *region)
	{
		if (!region)
			return;

		for (Node *node: region->get_nodes())
			analyze_node(result, node);
		for (const Region *child: region->get_children())
			analyze_region(result, child);
	}

	void LocalAliasAnalysisPass::analyze_node(LocalAliasResult &result, Node *node)
	{
		if (node->parent_region && is_global_scope(node->parent_region))
		{
			result.mark_escaped(node);
			return;
		}

		switch (node->ir_type)
		{
			case NodeType::HEAP_ALLOC:
			case NodeType::STACK_ALLOC:
				handle_allocation(result, node);
				break;

			case NodeType::PTR_ADD:
				handle_pointer_arithmetic(result, node);
				break;

			case NodeType::ADDR_OF:
				handle_address_of(result, node);
				break;

			case NodeType::PARAM:
				handle_parameter(result, node);
				break;

			case NodeType::LOAD:
			case NodeType::PTR_LOAD:
			case NodeType::ATOMIC_LOAD:
				handle_load(result, node);
				result.add_load_operation(node);
				break;

			case NodeType::STORE:
			case NodeType::PTR_STORE:
			case NodeType::ATOMIC_STORE:
				handle_store(result, node);
				result.add_store_operation(node);
				break;

			case NodeType::CALL:
			case NodeType::INVOKE:
				handle_function_call(result, node);
				break;

			case NodeType::RET:
				handle_return(result, node);
				break;

			case NodeType::REINTERPRET_CAST:
				handle_cast(result, node);
				break;

			default:
				if (node->type_kind == DataType::POINTER && !node->inputs.empty())
					result.add_pointer_copy(node, node->inputs[0]);

				break;
		}
	}

	void LocalAliasAnalysisPass::handle_allocation(LocalAliasResult &result, Node *node) const
	{
		std::uint64_t size = 0;
		if (node->ir_type == NodeType::HEAP_ALLOC)
		{
			if (node->inputs.size() >= 2)
			{
				if (Node *size_node = node->inputs[1];
					size_node->ir_type == NodeType::LIT)
				{
					size = extract_integer_literal(size_node);
				}
			}
		}
		else if (node->ir_type == NodeType::STACK_ALLOC)
		{
			if (!node->inputs.empty())
			{
				if (Node *size_node = node->inputs[0];
					size_node->ir_type == NodeType::LIT)
				{
					size = extract_integer_literal(size_node);
				}
			}
		}

		result.add_allocation_site(node, size);
	}

	void LocalAliasAnalysisPass::handle_pointer_arithmetic(LocalAliasResult &result, Node *node)
	{
		if (node->inputs.size() < 2)
			return;

		Node *base_ptr = nullptr;
		const std::int64_t offset = compute_pointer_offset(node, base_ptr);

		if (base_ptr)
		{
			/* record the memory location for this pointer */
			MemoryLocation loc;
			loc.base = result.get_pointer_source(base_ptr); /* follow copies */
			loc.offset = offset;
			loc.size = get_access_size(node);

			result.add_location(node, loc);
		}
	}

	void LocalAliasAnalysisPass::handle_address_of(LocalAliasResult &result, Node *node) const
	{
		if (node->inputs.empty())
			return;

		Node *var = node->inputs[0];

		/* create a memory location */
		MemoryLocation loc;
		loc.base = var;
		loc.offset = 0;
		loc.size = get_access_size(var);

		result.add_location(node, loc);
	}

	void LocalAliasAnalysisPass::handle_parameter(LocalAliasResult &result, Node *node) const
	{
		if (node->type_kind == DataType::POINTER)
		{
			if (node->parent_region && is_global_scope(node->parent_region))
			{
				result.mark_escaped(node);
				return;
			}

			MemoryLocation loc;
			loc.base = node;
			loc.offset = 0;
			loc.size = 0;

			result.add_location(node, loc);
			result.mark_escaped(node); /* parameters always escape */
		}
	}

	void LocalAliasAnalysisPass::handle_load(LocalAliasResult &result, Node *node) const
	{
		if (node->type_kind == DataType::POINTER)
			/* this is a load of a pointer value; mark it as potentially
			 * escaped since it came from memory, and it is unknown what is it pointed to */
			result.mark_escaped(node);
	}

	void LocalAliasAnalysisPass::handle_function_call(LocalAliasResult &result, const Node *node) const
	{
		for (std::size_t i = 1; i < node->inputs.size(); ++i)
		{
			if (Node *arg = node->inputs[i];
				is_pointer_type(arg->type_kind))
			{
				Node *ultimate_source = result.get_pointer_source(arg);
				result.mark_escaped(ultimate_source);

				if (const MemoryLocation *loc = result.get_location(ultimate_source))
					result.mark_escaped(loc->base);

				if (ultimate_source->ir_type == NodeType::ADDR_OF && !ultimate_source->inputs.empty())
				{
					Node *pointed_to_object = ultimate_source->inputs[0];
					result.mark_escaped(pointed_to_object);
					if (const MemoryLocation *pointed_loc = result.get_location(pointed_to_object))
						result.mark_escaped(pointed_loc->base);
				}
			}
		}
	}

	void LocalAliasAnalysisPass::handle_return(LocalAliasResult &result, const Node *node) const
	{
		if (!node->inputs.empty())
		{
			Node *ret_val = node->inputs[0];
			if (ret_val->type_kind == DataType::POINTER)
			{
				Node *ultimate_source = result.get_pointer_source(ret_val);
				result.mark_escaped(ultimate_source);
				if (const MemoryLocation *loc = result.get_location(ultimate_source))
					result.mark_escaped(loc->base);
			}
		}
	}

	void LocalAliasAnalysisPass::handle_cast(LocalAliasResult &result, Node *node) const
	{
		/* pointer casts are essentially copies */
		if (node->type_kind == DataType::POINTER && !node->inputs.empty())
		{
			if (node->inputs[0]->type_kind == DataType::POINTER)
				result.add_pointer_copy(node, node->inputs[0]);
		}
	}

	void LocalAliasAnalysisPass::perform_escape_analysis(LocalAliasResult &result, Module &module)
	{
		auto changed = true;
		while (changed)
		{
			changed = false;

			for (Node *func: module.get_functions())
			{
				if (func->ir_type != NodeType::FUNCTION)
					continue;

				for (const Region *child: module.get_root_region()->get_children())
				{
					if (child->get_name() == module.get_context().get_string(func->str_id))
					{
						if (propagate_escapes_in_region(result, child))
							changed = true;
						break;
					}
				}
			}
		}
	}

	bool LocalAliasAnalysisPass::propagate_escapes_in_region(LocalAliasResult &result, const Region *region)
	{
		if (!region)
			return false;

		auto changed = false;
		for (Node *node: region->get_nodes())
		{
			/* if a pointer escapes, any pointer derived from it also escapes */
			switch (node->ir_type)
			{
				case NodeType::PTR_ADD:
				case NodeType::REINTERPRET_CAST:
					if (node->type_kind == DataType::POINTER && !node->inputs.empty())
					{
						Node *source = result.get_pointer_source(node->inputs[0]);
						if (result.has_escaped(source) && !result.has_escaped(node))
						{
							result.mark_escaped(node);
							changed = true;
						}
					}
					break;

				case NodeType::STORE:
				case NodeType::PTR_STORE:
					if (!node->inputs.empty())
					{
						if (Node *stored_val = node->inputs[0];
							stored_val->type_kind == DataType::POINTER)
						{
							if (Node *source = result.get_pointer_source(stored_val);
								!result.has_escaped(source))
							{
								result.mark_escaped(source);
								changed = true;
							}
						}
					}
					break;

				default:
					break;
			}
		}

		for (const Region *child: region->get_children())
		{
			if (propagate_escapes_in_region(result, child))
				changed = true;
		}

		return changed;
	}

	std::uint64_t LocalAliasAnalysisPass::extract_integer_literal(Node *node) const
	{
		if (!node || node->ir_type != NodeType::LIT)
			return 0;

		switch (node->type_kind)
		{
			case DataType::INT8:
				return static_cast<std::uint64_t>(node->data.get<DataType::INT8>());
			case DataType::INT16:
				return static_cast<std::uint64_t>(node->data.get<DataType::INT16>());
			case DataType::INT32:
				return static_cast<std::uint64_t>(node->data.get<DataType::INT32>());
			case DataType::INT64:
				return static_cast<std::uint64_t>(node->data.get<DataType::INT64>());
			case DataType::UINT8:
				return node->data.get<DataType::UINT8>();
			case DataType::UINT16:
				return node->data.get<DataType::UINT16>();
			case DataType::UINT32:
				return node->data.get<DataType::UINT32>();
			case DataType::UINT64:
				return node->data.get<DataType::UINT64>();
			default:
				return 0;
		}
	}

	std::int64_t LocalAliasAnalysisPass::compute_pointer_offset(Node *node, Node *&base_ptr)
	{
		if (!node)
			return -1;

		switch (node->ir_type)
		{
			case NodeType::PARAM:
			case NodeType::HEAP_ALLOC:
			case NodeType::STACK_ALLOC:
			case NodeType::ADDR_OF:
				base_ptr = node;
				return 0;

			case NodeType::PTR_ADD:
				if (node->inputs.size() >= 2)
				{
					const std::int64_t base_offset = compute_pointer_offset(node->inputs[0], base_ptr);
					if (base_offset == -1 || !base_ptr)
						return -1;

					if (Node *offset_node = node->inputs[1];
						offset_node->ir_type == NodeType::LIT)
					{
						auto offset_value = static_cast<std::int64_t>(extract_integer_literal(offset_node));
						return base_offset + offset_value;
					}
				}
				return -1;

			default:
				return -1;
		}
	}

	std::uint64_t LocalAliasAnalysisPass::get_access_size(Node *node) const
	{
		if (!node)
			return 0;
		switch (node->type_kind)
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
			case DataType::POINTER: /* note: assuming 64-bit pointers;
										doesn't really matter when it is platform-agnostic */
				return 8;

			case DataType::ARRAY:
			{
				/* for arrays, get the element type and count */
				if (node->data.type() == DataType::ARRAY)
				{
					const auto &[elem_type, count] = node->data.get<DataType::ARRAY>();
					const std::uint64_t elem_size = get_type_size(elem_type);
					return elem_size * count;
				}
				return 0;
			}

			case DataType::STRUCT:
			{
				/* for structs, get the size from the type data */
				if (node->data.type() == DataType::STRUCT)
					return node->data.get<DataType::STRUCT>().size;
				return 0;
			}

			default:
				return 0; /* unknown size */
		}
	}

	std::uint64_t LocalAliasAnalysisPass::get_type_size(DataType type) const
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
				return 0;
		}
	}
}
