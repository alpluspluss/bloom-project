/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/ipo/pass-context.hpp>

namespace blm
{
    IPOPassContext::IPOPassContext(std::vector<Module*>& modules, int opt_level, bool debug_mode)
        : mods(modules), opt_lvl(opt_level), dbg_mode(debug_mode)
    {
    }

    std::vector<Module*>& IPOPassContext::modules()
    {
        return mods;
    }

    const std::vector<Module*>& IPOPassContext::modules() const
    {
        return mods;
    }

    int IPOPassContext::opt_level() const
    {
        return opt_lvl;
    }

    bool IPOPassContext::debug_mode() const
    {
        return dbg_mode;
    }

    void IPOPassContext::store_result(std::string_view key, std::unique_ptr<IPOAnalysisResult> result)
    {
        string_results[std::string(key)] = std::move(result);
    }

    const IPOAnalysisResult* IPOPassContext::get_result(std::string_view key) const
    {
        const auto it = string_results.find(std::string(key));
        if (it == string_results.end())
            return nullptr;
        return it->second.get();
    }

    bool IPOPassContext::has_result(std::string_view key) const
    {
        return string_results.contains(std::string(key));
    }

    void IPOPassContext::invalidate_by(const std::type_info& invalidating_pass)
    {
        /* remove all results that are invalidated by this pass type */
        std::erase_if(type_results, [&](const auto& pair) {
            const auto& [type_idx, result] = pair;

            /* check if this analysis is preserved */
            if (preserved_analyses.contains(type_idx))
                return false;

            return result->invalidated_by(invalidating_pass);
        });

        std::erase_if(string_results, [&](const auto& pair) {
            const auto& [key, result] = pair;
            return result->invalidated_by(invalidating_pass);
        });
    }

    void IPOPassContext::invalidate_by_modules(const std::unordered_set<Module*>& changed_modules)
    {
        /* remove all results that are invalidated by module changes */
        std::erase_if(type_results, [&](const auto& pair) {
            const auto& [type_idx, result] = pair;

            /* check if this analysis is preserved */
            if (preserved_analyses.contains(type_idx))
                return false;

            return result->invalidated_by_modules(changed_modules);
        });

        std::erase_if(string_results, [&](const auto& pair) {
            const auto& [key, result] = pair;
            return result->invalidated_by_modules(changed_modules);
        });
    }

    void IPOPassContext::invalidate_matching(std::string_view pattern)
    {
        std::erase_if(string_results, [&](const auto& pair) {
            const auto& [key, result] = pair;
            return matches_pattern(key, pattern);
        });
    }

    void IPOPassContext::update_stat(std::string_view name, std::size_t delta)
    {
        stats[std::string(name)] += delta;
    }

    std::size_t IPOPassContext::get_stat(std::string_view name) const
    {
        const auto it = stats.find(std::string(name));
        if (it == stats.end())
            return 0;
        return it->second;
    }

    bool IPOPassContext::matches_pattern(std::string_view key, std::string_view pattern) const
    {
        /* simple wildcard matching - supports * at the end */
        if (pattern.ends_with('*'))
        {
            const auto prefix = pattern.substr(0, pattern.length() - 1);
            return key.starts_with(prefix);
        }

        /* exact match */
        return key == pattern;
    }
}