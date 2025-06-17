/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#include <bloom/support/string-table.hpp>

namespace blm
{
	StringTable::StringTable() : next_id(1)
	{
		strs.emplace_back("");
		strtid[""] = 0;
	}

	StringTable::StringId StringTable::intern(std::string_view str)
	{
		/* all empty strings are the same */
		if (str.empty())
			return 0;

		/* if already exists */
		if (const auto it = strtid.find(std::string(str));
			it != strtid.end())
		{
			return it->second;
		}

		const StringId nid = next_id++;
		strs.emplace_back(str);
		strtid[std::string(str)] = nid;
		return nid;
	}

	std::string_view StringTable::get(const StringId id) const
	{
		if (id >= strs.size())
			return {}; /* invalid id */
		return strs[id];
	}

	bool StringTable::contains(std::string_view str) const
	{
		return strtid.contains(std::string(str));
	}

	std::size_t StringTable::size() const
	{
		return strs.size();
	}

	void StringTable::clear()
	{
		strs.clear();
		strtid.clear();

		strs.emplace_back("");
		strtid[""] = 0;

		next_id = 1;
	}
}
