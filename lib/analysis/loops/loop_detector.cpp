/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <queue>
#include <stack>
#include <bloom/analysis/loops/loop-detector.hpp>

namespace blm
{
	LoopTree LoopDetector::analyze_function(Region *function_region)
	{
		if (!function_region)
			return {};

		std::vector<BackEdge> back_edges = find_back_edges(function_region);
		std::vector<std::unique_ptr<Loop> > loops;
		for (const BackEdge &edge: back_edges)
		{
			if (auto loop = build_natural_loop(edge))
				loops.push_back(std::move(loop));
		}

		return build_loop_tree(std::move(loops));
	}

	std::vector<LoopDetector::BackEdge> LoopDetector::find_back_edges(Region *root)
	{
		std::vector<BackEdge> back_edges;

		visit_regions(root, [&](Region *region)
		{
			for (const Node *node: region->get_nodes())
			{
				/* only consider control flow nodes */
				if (node->ir_type != NodeType::JUMP &&
				    node->ir_type != NodeType::BRANCH &&
				    node->ir_type != NodeType::INVOKE)
					continue;

				std::vector<Region *> targets = get_control_targets(node);
				for (Region *target: targets)
				{
					/* back-edge: target dominates source */
					if (target && target->dominates(region))
						back_edges.push_back({ region, target });
				}
			}
		});

		return back_edges;
	}

	std::unique_ptr<Loop> LoopDetector::build_natural_loop(const BackEdge &back_edge)
	{
		Region *header = back_edge.target;
		Region *latch = back_edge.source;

		/* create the loop */
		auto loop = std::make_unique<Loop>();
		loop->header = header;
		loop->latches.push_back(latch);

		loop->body_regions = find_loop_body(header, latch);
		for (Region *region: loop->get_all_regions())
		{
			for (const Node *node: region->get_nodes())
			{
				if (node->ir_type != NodeType::JUMP &&
				    node->ir_type != NodeType::BRANCH &&
				    node->ir_type != NodeType::INVOKE)
					continue;

				for (std::vector<Region *> targets = get_control_targets(node);
				     Region *target: targets)
				{
					/* if target is outside the loop, it's an exit */
					if (target && !loop->contains(target))
					{
						if (std::ranges::find(loop->exits, target) == loop->exits.end())
							loop->exits.push_back(target);
					}
				}
			}
		}

		return loop;
	}

	std::vector<Region *> LoopDetector::get_control_targets(const Node *node)
	{
		std::vector<Region *> targets;

		if (!node)
			return targets;

		switch (node->ir_type)
		{
			case NodeType::JUMP:
				/* JUMP has one target: inputs[0] should be an ENTRY node */
				if (node->inputs.size() >= 1)
				{
					if (const Node *entry = node->inputs[0];
						entry && entry->ir_type == NodeType::ENTRY)
					{
						targets.push_back(entry->parent_region);
					}
				}
				break;

			case NodeType::BRANCH:
				/* BRANCH has two targets: inputs[1] (true), inputs[2] (false) */
				if (node->inputs.size() >= 3)
				{
					/* inputs[0] = condition, inputs[1] = true target, inputs[2] = false target */
					for (std::size_t i = 1; i <= 2; i++)
					{
						if (const Node *entry = node->inputs[i];
							entry && entry->ir_type == NodeType::ENTRY)
						{
							targets.push_back(entry->parent_region);
						}
					}
				}
				break;

			case NodeType::INVOKE:
				/* INVOKE has two targets: normal and exception paths */
				if (node->inputs.size() >= 2)
				{
					/* last two inputs are normal and exception ENTRY nodes */
					for (std::size_t i = node->inputs.size() - 2; i < node->inputs.size(); i++)
					{
						if (const Node *entry = node->inputs[i];
							entry && entry->ir_type == NodeType::ENTRY)
						{
							targets.push_back(entry->parent_region);
						}
					}
				}
				break;

			default:
				break;
		}

		return targets;
	}

	std::unordered_set<Region *> LoopDetector::find_loop_body(Region *header, Region *latch)
	{
		std::unordered_set<Region *> body;
		std::stack<Region *> worklist;

		worklist.push(latch);
		body.insert(latch);
		while (!worklist.empty())
		{
			Region *current = worklist.top();
			worklist.pop();

			/* if we've reached the header, don't go further */
			if (current == header)
				continue;

			/* find all regions that can reach current by
			 * just check all regions to see if they have
			 * control flow edges to current */
			visit_regions(header, [&](Region *candidate)
			{
				if (body.contains(candidate) || candidate == header)
					return;

				/* if candidate has control flow to current */
				for (const Node *node: candidate->get_nodes())
				{
					if (node->ir_type != NodeType::JUMP &&
					    node->ir_type != NodeType::BRANCH &&
					    node->ir_type != NodeType::INVOKE)
						continue;

					std::vector<Region *> targets = get_control_targets(node);
					for (Region *target: targets)
					{
						if (target == current)
						{
							body.insert(candidate);
							worklist.push(candidate);
							return;
						}
					}
				}

				/* also check parent-child relationships for structured control flow */
				if (candidate->get_parent() == current ||
				    std::ranges::find(candidate->get_children(), current) != candidate->get_children().end())
				{
					body.insert(candidate);
					worklist.push(candidate);
				}
			});
		}

		/* remove the latch from body if it's the same as header */
		if (latch == header)
			body.erase(latch);

		return body;
	}

	LoopTree LoopDetector::build_loop_tree(std::vector<std::unique_ptr<Loop> > loops)
	{
		LoopTree tree;

		if (loops.empty())
			return tree;

		establish_loop_hierarchy(loops);
		for (auto &loop: loops)
		{
			std::size_t depth = 0;
			Loop *parent = loop->parent;
			while (parent)
			{
				depth++;
				parent = parent->parent;
			}
			loop->depth = depth;
			tree.max_depth = std::max(tree.max_depth, depth);
			for (Region *region: loop->get_all_regions())
			{
				if (!tree.region_to_loop.contains(region) ||
				    tree.region_to_loop[region]->depth < loop->depth)
				{
					tree.region_to_loop[region] = loop.get();
				}
			}

			if (!loop->parent)
				tree.root_loops.push_back(loop.get());

			tree.all_loops.push_back(std::move(loop));
		}

		return tree;
	}

	void LoopDetector::establish_loop_hierarchy(std::vector<std::unique_ptr<Loop> > &loops)
	{
		for (auto &loop: loops)
		{
			Loop *best_parent = nullptr;
			auto best_parent_size = SIZE_MAX;

			for (auto &candidate: loops)
			{
				/* skip self */
				if (candidate.get() == loop.get())
					continue;

				if (candidate->contains(loop->header))
				{
					/* prefer the smallest containing loop */
					std::size_t candidate_size = candidate->get_all_regions().size();
					if (candidate_size < best_parent_size)
					{
						best_parent = candidate.get();
						best_parent_size = candidate_size;
					}
				}
			}

			if (best_parent)
			{
				loop->parent = best_parent;
				best_parent->children.push_back(loop.get());
			}
		}
	}
}
