/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/laa.hpp>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/support/relation.hpp>
#include <bloom/transform/dse.hpp>

namespace blm
{
	std::string_view DSEPass::name() const
	{
		return "dead-store-elimination";
	}

	std::string_view DSEPass::description() const
	{
		return "removes stores that are never read before being overwritten";
	}

	std::vector<const std::type_info *> DSEPass::required_passes() const
	{
		return get_pass_types<LocalAliasAnalysisPass>();
	}

	bool DSEPass::run(Module &m, PassContext &ctx)
	{
		const auto *alias_result = ctx.get_result<LocalAliasResult>();
		std::unique_ptr<LocalAliasResult> local_result;
		if (!alias_result)
		{
			LocalAliasAnalysisPass laa;
			local_result = std::unique_ptr<LocalAliasResult>(
				dynamic_cast<LocalAliasResult *>(laa.analyze(m, ctx).release()));
			alias_result = local_result.get();
		}

		dead_stores.clear();
		const auto removed = static_cast<std::int64_t>(process_region(m.get_root_region(), *alias_result));
		ctx.update_stat("dse.removed_stores", removed);

		return removed > 0;
	}

	std::size_t DSEPass::process_region(const Region *region, const LocalAliasResult &alias_result) // NOLINT(*-no-recursion)
	{
		if (!region)
			return 0;

		std::size_t removed = 0;
		std::unordered_map<Node *, Node *> last_store_to_location;
		std::unordered_set<Node *> potentially_dead_stores;
		std::unordered_set<Node *> definitely_live_stores;
		for (Node *node: region->get_nodes())
		{
			if (is_store_node(node))
			{
				Node *store_addr = get_store_address(node);
				if (!store_addr)
					continue;

				if (is_global_scope(node->parent_region) ||
					is_global_scope(store_addr->parent_region))
				{
					definitely_live_stores.insert(node);
					continue;
				}

				if ((node->props & NodeProps::NO_OPTIMIZE) != NodeProps::NONE)
				{
					definitely_live_stores.insert(node);
					continue;
				}

				std::vector<Node *> aliasing_addresses_to_remove;
				for (const auto &[other_addr, other_store]: last_store_to_location)
				{
					if (other_addr == store_addr)
						continue;

					if (AliasResult alias = alias_result.alias(store_addr, other_addr);
						alias == AliasResult::MUST_ALIAS)
					{
						/* definitely same location; previous store is potentially dead */
						potentially_dead_stores.insert(other_store);
						aliasing_addresses_to_remove.push_back(other_addr);
					}
					else if (alias == AliasResult::PARTIAL_ALIAS)
					{
						if (can_eliminate_partial_overlap(other_store, node, other_addr, store_addr, alias_result))
						{
							potentially_dead_stores.insert(other_store);
							aliasing_addresses_to_remove.push_back(other_addr);
						}
					}
				}

				if (auto it = last_store_to_location.find(store_addr);
					it != last_store_to_location.end())
				{
					potentially_dead_stores.insert(it->second);
				}

				for (Node *addr_to_remove: aliasing_addresses_to_remove)
					last_store_to_location.erase(addr_to_remove);
				last_store_to_location[store_addr] = node;
			}
			else if (is_load_node(node))
			{
				Node *load_addr = get_memory_address(node);
				if (!load_addr)
					continue;

				/* this load makes stores to same/aliasing locations live */
				for (const auto &[store_addr, store]: last_store_to_location)
				{
					AliasResult alias = alias_result.alias(load_addr, store_addr);
					if (alias != AliasResult::NO_ALIAS)
					{
						/* this store is read; definitely live */
						definitely_live_stores.insert(store);
						potentially_dead_stores.erase(store);
					}
				}
			}
			else if (is_call_node(node))
			{
				for (const auto &[store_addr, store]: last_store_to_location)
				{
					if (alias_result.has_escaped(store_addr))
					{
						definitely_live_stores.insert(store);
						potentially_dead_stores.erase(store);
					}
				}
			}
		}

		std::unordered_set<Node *> final_stores_to_remove;
		for (Node *store: potentially_dead_stores)
		{
			/* only eliminate if not marked as definitely live AND address hasn't escaped */
			if (!definitely_live_stores.contains(store))
			{
				if (Node *store_addr = get_store_address(store);
					store_addr && !alias_result.has_escaped(store_addr))
				{
					final_stores_to_remove.insert(store);
				}
			}
		}

		for (Node *store: final_stores_to_remove)
		{
			dead_stores.insert(store);
			remove_dead_store(store);
			removed++;
		}

		for (const Region *child: region->get_children())
			removed += process_region(child, alias_result);

		return removed;
	}

	bool DSEPass::is_store_node(Node *node)
	{
		return node && (node->ir_type == NodeType::STORE ||
		                node->ir_type == NodeType::PTR_STORE ||
		                node->ir_type == NodeType::ATOMIC_STORE);
	}

	bool DSEPass::is_load_node(Node *node)
	{
		return node && (node->ir_type == NodeType::LOAD ||
		                node->ir_type == NodeType::PTR_LOAD ||
		                node->ir_type == NodeType::ATOMIC_LOAD);
	}

	bool DSEPass::is_call_node(Node *node)
	{
		return node && (node->ir_type == NodeType::CALL || node->ir_type == NodeType::INVOKE);
	}

	Node *DSEPass::get_store_address(Node *store)
	{
		if (!store)
			return nullptr;

		switch (store->ir_type)
		{
			case NodeType::STORE:case NodeType::PTR_STORE:
			case NodeType::ATOMIC_STORE:
				return store->inputs.size() > 1 ? store->inputs[1] : nullptr;
			default:
				return nullptr;
		}
	}

	Node *DSEPass::get_memory_address(Node *mem_op)
	{
		if (!mem_op)
			return nullptr;

		switch (mem_op->ir_type)
		{
			case NodeType::LOAD:
			case NodeType::PTR_LOAD:
			case NodeType::ATOMIC_LOAD:
				return mem_op->inputs.empty() ? nullptr : mem_op->inputs[0];

			case NodeType::STORE:
			case NodeType::PTR_STORE:
			case NodeType::ATOMIC_STORE:
				return mem_op->inputs.size() > 1 ? mem_op->inputs[1] : nullptr;

			default:
				return nullptr;
		}
	}

	void DSEPass::remove_dead_store(Node *store)
	{
		if (!store || !store->parent_region)
			return;
		for (Node *input: store->inputs)
		{
			if (input)
			{
				auto &users = input->users;
				std::erase(users, store);
			}
		}

		store->parent_region->remove_node(store);
	}

	bool DSEPass::can_eliminate_partial_overlap(Node *old_store, Node *new_store,
	                                            Node *old_addr, Node *new_addr,
	                                            const LocalAliasResult &alias_result) const
	{
		const MemoryLocation *old_loc = alias_result.get_location(old_addr);
		const MemoryLocation *new_loc = alias_result.get_location(new_addr);
		if (!old_loc || !new_loc)
			return false; /* can't analyze without location info */

		if (old_loc->base != new_loc->base ||
		    old_loc->offset == -1 || new_loc->offset == -1 ||
		    old_loc->size == 0 || new_loc->size == 0)
		{
			return false;
		}

		const std::int64_t old_start = old_loc->offset;
		const std::int64_t old_end = old_start + static_cast<std::int64_t>(old_loc->size);
		const std::int64_t new_start = new_loc->offset;
		if (const std::int64_t new_end = new_start + static_cast<int64_t>(new_loc->size);
			new_start <= old_start && new_end >= old_end)
		{
			return true; /* new store completely overwrites old store */
		}

		if (old_loc->size == new_loc->size &&
		    old_start == new_start &&
		    get_store_value_type(old_store) == get_store_value_type(new_store))
		{
			return true;
		}
		return false; /* keep both stores for other partial overlap cases */
	}

	DataType DSEPass::get_store_value_type(Node *store)
	{
		if (!store || store->inputs.empty())
			return DataType::VOID;
		return store->inputs[0]->type_kind;
	}
}
