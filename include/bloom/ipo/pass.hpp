/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <string_view>
#include <typeinfo>
#include <vector>
#include <bloom/ipo/pass-context.hpp>

namespace blm
{
    class Module;
    class IPOPassContext;

    /**
     * @brief Base class for IPO passes
     */
    class IPOPass
    {
    public:
        virtual ~IPOPass() = default;

        /**
         * @brief Execute the IPO pass on the given modules
         * @param modules Vector of modules to process
         * @param context The IPO pass context
         * @return True if the pass made changes, false otherwise
         */
        virtual bool run(std::vector<Module*>& modules, IPOPassContext& context) = 0;

        /**
         * @brief Get the name of this pass
         * @return The pass name
         */
        [[nodiscard]] virtual std::string_view name() const = 0;

        /**
         * @brief Get a description of what this pass does
         * @return The pass description
         */
        [[nodiscard]] virtual std::string_view description() const = 0;

        /**
         * @brief Get the type information for this pass
         * @return Type info for this pass
         */
        [[nodiscard]] virtual const std::type_info& blm_id() const = 0;

        /**
         * @brief Get the IPO analysis passes this pass requires
         * @return Vector of required analysis pass type infos
         */
        [[nodiscard]] virtual std::vector<const std::type_info*> required_passes() const
        {
            return {};
        }

        /**
         * @brief Get the IPO analysis passes this pass invalidates
         * @return Vector of invalidated analysis pass type infos
         */
        [[nodiscard]] virtual std::vector<const std::type_info*> invalidated_passes() const
        {
            return {};
        }

    protected:
        /**
         * @brief Helper to get analysis results with proper typing
         * @tparam T The type of analysis result to get
         * @param context The IPO pass context
         * @return Pointer to the analysis result, or nullptr if not available
         */
        template<typename T>
        const T* get_analysis(IPOPassContext& context) const
        {
            return context.get_result<T>();
        }

        /**
         * @brief Helper to mark analysis as preserved
         * @tparam T The type of analysis to preserve
         * @param context The IPO pass context
         */
        template<typename T>
        void preserve_analysis(IPOPassContext& context) const
        {
            context.mark_preserved<T>();
        }

        /**
         * @brief Helper to get multiple pass types for dependencies
         * @tparam PassTypes The pass types to get type info for
         * @return Vector of type info pointers
         */
        template<typename... PassTypes>
        static std::vector<const std::type_info*> get_pass_types()
        {
            return { &typeid(PassTypes)... };
        }
    };
}
