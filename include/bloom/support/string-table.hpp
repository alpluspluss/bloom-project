/* this project is part of the Bloom Project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace blm
{
	/**
	 * @brief Concurrent sparse-dense table for string interning
	 */
	class StringTable
	{
	public:
		using StringId = std::uint64_t;
		static constexpr StringId INVALID_STRING_ID = std::numeric_limits<StringId>::max();

		/**
		 * @brief Constructor
		 */
		StringTable();

		/**
		 * @param str String to intern
		 * @return string index; invalid if equal `std::numeric_limits<StringId>::max();`
		 */
		StringId intern(std::string_view str);

		/**
		 * @param id String index that is produced from the table
		 * @return Read-only version of the initial string from the sparse-dense mapping
		 */
		std::string_view get(StringId id) const;

		/**
		 * @param str String to find
		 * @return `true` if found otherwise `false`
		 */
		bool contains(std::string_view str) const;

		/**
		 * @return Current size of the table
		 */
		std::size_t size() const;

		/**
		 * @brief Clear all data stored inside the table
		 */
		void clear();

	private:
		std::unordered_map<std::string, StringId> strtid;
		std::vector<std::string> strs;
		std::atomic<StringId> next_id;
	};
}
