/* this project is part of the bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/foundation/region.hpp>

namespace blm
{
	inline bool is_global_scope(const Region* region)
	{
		if (!region)
			return false;
		return region->get_parent() == nullptr;
	}
}
