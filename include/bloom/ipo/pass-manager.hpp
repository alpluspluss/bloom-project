/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <bloom/ipo/pass-context.hpp>

namespace blm
{
    class IPOPass;

    /**
     * @brief Manages IPO pass registration and execution
     *
     * The IPOPassManager is responsible for registering IPO passes,
     * executing them in the order they were added, and collecting
     * and reporting statistics.
     */
    class IPOPassManager
    {
    public:
        /**
         * @brief Creates an IPO pass manager for the given modules with specified options
         * @param modules The modules to operate on
         * @param opt_level The optimization level (0-3)
         * @param debug_mode Whether debug information is enabled
         * @param verbosity Level of output detail (0=minimal, 1=normal, 2=verbose)
         */
        explicit IPOPassManager(std::vector<Module*>& modules, int opt_level = 0,
                               bool debug_mode = false, int verbosity = 0);

        /**
         * @brief Registers a pass with optional configuration
         * @tparam PassT The type of pass to register
         * @tparam Args Types of arguments to forward to the pass constructor
         * @param args Arguments to forward to the pass constructor
         * @throws std::runtime_error if the pass is already registered
         */
        template<typename PassT, typename... Args>
        void add_pass(Args&&... args)
        {
            auto pass = std::make_unique<PassT>(std::forward<Args>(args)...);
            const auto type_idx = std::type_index(pass->blm_id());

            if (passes.contains(type_idx))
            {
                throw std::runtime_error("IPO Pass already registered");
            }

            passes[type_idx] = std::move(pass);
            pass_order.push_back(type_idx);
        }

        /**
         * @brief Registers a pass instance
         * @tparam PassT The type of pass to register
         * @param pass The pass to register
         * @throws std::runtime_error if the pass is already registered
         */
        template<typename PassT>
        void add_pass(std::unique_ptr<PassT> pass)
        {
            const auto type_idx = std::type_index(pass->blm_id());

            if (passes.contains(type_idx))
            {
                throw std::runtime_error("IPO Pass already registered");
            }

            passes[type_idx] = std::move(pass);
            pass_order.push_back(type_idx);
        }

        /**
         * @brief Runs a specific pass
         * @param pass_type Type information for the pass to run
         * @return True if the pass made changes, false otherwise
         * @throws std::runtime_error if the pass is not found or if the pass throws
         */
        bool run_pass(const std::type_info& pass_type);

        /**
         * @brief Runs a pass by type
         * @tparam PassT The type of pass to run
         * @return True if the pass made changes, false otherwise
         * @throws std::runtime_error if the pass is not found or if the pass throws
         */
        template<typename PassT>
        bool run_pass()
        {
            return run_pass(typeid(PassT));
        }

        /**
         * @brief Runs all registered passes in the order they were added
         * @throws std::runtime_error if any pass throws an exception
         */
        void run_all();

        /**
         * @brief Gets the IPO pass context
         * @return Reference to the IPO pass context
         */
        IPOPassContext& get_context();
        const IPOPassContext& get_context() const;

        /**
         * @brief Sets the verbosity level
         * @param level The verbosity level
         */
        void set_verbosity(int level);

        /**
         * @brief Prints execution statistics
         * @param os Output stream to print to
         */
        void print_statistics(std::ostream& os = std::cout) const;

    private:
        std::vector<Module*>& mods;
        int verbosity_lvl = 0;
        IPOPassContext ctx;

        std::unordered_map<std::type_index, std::unique_ptr<IPOPass>> passes;
        std::unordered_map<std::type_index, double> pass_times;
        std::vector<std::type_index> pass_order;
    };
}
