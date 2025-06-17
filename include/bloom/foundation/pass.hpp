/* this project is part of the bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <string_view>
#include <typeinfo>
#include <vector>
#include <bloom/foundation/module.hpp>

namespace blm
{
    class PassContext;

    /**
     * @brief Base class for all optimization passes.
     *
     * This class represents the interface that all optimization passes,
     * whether analysis or transformation, must implement.
     */
    class Pass
    {
    public:
        virtual ~Pass() = default;

        /**
         * @brief Returns the type identification for this pass.
         * @return Type information for the pass.
         */
        [[nodiscard]] virtual const std::type_info& blm_id() const
        {
            return typeid(*this);
        }

        /**
         * @brief Returns the name of the pass.
         * @return A string view containing the pass name.
         */
        [[nodiscard]] virtual std::string_view name() const = 0;

        /**
         * @brief Returns a description of what the pass does.
         * @return A string view containing the pass description.
         */
        [[nodiscard]] virtual std::string_view description() const = 0;

        /**
         * @brief Returns a list of passes that must run before this pass.
         * @return Vector of type_info pointers for required passes.
         */
        [[nodiscard]] virtual std::vector<const std::type_info*> required_passes() const
        {
            return {};
        }

        /**
         * @brief Returns a list of passes that this pass will invalidate.
         * @return Vector of type_info pointers for invalidated passes.
         */
        [[nodiscard]] virtual std::vector<const std::type_info*> invalidated_passes() const
        {
            return {};
        }

        /**
         * @brief Runs the pass on the given module with the provided context.
         * @param m The module to operate on.
         * @param ctx The pass context for storing results and stats.
         * @return True if the pass succeeded, false otherwise.
         */
        virtual bool run(Module& m, PassContext& ctx) = 0;

        /**
         * @brief Determines if this pass should run at the given optimization level.
         * @return True if the pass should run at this level, false otherwise.
         */
        [[nodiscard]] virtual bool run_at_opt_level(int) const
        {
            return true;
        }

        /**
         * @brief Helper for returning pass types for dependency specification.
         * @tparam PassTypes Types of passes to get type information for.
         * @return Vector of type_info pointers for the specified pass types.
         */
        template<typename... PassTypes>
        static std::vector<const std::type_info*> get_pass_types()
        {
            return { &typeid(PassTypes)... };
        }
    };
}
