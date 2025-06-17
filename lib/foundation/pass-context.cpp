/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/foundation/pass-context.hpp>

namespace blm
{
	PassContext::PassContext(Module& module, const int opt_level, const bool debug_mode)
		: mod(module), opt_lvl(opt_level), dbg_mode(debug_mode) {}

	Module& PassContext::module()
	{
		return mod;
	}

	const Module& PassContext::module() const
	{
		return mod;
	}

	int PassContext::opt_level() const
	{
		return opt_lvl;
	}

	bool PassContext::debug_mode() const
	{
		return dbg_mode;
	}

	void PassContext::store_result(const std::type_info& pass_type,
								  std::unique_ptr<AnalysisResult> result)
	{
		res[std::type_index(pass_type)] = std::move(result);
	}

	bool PassContext::has_result(const std::type_info& pass_type) const
	{
		return res.contains(std::type_index(pass_type));
	}

	void PassContext::invalidate(const std::type_info& pass_type)
	{
		res.erase(std::type_index(pass_type));
	}

	void PassContext::invalidate_by(const std::type_info& invalidating_pass)
	{
		std::vector<std::type_index> keys;
		keys.reserve(res.size());
		for (const auto& [key, value] : res)
			keys.push_back(key);

		for (const auto& key : keys)
		{
			if (const auto& result = res[key];
				result->invalidated_by(invalidating_pass))
			{
				res.erase(key);
			}
		}
	}

	void PassContext::update_stat(const std::string_view name, const std::size_t delta)
	{
		stats[std::string(name)] += delta;
	}

	std::size_t PassContext::get_stat(const std::string_view name) const
	{
		const auto it = stats.find(std::string(name));
		if (it == stats.end())
			return 0;
		return it->second;
	}
}
