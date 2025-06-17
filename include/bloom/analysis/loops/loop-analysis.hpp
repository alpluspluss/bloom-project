/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/analysis/loops/loop-detector.hpp>
#include <bloom/foundation/analysis-pass.hpp>
#include <bloom/foundation/pass-context.hpp>

namespace blm
{
	/**
	 * @brief Analysis result containing loop information for a module
	 */
	class LoopAnalysisResult : public AnalysisResult
	{
	public:
		/**
		 * @brief Get the innermost loop containing a region
		 * @param region The region to check
		 * @return Pointer to loop, or nullptr if not in a loop
		 */
		[[nodiscard]] Loop* get_loop_for_region(Region* region) const;

		/**
		 * @brief Get loop tree for a specific function
		 * @param function The function node
		 * @return Pointer to loop tree, or nullptr if not found
		 */
		[[nodiscard]] const LoopTree* get_loops_for_function(Node* function) const;

		/**
		 * @brief Check if this analysis is invalidated by a transform
		 * @param transform_type The type of transform pass
		 * @return True if invalidated
		 */
		bool invalidated_by(const std::type_info& transform_type) const override;

		class Impl;
		std::unique_ptr<Impl> pimpl;

		LoopAnalysisResult();
		~LoopAnalysisResult() override;
		LoopAnalysisResult(const LoopAnalysisResult&) = delete;
		LoopAnalysisResult& operator=(const LoopAnalysisResult&) = delete;
		LoopAnalysisResult(LoopAnalysisResult&&) noexcept;
		LoopAnalysisResult& operator=(LoopAnalysisResult&&) noexcept;
	};

	/**
	 * @brief Analysis pass that detects loops in all functions
	 */
	class LoopAnalysisPass : public AnalysisPass
	{
	public:
		/**
		 * @brief Get the name of this pass
		 */
		[[nodiscard]] std::string_view name() const override
		{
			return "loop-analysis";
		}

		/**
		 * @brief Get the description of this pass
		 */
		[[nodiscard]] std::string_view description() const override
		{
			return "analyzes loop structure and builds loop trees for optimization";
		}

		/**
		 * @brief Analyze the module and detect all loops
		 * @param module The module to analyze
		 * @param context The pass context
		 * @return Loop analysis result
		 */
		std::unique_ptr<AnalysisResult> analyze(Module& module, PassContext& context) override;
	};

	/**
	 * @brief Helper function to get loop analysis from pass context
	 * @param context The pass context
	 * @return Pointer to loop analysis result, or nullptr if not available
	 */
	inline const LoopAnalysisResult* get_loop_analysis(const PassContext& context)
	{
		return context.get_result<LoopAnalysisResult>();
	}
}
