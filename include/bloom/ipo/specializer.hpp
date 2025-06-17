/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <bloom/foundation/context.hpp>
#include <bloom/foundation/module.hpp>
#include <bloom/foundation/node.hpp>
#include <bloom/foundation/region.hpp>
#include <bloom/foundation/typed-data.hpp>

namespace blm
{
	/**
	 * @brief Represents a lattice value for SCCP analysis
	 */
	struct LatticeValue
	{
		enum class State : std::uint8_t
		{
			TOP,      /* undefined/unknown */
			CONSTANT, /* known constant value */
			BOTTOM    /* not constant; multiple values possible */
		} state = State::TOP;

		TypedData value; /* only valid when state == CONSTANT */

		/**
		 * @brief Create a TOP lattice value
		 */
		LatticeValue() = default;

		/**
		 * @brief Create a CONSTANT lattice value
		 */
		template<typename T, DataType DT>
		static LatticeValue make_constant(T &&val)
		{
			LatticeValue lv;
			lv.state = State::CONSTANT;
			lv.value.set<T, DT>(std::forward<T>(val));
			return lv;
		}

		/**
		 * @brief Create a CONSTANT lattice value
		 */
		template<typename T, DataType DT>
		static LatticeValue make_constant(const T& val)
		{
			LatticeValue lv;
			lv.state = State::CONSTANT;
			lv.value.set<T, DT>(val);
			return lv;
		}

		/**
		 * @brief Create a BOTTOM lattice value
		 */
		static LatticeValue make_bottom()
		{
			LatticeValue lv;
			lv.state = State::BOTTOM;
			return lv;
		}

		/**
		 * @brief Check if this value is constant
		 */
		[[nodiscard]] bool is_constant() const
		{
			return state == State::CONSTANT;
		}

		/**
		 * @brief Check if this value is top
		 */
		[[nodiscard]] bool is_top() const
		{
			return state == State::TOP;
		}

		/**
		 * @brief Check if this value is bottom
		 */
		[[nodiscard]] bool is_bottom() const
		{
			return state == State::BOTTOM;
		}
	};

	/**
	 * @brief Utility for function specialization across IPO passes
	 */
	class FunctionSpecializer
	{
	public:
		/**
		 * @brief Request for function specialization
		 */
		struct SpecializationRequest
		{
			/** @brief Original function to specialize */
			Node *original_function = nullptr;
			/** @brief Specialized parameters; param_index, constant_value */
			std::vector<std::pair<std::size_t, LatticeValue> > specialized_params;
			/** @brief Call sites that would benefit from this specialization */
			std::vector<Node *> call_sites;
			/** @brief Estimated benefit score for this specialization */
			double benefit_score = 0.0;

			/**
			 * @brief Get the number of parameters that are constant
			 */
			[[nodiscard]] std::size_t constant_parameter_count() const
			{
				return specialized_params.size();
			}

			/**
			 * @brief Check if a parameter index is being specialized
			 */
			[[nodiscard]] bool is_specialized_parameter(std::size_t param_idx) const
			{
				return std::any_of(specialized_params.begin(), specialized_params.end(),
				                   [param_idx](const auto &pair)
				                   {
					                   return pair.first == param_idx;
				                   });
			}

			/**
			 * @brief Get the constant value for a specialized parameter
			 */
			[[nodiscard]] const LatticeValue *get_specialized_value(std::size_t param_idx) const
			{
				auto it = std::find_if(specialized_params.begin(), specialized_params.end(),
				                       [param_idx](const auto &pair)
				                       {
					                       return pair.first == param_idx;
				                       });
				return (it != specialized_params.end()) ? &it->second : nullptr;
			}
		};

		/**
		 * @brief Construct a new Function Specializer
		 */
		FunctionSpecializer() = default;

		/**
		 * @brief Create specialized version of function with constant args
		 * @param req The specialization request
		 * @param target_module Module to create the specialized function in
		 * @return Pointer to the specialized function, or nullptr on failure
		 */
		Node *specialize_function(const SpecializationRequest &req, Module &target_module);

		/**
		 * @brief Update call sites to use specialized version
		 * @param req The original specialization request for parameter info
		 * @param call_sites Call sites to redirect
		 * @param specialized_func The specialized function to redirect to
		 * @return Number of call sites successfully redirected
		 */
		static std::size_t redirect_call_sites(const SpecializationRequest &req,
		                                const std::vector<Node *> &call_sites,
		                                Node *specialized_func);

		/**
		 * @brief Check if specialization is profitable
		 * @param req The specialization request to evaluate
		 * @return True if specialization should be performed
		 */
		[[nodiscard]] bool should_specialize(const SpecializationRequest &req) const;

		/**
		 * @brief Calculate benefit score for a specialization request
		 * @param req The specialization request
		 * @return Benefit score (higher = more beneficial)
		 */
		[[nodiscard]] static double calculate_benefit_score(const SpecializationRequest &req) ;

		/**
		 * @brief Set specialization heuristics
		 */
		void set_max_call_sites(std::size_t max)
		{
			max_call_sites = max;
		}

		void set_max_function_size(std::size_t max)
		{
			max_function_size = max;
		}

		void set_min_benefit_threshold(double threshold)
		{
			min_benefit_threshold = threshold;
		}

	private:
		/**
		 * @brief Clone function structure without constant substitution
		 * @param original Original function to clone
		 * @param target_module Module to create the clone in
		 * @param specialized_params Specialized parameters
		 * @return Cloned function node, or nullptr on failure
		 */
		static Node *clone_function_skeleton(Node *original, Module &target_module, const std::vector<std::pair<std::size_t, LatticeValue>>& specialized_params);

		/**
		 * @brief Clone region hierarchy for a function
		 * @param original_region Original region to clone
		 * @param target_module Module to create regions in
		 * @param node_mapping Mapping from original nodes to cloned nodes
		 * @return Cloned region, or nullptr on failure
		 */
		Region *clone_region_hierarchy(const Region *original_region, Module &target_module,
		                               std::unordered_map<Node *, Node *> &node_mapping);

		/**
		 * @brief Clone a single node
		 * @param original Original node to clone
		 * @param target_module Module to create the node in
		 * @return Cloned node, or nullptr on failure
		 */
		static Node *clone_node(Node *original, Module &target_module);

		/**
		 * @brief Substitute parameters with constant values in cloned function
		 * @param cloned_region The cloned function's region
		 * @param specialized_params Parameters to specialize with their constant values
		 */
		static void substitute_parameters_with_constants(Region *cloned_region,
		                                                 const std::vector<std::pair<std::size_t, LatticeValue>> &specialized_params);

		/**
		 * @brief Fix up input/user relationships after cloning
		 * @param original_region Original region
		 * @param cloned_region Cloned region
		 * @param node_mapping Mapping from original to cloned nodes
		 */
		void fixup_node_connections(const Region *original_region, Region *cloned_region,
		                            const std::unordered_map<Node *, Node *> &node_mapping);

		/**
		 * @brief Generate a unique name for the specialized function
		 * @param original_func Original function
		 * @param specialized_params Specialized parameters
		 * @param ctx Context for string interning
		 * @return String ID for the specialized function name
		 */
		static StringTable::StringId generate_specialized_name(Node *original_func,
		                                                       const std::vector<std::pair<std::size_t, LatticeValue> >
		                                                       &
		                                                       specialized_params,
		                                                       Context &ctx);

		/**
		 * @brief Estimate the size of a function
		 * @param func Function to estimate
		 * @param modules All modules
		 * @return Estimated size in nodes
		 */
		[[nodiscard]] static std::size_t estimate_function_size(Node *func, const std::vector<Module *> &modules);

		/**
		 * @brief Find the region corresponding to a function
		 * @param func Function to find region for
		 * @param modules All modules to search
		 * @return Function's region, or nullptr if not found
		 */
		[[nodiscard]] static Region *find_function_region(Node *func, const std::vector<Module *> &modules);

		/**
		 * @brief Compute a unique key for specialization request
		 * @param func Original function
		 * @param specialized_params Parameters being specialized
		 * @return Unique key for this specialization
		 */
		static std::uint64_t compute_specialization_key(Node* func,
												const std::vector<std::pair<std::size_t, LatticeValue>>& specialized_params);

		std::unordered_map<std::uint64_t, Node*> specialization_cache;
		std::size_t max_call_sites = 8;
		std::size_t max_function_size = 100;
		double min_benefit_threshold = 2.0;
		std::size_t min_constant_args = 1;
	};
}
