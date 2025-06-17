/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <string>
#include <typeindex>
#include <unordered_map>
#include <bloom/foundation/analysis-pass.hpp>

namespace blm
{
    class Module;

    /**
     * @brief Context for pass execution, storing analysis results and statistics.
     *
     * The PassContext maintains state between passes, including analysis results
     * and statistics about the optimization process.
     */
    class PassContext
    {
    public:
        /**
         * @brief Creates a context for the given module with specified options.
         * @param module The module being processed.
         * @param opt_level The optimization level (0-3).
         * @param debug_mode Whether debug information is enabled.
         */
        explicit PassContext(Module& module, int opt_level = 0, bool debug_mode = false);

        /**
         * @brief Returns the module being processed.
         * @return Reference to the module.
         */
        Module& module();
        [[nodiscard]] const Module& module() const;

        /**
         * @brief Returns the optimization level.
         * @return The optimization level.
         */
        [[nodiscard]] int opt_level() const;

        /**
         * @brief Checks if debug mode is enabled.
         * @return True if debug mode is enabled, false otherwise.
         */
        [[nodiscard]] bool debug_mode() const;

        /**
         * @brief Stores an analysis result.
         * @param pass_type Type information for the pass.
         * @param result The analysis result to store.
         */
        void store_result(const std::type_info& pass_type,
                         std::unique_ptr<AnalysisResult> result);

        /**
         * @brief Gets a result by its type.
         * @tparam ResultT The type of result to get.
         * @return Pointer to the result, or nullptr if not found.
         */
        template<typename ResultT>
        [[nodiscard]] const ResultT* get_result() const
        {
            const auto it = res.find(std::type_index(typeid(ResultT)));
            if (it == res.end())
                return nullptr;
            return dynamic_cast<const ResultT*>(it->second.get());
        }

        /**
         * @brief Gets a result for a specific pass type.
         * @tparam ResultT The type of result to get.
         * @param pass_type Type information for the pass.
         * @return Pointer to the result, or nullptr if not found.
         */
        template<typename ResultT>
        [[nodiscard]] const ResultT* get_result(const std::type_info& pass_type) const
        {
            const auto it = res.find(std::type_index(pass_type));
            if (it == res.end())
                return nullptr;
            return dynamic_cast<const ResultT*>(it->second.get());
        }

        /**
         * @brief Checks if a result exists for a pass.
         * @param pass_type Type information for the pass.
         * @return True if a result exists, false otherwise.
         */
        [[nodiscard]] bool has_result(const std::type_info& pass_type) const;

        /**
         * @brief Invalidates the result for a specific pass.
         * @param pass_type Type information for the pass.
         */
        void invalidate(const std::type_info& pass_type);

        /**
         * @brief Invalidates results affected by a transform pass.
         * @param invalidating_pass Type information for the invalidating pass.
         */
        void invalidate_by(const std::type_info& invalidating_pass);

        /**
         * @brief Updates a statistic value.
         * @param name The name of the statistic.
         * @param delta The amount to add to the statistic.
         */
        void update_stat(std::string_view name, std::size_t delta);

        /**
         * @brief Gets a statistic value.
         * @param name The name of the statistic.
         * @return The statistic value, or 0 if not found.
         */
        [[nodiscard]] std::size_t get_stat(std::string_view name) const;

    private:
        Module& mod;
        int opt_lvl;
        bool dbg_mode;

        std::unordered_map<std::type_index, std::unique_ptr<AnalysisResult>> res;
        std::unordered_map<std::string, std::size_t> stats;
    };
}
