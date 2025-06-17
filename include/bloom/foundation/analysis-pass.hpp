/* this project is part of the bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <memory>
#include <bloom/foundation/pass.hpp>

namespace blm
{
	/**
	 * @brief Abstract base class for analysis results.
	 */
	class AnalysisResult
	{
	public:
		virtual ~AnalysisResult() = default;

		/**
		 * @brief Check if this result is invalidated by a transform pass.
		 * @param transform_type The type information of the transform pass.
		 * @return True if this result is invalidated by the transform, false otherwise.
		 */
		[[nodiscard]] virtual bool invalidated_by(const std::type_info& transform_type) const = 0;
	};

	/**
	 * @brief Base class for passes that perform analysis.
	 *
	 * Analysis passes compute information about the IR without modifying it.
	 * They store their results in the pass context for use by other passes.
	 */
	class AnalysisPass : public Pass
	{
	public:
		/**
		 * @brief Performs the analysis and returns a result.
		 * @param module The module to analyze.
		 * @param context The pass context for accessing other analysis results.
		 * @return A unique pointer to the analysis result, or nullptr on failure.
		 */
		virtual std::unique_ptr<AnalysisResult> analyze(Module& module, PassContext& context) = 0;

		/**
		 * @brief Implementation of run that stores the result.
		 * @param module The module to analyze.
		 * @param context The pass context for storing results.
		 * @return True if analysis succeeded, false otherwise.
		 */
		bool run(Module& module, PassContext& context) override;
	};
}
