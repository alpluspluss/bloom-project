/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/analysis-pass.hpp>
#include <bloom/foundation/pass-context.hpp>

namespace blm
{
	bool AnalysisPass::run(Module& module, PassContext& context)
	{
		auto result = analyze(module, context);
		if (!result)
			return false;

		context.store_result(blm_id(), std::move(result));
		return true;
	}
}
