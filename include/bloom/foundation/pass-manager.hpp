/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <bloom/foundation/pass-context.hpp>
#include <bloom/foundation/pass.hpp>

namespace blm
{
    /**
     * @brief Manages pass registration and execution.
     *
     * The PassManager is responsible for registering passes,
     * tracking dependencies between passes, executing passes
     * in the correct order, and collecting and reporting
     * statistics.
     */
    class PassManager
    {
    public:
        /**
         * @brief Creates a pass manager for the given module with specified options.
         * @param module The module to operate on.
         * @param opt_level The optimization level (0-3).
         * @param debug_mode Whether debug information is enabled.
         * @param verbosity Level of output detail (0=minimal, 1=normal, 2=verbose).
         */
        explicit PassManager(Module& module, int opt_level = 0,
                            bool debug_mode = false, int verbosity = 0);

        /**
         * @brief Registers a pass with optional configuration.
         * @tparam PassT The type of pass to register.
         * @tparam Args Types of arguments to forward to the pass constructor.
         * @param args Arguments to forward to the pass constructor.
         * @throws std::runtime_error if the pass is already registered.
         */
        template<typename PassT, typename... Args>
        void add_pass(Args&&... args)
        {
            auto pass = std::make_unique<PassT>(std::forward<Args>(args)...);
            const auto type_idx = std::type_index(pass->blm_id());

            /* get deps */
            auto required = pass->required_passes();
            auto invalidated = pass->invalidated_passes();
            passes[type_idx] = {
                .pass = std::move(pass),
                .required = {},
                .invalidated = {}
            };

            /* convert type info pointers to type indices */
            for (const auto* req : required)
            {
                passes[type_idx].required.push_back(std::type_index(*req));
            }

            for (const auto* inv : invalidated)
            {
                passes[type_idx].invalidated.push_back(std::type_index(*inv));
            }

            for (const auto& req_idx : passes[type_idx].required)
            {
                deps_graph[req_idx].dependents.insert(type_idx);
                deps_graph[type_idx].required.insert(req_idx);
            }

            /* Store pass in the order it was added */
            pass_order.push_back(type_idx);
        }

        /**
         * @brief Registers a pass with optional configuration.
         * @tparam PassT The type of pass to register.
         * @param pass The pass to register.
         * @throws std::runtime_error if the pass is already registered.
         */
        template<typename PassT>
        void add_pass(std::unique_ptr<PassT> pass)
        {
            const auto type_idx = std::type_index(pass->blm_id());

            /* get deps */
            auto required = pass->required_passes();
            auto invalidated = pass->invalidated_passes();

            PassInfo info;
            info.pass = std::move(pass); /* transfer ownership */
            info.required = {};
            info.invalidated = {};
            passes[type_idx] = std::move(info);

            /* convert type info pointers to type indices */
            for (const auto* req : required)
                passes[type_idx].required.push_back(std::type_index(*req));

            for (const auto* inv : invalidated)
                passes[type_idx].invalidated.push_back(std::type_index(*inv));

            for (const auto& req_idx : passes[type_idx].required)
            {
                deps_graph[req_idx].dependents.insert(type_idx);
                deps_graph[type_idx].required.insert(req_idx);
            }

            pass_order.push_back(type_idx);
        }

        /**
         * @brief Runs a specific pass.
         * @param pass_type Type information for the pass to run.
         * @return True if the pass succeeded, false otherwise.
         * @throws std::runtime_error if the pass is not found.
         */
        bool run_pass(const std::type_info& pass_type);

        /**
         * @brief Runs a pass by type.
         * @tparam PassT The type of pass to run.
         * @return True if the pass succeeded, false otherwise.
         */
        template<typename PassT>
        bool run_pass()
        {
            return run_pass(typeid(PassT));
        }

        /**
         * @brief Runs all registered passes in dependency order.
         * @return True if all passes succeeded, false otherwise.
         */
        bool run_all();

        /**
         * @brief Gets the pass context.
         * @return Reference to the pass context.
         */
        PassContext& get_context();
        [[nodiscard]] const PassContext& get_context() const;

        /**
         * @brief Sets the verbosity level.
         * @param level The verbosity level.
         */
        void set_verbosity(int level);

        /**
         * @brief Prints execution statistics.
         * @param os Output stream to print to.
         */
        void print_statistics(std::ostream& os = std::cout) const;

    private:
        /**
         * @brief Information about pass dependencies.
         */
        struct PassDependencyInfo
        {
            std::set<std::type_index> required;
            std::set<std::type_index> dependents;
        };

        /**
         * @brief Information about a registered pass.
         */
        struct PassInfo
        {
            std::unique_ptr<Pass> pass;
            std::vector<std::type_index> required;
            std::vector<std::type_index> invalidated;
        };

        Module& mod;
        int verbosity_lvl = 0;
        PassContext ctx;

        std::unordered_map<std::type_index, PassInfo> passes;
        std::unordered_map<std::type_index, PassDependencyInfo> deps_graph;
        std::unordered_map<std::type_index, double> pass_times;
        std::vector<std::type_index> pass_order;
    };
}
