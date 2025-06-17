/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <string>
#include <vector>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/dbinfo.hpp>
#include <bloom/support/string-table.hpp>

namespace blm
{
    class Module;

    /**
     * @brief Represents a region of code
     *
     * Regions form a hierarchical structure that represents different
     * levels of program scope: functions, blocks, modules, and so on.
     */
    class Region
    {
    public:
        /**
         * @brief Construct a new Region
         *
         * @param ctx Context that owns this region
         * @param mod Module that owns this region
         * @param name Name of the region
         * @param parent Parent region (nullptr for root)
         */
        Region(Context& ctx, Module &mod, std::string_view name, Region *parent = nullptr);

        /**
         * @brief Destructor
         */
        ~Region();

        Region(const Region &) = delete;

        Region &operator=(const Region &) = delete;

        Region(Region &&) = delete;

        Region &operator=(Region &&) = delete;

        /**
         * @brief Get the name of the region
         */
        [[nodiscard]] std::string_view get_name() const;

        /**
         * @brief Get the parent region
         */
        [[nodiscard]] Region *get_parent() const
        {
            return parent;
        }

        /**
         * @brief Get the module that owns this region
         */
        Module &get_module()
        {
            return module;
        }

        /**
         * @brief Get the module that owns this region (const version)
         */
        [[nodiscard]] const Module &get_module() const
        {
            return module;
        }

        /**
         * @brief Add a child region
         *
         * @param child Child region to add
         */
        void add_child(Region *child);

        /**
         * @brief Get all child regions
         */
        [[nodiscard]] const std::vector<Region *> &get_children() const
        {
            return children;
        }

        /**
         * @brief Create a node in this region
         *
         * @tparam T Type of node to create
         * @tparam Args Constructor argument types
         * @param args Constructor arguments
         * @return T* Pointer to created node
         */
        template<typename T, typename... Args>
            requires(std::is_base_of_v<Node, T>)
        T *create_node(Args &&... args);

        /**
         * @brief Add an existing node to this region
         *
         * @param node Node to add
         */
        void add_node(Node *node);

        /**
         * @brief Remove a node from this region
         *
         * @param node Node to remove
         */
        void remove_node(Node *node);

        /**
         * @brief Get all nodes in this region
         */
        [[nodiscard]] const std::vector<Node *> &get_nodes() const
        {
            return nodes;
        }

        /**
         * @brief Get the control dependency for this region
         */
        [[nodiscard]] Node *get_control_dependency() const
        {
            return control_dependency;
        }

        /**
        * @brief Get the debug info for this region
        */
        DebugInfo &get_debug_info()
        {
            return debug_info;
        }

        /**
         * @brief Get the debug info for this region (const version)
         */
        [[nodiscard]] const DebugInfo &get_debug_info() const
        {
            return debug_info;
        }

        /**
         * @brief Create a node with debug location information
         *
         * @tparam T Type of node to create
         * @param file_id ID of the source file
         * @param line Line number
         * @param column Column number (optional)
         * @param args Constructor arguments
         * @return T* Pointer to created node
         */
        template<typename T, typename... Args>
            requires(std::is_base_of_v<Node, T>)
        T *create_node_with_location(StringTable::StringId file_id,
                                     std::uint32_t line,
                                     std::uint32_t column,
                                     Args &&... args);

        /**
         * @brief Insert a node before another node in the region
         * @param before Node to insert before
         * @param node Node to insert
         */
        void insert_node_before(Node* before, Node* node);

        void insert_node_after(Node *after, Node *node);

        /**
         * @brief Insert a node after another node in the region
         * @param node Node to insert
         */
        void insert_at_beginning(Node* node);

        /**
         * @brief Check if region is terminated e.g. ends with return, branch and so on
         */
        [[nodiscard]] bool is_terminated() const;

        /**
         * @brief Check if this region dominates another region
         *
         * A region dominates another if all execution paths to the target region
         * must pass through this region. This includes both structured dominance
         * (parent-child hierarchy) and unstructured control flow analysis.
         *
         * Unlike simple tree-based dominance, this method accounts for
         * - JUMP nodes that can bypass hierarchical structure
         * - BRANCH nodes with targets outside normal control flow
         * - INVOKE nodes with exception handling paths
         *
         * @param possible_dominated The region to check if it's dominated by this region
         * @return true if this region dominates the possible_dominated region, false otherwise
         *
         * @note This method is critical for optimization passes like PRE, CSE, and code motion
         *       that require accurate dominance information for correctness
         */
        bool dominates(const Region* possible_dominated) const;

        /**
         * @brief Check if this region contains unstructured control flow targeting another region
         *
         * Examines all nodes in this region to detect control flow operations (JUMP, BRANCH, INVOKE)
         * that target the specified region. Such operations can break normal parent-child dominance
         * relationships by creating alternate execution paths.
         *
         * This method follows the Bloom IR operand conventions:
         * - JUMP: inputs[0] = target ENTRY node
         * - BRANCH: inputs[1] = true target ENTRY, inputs[2] = false target ENTRY
         * - INVOKE: last two inputs = normal/exception ENTRY nodes
         *
         * @param target The target region to check for unstructured jumps to
         * @return true if this region contains control flow that targets the specified region
         *         in a way that would break hierarchical dominance, false otherwise
         *
         * @note Used internally by dominates() to detect cases where tree-based dominance
         *       analysis would be insufficient
         */
        bool has_unstructured_jumps_to(const Region *target) const;

        /**
         * @brief Check dominance using only parent-child hierarchy
         *
         * Performs traditional tree-based dominance analysis where a region dominates
         * another if it's an ancestor in the region hierarchy. This method ignores
         * unstructured control flow and assumes strict hierarchical relationships.
         *
         * A region dominates another via tree structure if:
         * - They are the same region, OR
         * - This region is an ancestor of the target region in the parent-child tree
         *
         * @param possible_dominated The region to check if it's dominated by this region
         * @return true if this region dominates the target via hierarchical structure, false otherwise
         *
         * @note This is a helper method for dominates(). Most code should use dominates()
         *       instead as it provides more accurate analysis for modern IR with control flow
         *
         * @see dominates() for full dominance analysis including unstructured control flow
         */
        bool dominates_via_tree(const Region *possible_dominated) const;

        /**
          * @brief Replace a node with another node in this region
          *
          * This method replaces old_node with new_node in the region's node list,
          * maintaining the original position. It also updates all usages of old_node
          * to point to new_node.
          *
          * @param old_node Node to be replaced
          * @param new_node Node to replace with
          * @param update_connections Whether to update the connections between nodes
          * @return true if the node was found and replaced, false otherwise
          */
        bool replace_node(Node* old_node, Node* new_node, bool update_connections = true);

    private:
        std::vector<Region *> children;
        std::vector<Node *> nodes;
        Node *control_dependency = nullptr;
        Context& ctx;
        Module &module;
        Region *parent;
        DebugInfo debug_info { *this };
        StringTable::StringId name_id;
    };

    template<typename T, typename... Args>
        requires(std::is_base_of_v<Node, T>)
    T *Region::create_node(Args &&... args)
    {
        T *node = ctx.create<T>(std::forward<Args>(args)...);
        add_node(node);
        return node;
    }

    template<typename T, typename... Args>
            requires(std::is_base_of_v<Node, T>)
    T *Region::create_node_with_location(StringTable::StringId file_id,
                                     std::uint32_t line,
                                     std::uint32_t column,
                                     Args &&... args)
    {
        T *node = create_node<T>(std::forward<Args>(args)...);
        debug_info.set_node_location(node, file_id, line, column);
        return node;
    }
}
