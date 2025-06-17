/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_set>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/pass-manager.hpp>

namespace blm
{
    PassManager::PassManager(Module &module, const int opt_level,
                             const bool debug_mode, const int verbosity) : mod(module), verbosity_lvl(verbosity),
                                                                           ctx(module, opt_level, debug_mode) {}

    bool PassManager::run_pass(const std::type_info &pass_type)
    {
        const auto type_idx = std::type_index(pass_type);
        const auto it = passes.find(type_idx);
        if (it == passes.end())
        {
            throw std::runtime_error(
                std::format("pass {} not found", pass_type.name()));
        }

        auto &[pass, required, invalidated] = it->second;

        /* Run any required passes first */
        for (const auto &req: required)
        {
            const std::type_info *req_type = nullptr;
            for (const auto &[idx, data]: passes)
            {
                if (idx == req)
                {
                    req_type = &data.pass->blm_id();
                    break;
                }
            }

            if (!req_type)
            {
                throw std::runtime_error(
                    std::format("Required pass {} not found", req.name()));
            }

            if (!run_pass(*req_type))
                return false;
        }

        /* skip if not applicable at this opt level */
        if (!pass->run_at_opt_level(ctx.opt_level()))
            return true; /* skipping is not failure */

        const auto start = std::chrono::high_resolution_clock::now();
        const bool success = pass->run(mod, ctx);
        const auto end = std::chrono::high_resolution_clock::now();

        /* record timing information */
        const auto duration = std::chrono::duration<double>(end - start).count();
        pass_times[type_idx] += duration;

        /* output progress information based on verbosity */
        if (verbosity_lvl > 0)
        {
            std::cout << std::format("pass {} completed in {:.2f}ms ({})\n",
                                     pass->name(),
                                     duration * 1000,
                                     success ? "success" : "failure");
        }

        if (success)
        {
            for (const auto &inv: invalidated)
            {
                const std::type_info *inv_type = nullptr;
                for (const auto &[idx, data]: passes)
                {
                    if (idx == inv)
                    {
                        inv_type = &data.pass->blm_id();
                        break;
                    }
                }

                if (inv_type)
                {
                    ctx.invalidate(*inv_type);
                }
            }
            ctx.invalidate_by(pass_type);
        }

        return success;
    }

    bool PassManager::run_all()
    {
        /* Run passes in the order they were added */
        for (const auto& type_idx : pass_order)
        {
            const std::type_info *pass_type = nullptr;
            for (const auto &[idx, data]: passes)
            {
                if (idx == type_idx)
                {
                    pass_type = &data.pass->blm_id();
                    break;
                }
            }

            if (!pass_type)
            {
                throw std::runtime_error(
                    std::format("pass {} not found in execution order", type_idx.name()));
            }

            if (!run_pass(*pass_type))
                return false;
        }

        return true;
    }

    PassContext &PassManager::get_context()
    {
        return ctx;
    }

    const PassContext &PassManager::get_context() const
    {
        return ctx;
    }

    void PassManager::set_verbosity(const int level)
    {
        verbosity_lvl = level;
    }

    void PassManager::print_statistics(std::ostream &os) const
    {
        if (pass_times.empty())
        {
            os << "no passes have been executed.\n";
            return;
        }

        std::vector<std::pair<std::type_index, double> > sorted_times(
            pass_times.begin(), pass_times.end());

        std::ranges::sort(sorted_times,
                          [](const auto &a, const auto &b)
                          {
                              return a.second > b.second;
                          });

        auto total_time = 0.0;
        for (const auto &[_, time]: sorted_times)
            total_time += time;
        os << "pass execution statistics:\n";

        for (const auto &[idx, time]: sorted_times)
        {
            double percent = (time / total_time) * 100.0;
            std::string_view name = "unknown";
            if (auto it = passes.find(idx);
                it != passes.end())
            {
                name = it->second.pass->name();
            }

            os << std::format("{:<30} {:>8.2f}ms ({:>5.1f}%)\n",
                              name, time * 1000, percent);
        }

        os << std::format("total: {:.2f}ms\n", total_time * 1000);
    }
}
