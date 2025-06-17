/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <format>
#include <stdexcept>
#include <bloom/ipo/pass-manager.hpp>
#include <bloom/ipo/pass.hpp>

namespace blm
{
    IPOPassManager::IPOPassManager(std::vector<Module*>& modules, int opt_level, bool debug_mode, int verbosity)
        : mods(modules), verbosity_lvl(verbosity), ctx(modules, opt_level, debug_mode)
    {
    }

    bool IPOPassManager::run_pass(const std::type_info& pass_type)
    {
        const auto type_idx = std::type_index(pass_type);
        const auto it = passes.find(type_idx);

        if (it == passes.end())
        {
            throw std::runtime_error(
                std::format("IPO Pass {} not found", pass_type.name()));
        }

        IPOPass* pass = it->second.get();

        if (verbosity_lvl >= 1)
        {
            std::cout << "Running IPO pass: " << pass->name() << std::endl;
        }

        const auto start_time = std::chrono::high_resolution_clock::now();

        bool result = false;
        try
        {
            result = pass->run(mods, ctx);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(
                std::format("IPO pass {} failed: {}", pass->name(), e.what()));
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration<double>(end_time - start_time);
        pass_times[type_idx] = duration.count();

        if (verbosity_lvl >= 1)
        {
            std::cout << "IPO pass " << pass->name()
                      << (result ? " made changes" : " made no changes")
                      << " (" << duration.count() << "s)" << std::endl;
        }

        return result;
    }

    void IPOPassManager::run_all()
    {
        if (verbosity_lvl >= 1)
            std::cout << "Running " << pass_order.size() << " IPO passes..." << std::endl;

        for (const auto& type_idx : pass_order)
        {
            const auto it = passes.find(type_idx);
            if (it == passes.end())
                continue;

            IPOPass* pass = it->second.get();

            if (verbosity_lvl >= 2)
            {
                std::cout << "Running IPO pass: " << pass->name() << std::endl;
            }

            const auto start_time = std::chrono::high_resolution_clock::now();

            bool result = false;
            try
            {
                result = pass->run(mods, ctx);
            }
            catch (const std::exception& e)
            {
                throw std::runtime_error(
                    std::format("IPO pass {} failed: {}", pass->name(), e.what()));
            }

            const auto end_time = std::chrono::high_resolution_clock::now();
            const auto duration = std::chrono::duration<double>(end_time - start_time);
            pass_times[type_idx] = duration.count();

            if (result)
            {
                /* invalidate analysis results that this pass affects */
                ctx.invalidate_by(pass->blm_id());
            }

            if (verbosity_lvl >= 2)
            {
                std::cout << "IPO pass " << pass->name()
                          << (result ? " made changes" : " made no changes")
                          << " (" << duration.count() << "s)" << std::endl;
            }
        }

        if (verbosity_lvl >= 1)
        {
            std::cout << "IPO pass execution complete." << std::endl;
        }
    }

    IPOPassContext& IPOPassManager::get_context()
    {
        return ctx;
    }

    const IPOPassContext& IPOPassManager::get_context() const
    {
        return ctx;
    }

    void IPOPassManager::set_verbosity(int level)
    {
        verbosity_lvl = level;
    }

    void IPOPassManager::print_statistics(std::ostream& os) const
    {
        os << "IPO statistics" << std::endl;

        auto total_time = 0.0;
        for (const auto& [type_idx, time] : pass_times)
            total_time += time;

        for (const auto& type_idx : pass_order)
        {
            const auto pass_it = passes.find(type_idx);
            const auto time_it = pass_times.find(type_idx);

            if (pass_it != passes.end() && time_it != pass_times.end())
            {
                const auto& pass = pass_it->second;
                const double time = time_it->second;
                const double percentage = total_time > 0.0 ? (time / total_time) * 100.0 : 0.0;

                os << std::format("{:<30} {:>8.3f}s ({:>5.1f}%)",
                                 pass->name(), time, percentage) << std::endl;
            }
        }

        os << std::format("{:<30} {:>8.3f}s", "Total", total_time) << std::endl;
        os << std::endl;
    }
}
