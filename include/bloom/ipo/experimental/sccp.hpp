/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <bloom/foundation/node.hpp>
#include <bloom/ipo/callgraph.hpp>
#include <bloom/ipo/pass.hpp>
#include <bloom/ipo/specializer.hpp>

namespace blm
{
    /**
     * @brief Result of IPO SCCP analysis containing lattice values for all nodes
     */
    class IPOSCCPResult final : public IPOAnalysisResult
    {
    public:
        /**
         * @brief Get the lattice value for a node
         * @param node The node to query
         * @return Lattice value, or TOP if not found
         */
        [[nodiscard]] LatticeValue get_lattice_value(Node* node) const
        {
            const auto it = lattice_values.find(node);
            return it != lattice_values.end() ? it->second : LatticeValue{};
        }

        /**
         * @brief Set the lattice value for a node
         * @param node The node to set
         * @param value The lattice value
         */
        void set_lattice_value(Node* node, const LatticeValue& value)
        {
            lattice_values[node] = value;
        }

        /**
         * @brief Check if a node has a constant value
         * @param node The node to check
         * @return True if node is constant
         */
        [[nodiscard]] bool is_constant(Node* node) const
        {
            return get_lattice_value(node).is_constant();
        }

        /**
         * @brief Get all nodes that were found to be constant
         * @return Set of constant nodes
         */
        [[nodiscard]] std::unordered_set<Node*> get_constant_nodes() const
        {
            std::unordered_set<Node*> constants;
            for (const auto& [node, value] : lattice_values)
            {
                if (value.is_constant())
                    constants.insert(node);
            }
            return constants;
        }

        [[nodiscard]] bool invalidated_by(const std::type_info& transform_type) const override;

        [[nodiscard]] std::unordered_set<Module*> depends_on_modules() const override
        {
            return analyzed_modules;
        }

        std::unordered_set<Module*> analyzed_modules;

    private:
        std::unordered_map<Node*, LatticeValue> lattice_values;
    };

    /**
     * @brief IPO Sparse Conditional Constant Propagation pass
     *
     * Performs constant propagation across module boundaries using worklist algorithm.
     * Propagates constant arguments into callees and constant return values back to callers.
     */
    class IPOSCCPPass final : public IPOPass
    {
    public:
        bool run(std::vector<Module*>& modules, IPOPassContext& context) override;

        [[nodiscard]] std::string_view name() const override
        {
            return "ipo-sparse-conditional-constant-propagation";
        }

        [[nodiscard]] std::string_view description() const override
        {
            return "performs interprocedural sparse conditional constant propagation";
        }

        [[nodiscard]] const std::type_info& blm_id() const override
        {
            return typeid(IPOSCCPPass);
        }

        [[nodiscard]] std::vector<const std::type_info*> required_passes() const override
        {
            return get_pass_types<CallGraphAnalysisPass>();
        }

    private:
        /**
         * @brief Initialize lattice values for all nodes
         * @param modules All modules being analyzed
         * @param result SCCP result to populate
         */
        static void initialize_lattice(std::vector<Module*>& modules, IPOSCCPResult& result);

        /**
         * @brief Run the main SCCP worklist algorithm
         * @param modules All modules being analyzed
         * @param call_graph Call graph for interprocedural analysis
         * @param result SCCP result to populate
         */
        void run_sccp(std::vector<Module*>& modules,
                               const CallGraph& call_graph,
                               IPOSCCPResult& result);

        /**
         * @brief Apply constant folding transformations based on SCCP results
         * @param module Module to transform
         * @param result SCCP analysis results
         * @return Number of nodes replaced
         */
        static std::size_t apply_constant_folding(Module& module, const IPOSCCPResult& result);

        /**
         * @brief Replace a node with a literal, updating all uses
         * @param old_node Node to replace
         * @param new_literal Literal to replace with
         * @return True if replacement was successful
         */
        static bool replace_node_with_literal(Node *old_node, Node *new_literal);

        /**
         * @brief Process a single node during SCCP
         * @param node Node to process
         * @param result SCCP result
         * @param worklist Worklist to add affected nodes to
         * @param call_graph Call graph for interprocedural analysis
         */
        void process_node(Node* node, IPOSCCPResult& result, std::queue<Node*>& worklist, const CallGraph& call_graph);

        /**
         * @brief Handle interprocedural effects of function calls
         * @param call_node CALL or INVOKE node
         * @param call_graph Call graph for context
         * @param result SCCP result
         * @param worklist Worklist to add affected nodes to
         */
        void handle_function_call(Node* call_node, const CallGraph& call_graph,
                                 IPOSCCPResult& result, std::queue<Node*>& worklist);

        /**
         * @brief Propagate constant arguments to function parameters
         * @param call_node The call site
         * @param callee_func The function being called
         * @param result SCCP result
         * @param worklist Worklist to add affected nodes to
         */
        static void propagate_arguments(Node* call_node, Node* callee_func,
                                IPOSCCPResult& result, std::queue<Node*>& worklist);

        /**
         * @brief Propagate return values back to call sites
         * @param return_node RET node
         * @param caller_func Function containing the return
         * @param call_graph Call graph for finding call sites
         * @param result SCCP result
         * @param worklist Worklist to add affected nodes to
         */
        static void propagate_return_value(Node* return_node, Node* caller_func,
                                   const CallGraph& call_graph,
                                   IPOSCCPResult& result, std::queue<Node*>& worklist);

        /**
         * @brief Meet operation for lattice values
         * @param a First lattice value
         * @param b Second lattice value
         * @return Meet of a and b
         */
        [[nodiscard]] static LatticeValue meet(const LatticeValue& a, const LatticeValue& b);

        /**
         * @brief Update lattice value and add users to worklist if changed
         * @param node Node to update
         * @param new_value New lattice value
         * @param result SCCP result
         * @param worklist Worklist to add users to
         * @return True if value changed
         */
        static bool update_lattice_value(Node* node, const LatticeValue& new_value,
                                 IPOSCCPResult& result, std::queue<Node*>& worklist);

        /**
         * @brief Evaluate a node with constant inputs
         * @param node Node to evaluate
         * @param result SCCP result for getting input values
         * @return Computed lattice value
         */
        [[nodiscard]] static LatticeValue evaluate_node(Node* node, const IPOSCCPResult& result);

        /**
         * @brief Evaluate arithmetic operations (+, -, *, /, %)
         */
        [[nodiscard]] static LatticeValue evaluate_arithmetic(Node* node, const std::vector<LatticeValue>& inputs);

        /**
         * @brief Evaluate comparison operations (<, <=, >, >=, ==, !=)
         */
        [[nodiscard]] static LatticeValue evaluate_comparison(Node* node, const std::vector<LatticeValue>& inputs);

        /**
         * @brief Evaluate bitwise operations (&, |, ^, <<, >>)
         */
        [[nodiscard]] static LatticeValue evaluate_bitwise(Node* node, const std::vector<LatticeValue>& inputs);

        /**
         * @brief Evaluate unary operations (~)
         */
        [[nodiscard]] static LatticeValue evaluate_unary(Node* node, const std::vector<LatticeValue>& inputs);

        /**
         * @brief Evaluate branch operations (NodeType::BRANCH)
         */
        [[nodiscard]] static LatticeValue evaluate_branch(const std::vector<LatticeValue> &inputs);

        /**
         * @brief Find function parameters in a function's region
         * @param func_node Function node
         * @param modules All modules
         * @return Vector of parameter nodes in declaration order
         */
        [[nodiscard]] static std::vector<Node*> find_function_parameters(Node* func_node,
                                                                  std::vector<Module*>& modules);

        /**
         * @brief Find return nodes in a function
         * @param func_node Function node
         * @param modules All modules
         * @return Vector of return nodes
         */
        [[nodiscard]] static std::vector<Node*> find_return_nodes(Node* func_node,
                                                           std::vector<Module*>& modules);

        /**
         * @brief Find the region for a function
         * @param func Function node
         * @param modules All modules
         * @return Function's region or nullptr
         */
        [[nodiscard]] static Region* find_function_region(Node* func, std::vector<Module*>& modules);

        /**
         * @brief Create a literal node from a lattice value (reuses constfold logic)
         * @param lattice_val Constant lattice value
         * @param module Module to create literal in
         * @return Literal node or nullptr
         */
        [[nodiscard]] static Node* create_literal_from_lattice(const LatticeValue& lattice_val, Module& module);

        std::unordered_set<Node*> processed_calls; /* prevent infinite recursion */
    };
}
