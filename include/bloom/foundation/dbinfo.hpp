/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>
#include <bloom/support/string-table.hpp>

namespace blm
{
	struct Node;
	class Region;

	/**
	 * @brief Debug record types
	 */
	enum class DebugRecordKind : std::uint8_t
	{
		/** @brief Source location information */
		LOCATION,
		/** @brief Variable information */
		VARIABLE,
		/** @brief Function information */
		FUNCTION,
		/** @brief Type information */
		TYPE
	};

	/**
	 * @brief Source location structure
	 */
	struct SourceLocation
	{
		/** @brief ID of the source file */
		StringTable::StringId file_id;
		/** @brief Line number */
		std::uint32_t line;
		/** @brief Column number */
		std::uint32_t column;

		SourceLocation() = default;

		SourceLocation(const StringTable::StringId file, const std::uint32_t l, std::uint32_t c = 0) : file_id(file),
			line(l), column(c) {}

		auto operator<=>(const SourceLocation &) const = default;
	};

	/**
	 * @brief Comprehensive debug information container
	 */
	class DebugInfo
	{
	public:
		/**
		 * @brief Constructor
		 *
		 * @param region The region that owns this debug info
		 */
		explicit DebugInfo(Region &region);

		/**
		 * @brief Get the region that owns this debug info
		 */
		Region &get_region() const
		{
			return region;
		}

		/**
		 * @brief Register a source file
		 *
		 * @param path Path to the source file
		 * @return StringTable::StringId ID of the registered file
		 */
		StringTable::StringId add_source_file(std::string_view path);

		/**
		 * @brief Get the path of a source file
		 *
		 * @param file_id ID of the source file
		 * @return std::string_view Path to the source file
		 */
		std::string_view get_source_file(StringTable::StringId file_id) const;

		/**
		 * @brief Associate a node with a source location
		 *
		 * @param node The node to associate
		 * @param file_id ID of the source file
		 * @param line Line number
		 * @param column Column number (optional)
		 */
		void set_node_location(Node *node, StringTable::StringId file_id,
		                       std::uint32_t line, std::uint32_t column = 0);

		/**
		 * @brief Get the source location of a node
		 *
		 * @param node The node to query
		 * @return std::optional<SourceLocation> The source location, if available
		 */
		std::optional<SourceLocation> get_node_location(Node *node) const;

		/**
		 * @brief Find nodes at a specific source location
		 *
		 * @param file_id ID of the source file
		 * @param line Line number
		 * @param column Column number (optional)
		 * @return std::vector<Node*> Nodes at the specified location
		 */
		std::vector<Node *> find_nodes_at_location(StringTable::StringId file_id,
		                                           std::uint32_t line,
		                                           std::uint32_t column = 0) const;

		/**
		 * @brief Register a variable
		 *
		 * @param node The node representing the variable
		 * @param name Name of the variable
		 * @param type_name Type name of the variable
		 * @param is_param Whether the variable is a parameter
		 * @param frame_offset Frame offset (if known)
		 */
		void add_variable(Node *node, std::string_view name, std::string_view type_name,
		                  bool is_param = false, std::int32_t frame_offset = 0);

		/**
		 * @brief Get variable information
		 *
		 * @param node The node representing the variable
		 * @return std::tuple<std::string_view, std::string_view, bool, std::int32_t>
		 *         Tuple of (name, type_name, is_param, frame_offset)
		 */
		std::tuple<std::string_view, std::string_view, bool, std::int32_t> get_variable_info(Node *node) const;

		/**
		 * @brief Register a function
		 *
		 * @param node The node representing the function
		 * @param name Name of the function
		 */
		void add_function(Node *node, std::string_view name);

		/**
		 * @brief Add a parameter to a function
		 *
		 * @param func_node The function node
		 * @param param_node The parameter node
		 */
		void add_parameter_to_function(Node *func_node, Node *param_node);

		/**
		 * @brief Add a local variable to a function
		 *
		 * @param func_node The function node
		 * @param var_node The variable node
		 */
		void add_local_var_to_function(Node *func_node, Node *var_node);

		/**
		 * @brief Get all parameters of a function
		 *
		 * @param func_node The function node
		 * @return std::vector<Node*> Parameter nodes
		 */
		std::vector<Node *> get_function_parameters(Node *func_node) const;

		/**
		 * @brief Get all local variables of a function
		 *
		 * @param func_node The function node
		 * @return std::vector<Node*> Local variable nodes
		 */
		std::vector<Node *> get_function_local_vars(Node *func_node) const;

		/**
		 * @brief Register a type
		 *
		 * @param name Name of the type
		 * @param size Size of the type in bytes (if known)
		 * @param alignment Alignment of the type in bytes (if known)
		 * @return StringTable::StringId ID of the registered type
		 */
		StringTable::StringId add_type(std::string_view name,
		                               std::uint32_t size = 0,
		                               std::uint32_t alignment = 0);

		/**
		 * @brief Get type information
		 *
		 * @param type_id ID of the type
		 * @return std::tuple<std::string_view, std::uint32_t, std::uint32_t>
		 *         Tuple of (name, size, alignment)
		 */
		std::tuple<std::string_view, std::uint32_t, std::uint32_t> get_type_info(StringTable::StringId type_id) const;

	private:
		Region &region;

		std::vector<StringTable::StringId> source_files;
		std::unordered_map<Node *, SourceLocation> node_locations;
		std::map<SourceLocation, std::vector<Node *> > location_to_nodes;

		struct VariableInfo
		{
			StringTable::StringId name_id;
			StringTable::StringId type_id;
			bool is_param;
			std::int32_t frame_offset;
		};

		std::unordered_map<Node *, VariableInfo> variables;

		struct FunctionInfo
		{
			StringTable::StringId name_id;
			std::vector<Node *> parameters;
			std::vector<Node *> local_vars;
		};

		std::unordered_map<Node *, FunctionInfo> functions;

		struct TypeInfo
		{
			StringTable::StringId name_id;
			std::uint32_t size;
			std::uint32_t alignment;
		};

		std::unordered_map<StringTable::StringId, TypeInfo> types;
	};
}
