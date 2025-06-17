/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/foundation/pass-manager.hpp>
#include <bloom/ipo/pass.hpp>
#include <bloom/transform/cse.hpp>

namespace blm
{
	/**
	 * @brief IPO Global Value Numbering pass
	 */
	class IPOGVNPass final : public IPOPass
	{
	public:
		bool run(std::vector<Module*>& modules, IPOPassContext& context) override
		{
			std::size_t total_eliminated = 0;

			for (Module* module : modules)
			{
				if (!module)
					continue;

				PassManager local_pm(*module, context.opt_level(), context.debug_mode(), 0);
				local_pm.add_pass<LocalAliasAnalysisPass>();
				local_pm.add_pass<CSEPass>();

				if (local_pm.run_all())
				{
					total_eliminated += local_pm.get_context().get_stat("cse.eliminated_expressions");
				}
			}

			context.update_stat("ipo_gvn.total_eliminated", total_eliminated);
			return total_eliminated > 0;
		}

		[[nodiscard]] std::string_view name() const override
		{
			return "ipo-global-value-numbering";
		}

		[[nodiscard]] std::string_view description() const override
		{
			return "performs global value numbering across all modules using local GVN";
		}

		[[nodiscard]] const std::type_info& blm_id() const override
		{
			return typeid(IPOGVNPass);
		}
	};

	/**
	 * @brief Alias for CSE pass to make the GVN naming clear
	 */
	using GVNPass = CSEPass;
}
