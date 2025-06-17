/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/analysis/loops/loop-analysis.hpp>
#include <bloom/analysis/loops/loop-detector.hpp>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>

namespace blm
{
	class LoopAnalysisResult::Impl
	{
	public:
		std::unordered_map<Node *, LoopTree> function_loops;
	};

	LoopAnalysisResult::LoopAnalysisResult() : pimpl(std::make_unique<Impl>()) {}

	LoopAnalysisResult::~LoopAnalysisResult() = default;

	LoopAnalysisResult::LoopAnalysisResult(LoopAnalysisResult &&) noexcept = default;

	LoopAnalysisResult &LoopAnalysisResult::operator=(LoopAnalysisResult &&) noexcept = default;

	Loop *LoopAnalysisResult::get_loop_for_region(Region *region) const
	{
		const Region *root = region;
		while (root->get_parent())
			root = root->get_parent();

		for (Node *node: root->get_nodes())
		{
			if (node->ir_type == NodeType::FUNCTION)
			{
				const LoopTree *tree = get_loops_for_function(node);
				return tree ? tree->get_loop_for(region) : nullptr;
			}
		}

		return nullptr;
	}

	const LoopTree *LoopAnalysisResult::get_loops_for_function(Node *function) const
	{
		const auto it = pimpl->function_loops.find(function);
		return it != pimpl->function_loops.end() ? &it->second : nullptr;
	}

	bool LoopAnalysisResult::invalidated_by(const std::type_info&) const
	{
		return true;
	}

	std::unique_ptr<AnalysisResult> LoopAnalysisPass::analyze(Module &module, PassContext &context)
	{
		auto result = std::make_unique<LoopAnalysisResult>();
		for (Node *function: module.get_functions())
		{
			if (function->ir_type != NodeType::FUNCTION)
				continue;

			/* find the function's region */
			Region *function_region = nullptr;
			for (Region *child: module.get_root_region()->get_children())
			{
				if (child->get_name() == module.get_context().get_string(function->str_id))
				{
					function_region = child;
					break;
				}
			}

			if (function_region)
			{
				LoopTree tree = LoopDetector::analyze_function(function_region);
				result->pimpl->function_loops[function] = std::move(tree);
			}
		}

		std::size_t total_loops = 0;
		std::size_t max_depth = 0;
		for (const auto &[function, tree]: result->pimpl->function_loops)
		{
			total_loops += tree.all_loops.size();
			max_depth = std::max(max_depth, tree.max_depth);
		}

		context.update_stat("loop_analysis.total_loops", total_loops);
		context.update_stat("loop_analysis.max_nesting_depth", max_depth);
		return result;
	}
}
