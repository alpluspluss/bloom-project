/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blm
{
    class Module;

    /**
     * @brief Base class for IPO analysis results
     */
    class IPOAnalysisResult
    {
    public:
        virtual ~IPOAnalysisResult() = default;

        /**
         * @brief Check if this result is invalidated by a transform pass
         * @param transform_type The type information of the transform pass
         * @return True if this result is invalidated by the transform, false otherwise
         */
        [[nodiscard]] virtual bool invalidated_by(const std::type_info& transform_type) const = 0;

        /**
         * @brief Check if this result is invalidated by changes to specific modules
         * @param changed_modules Set of modules that have been modified
         * @return True if this result needs to be recomputed, false otherwise
         */
        [[nodiscard]] virtual bool invalidated_by_modules(const std::unordered_set<Module*>& changed_modules) const
        {
            return !changed_modules.empty();
        }

        /**
         * @brief Get the modules this analysis depends on
         * @return Set of modules this result depends on
         */
        [[nodiscard]] virtual std::unordered_set<Module*> depends_on_modules() const = 0;
    };

    /**
     * @brief Context for IPO pass execution, storing analysis results and statistics
     */
    class IPOPassContext
    {
    public:
        /**
         * @brief Creates a context for the given modules with specified options
         * @param modules The modules being processed
         * @param opt_level The optimization level (0-3)
         * @param debug_mode Whether debug information is enabled
         */
        explicit IPOPassContext(std::vector<Module*>& modules, int opt_level = 0, bool debug_mode = false);

        /**
         * @brief Returns the modules being processed
         * @return Reference to the module vector
         */
        std::vector<Module*>& modules();
        [[nodiscard]] const std::vector<Module*>& modules() const;

        /**
         * @brief Returns the optimization level
         * @return The optimization level
         */
        [[nodiscard]] int opt_level() const;

        /**
         * @brief Checks if debug mode is enabled
         * @return True if debug mode is enabled, false otherwise
         */
        [[nodiscard]] bool debug_mode() const;

        /**
         * @brief Simple interface: stores an analysis result by type
         * @tparam T The type of result to store
         * @param result The analysis result to store
         */
        template<typename T>
        void store_result(std::unique_ptr<T> result)
        {
            static_assert(std::is_base_of_v<IPOAnalysisResult, T>, "T must inherit from IPOAnalysisResult");
            type_results[std::type_index(typeid(T))] = std::move(result);
        }

        /**
         * @brief Simple interface: gets a result by type
         * @tparam T The type of result to get
         * @return Pointer to the result, or nullptr if not found
         */
        template<typename T>
        const T* get_result() const
        {
            const auto it = type_results.find(std::type_index(typeid(T)));
            if (it == type_results.end())
                return nullptr;
            return dynamic_cast<const T*>(it->second.get());
        }

        /**
         * @brief Advanced interface, stores an analysis result with custom key
         * @param key Custom key for the result
         * @param result The analysis result to store
         */
        void store_result(std::string_view key, std::unique_ptr<IPOAnalysisResult> result);

        /**
         * @brief Advanced interface: gets a result by custom key
         * @param key Custom key for the result
         * @return Pointer to the result, or nullptr if not found
         */
        [[nodiscard]] const IPOAnalysisResult* get_result(std::string_view key) const;

        /**
         * @brief Checks if a result exists for a given type
         * @tparam T The type to check for
         * @return True if a result exists, false otherwise
         */
        template<typename T>
        [[nodiscard]] bool has_result() const
        {
            return type_results.contains(std::type_index(typeid(T)));
        }

        /**
         * @brief Checks if a result exists for a custom key
         * @param key The key to check for
         * @return True if a result exists, false otherwise
         */
        [[nodiscard]] bool has_result(std::string_view key) const;

        /**
         * @brief Invalidates results affected by a transform pass
         * @param invalidating_pass Type information for the invalidating pass
         */
        void invalidate_by(const std::type_info& invalidating_pass);

        /**
         * @brief Invalidates results affected by module changes
         * @param changed_modules Set of modules that have been modified
         */
        void invalidate_by_modules(const std::unordered_set<Module*>& changed_modules);

        /**
         * @brief Invalidates results matching a pattern (advanced interface)
         * @param pattern Pattern to match against keys (supports wildcards)
         */
        void invalidate_matching(std::string_view pattern);

        /**
         * @brief Marks an analysis as preserved (won't be invalidated)
         * @tparam T The type of analysis to preserve
         */
        template<typename T>
        void mark_preserved()
        {
            preserved_analyses.insert(std::type_index(typeid(T)));
        }

        /**
         * @brief Updates a statistic value
         * @param name The name of the statistic
         * @param delta The amount to add to the statistic
         */
        void update_stat(std::string_view name, std::size_t delta);

        /**
         * @brief Gets a statistic value
         * @param name The name of the statistic
         * @return The statistic value, or 0 if not found
         */
        [[nodiscard]] std::size_t get_stat(std::string_view name) const;

    private:
        std::unordered_map<std::type_index, std::unique_ptr<IPOAnalysisResult>> type_results;
        std::unordered_map<std::string, std::unique_ptr<IPOAnalysisResult>> string_results;
        std::unordered_set<std::type_index> preserved_analyses;
        std::unordered_map<std::string, std::size_t> stats;
        std::vector<Module*>& mods;
        int opt_lvl;
        bool dbg_mode;

        /**
         * @brief Helper to check if a key matches a pattern
         * @param key The key to check
         * @param pattern The pattern (supports * wildcard)
         * @return True if the key matches the pattern
         */
        [[nodiscard]] bool matches_pattern(std::string_view key, std::string_view pattern) const;
    };
}
