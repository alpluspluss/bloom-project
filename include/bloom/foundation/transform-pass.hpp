/* this project is part of the bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <bloom/foundation/pass.hpp>

namespace blm
{
	/**
	 * @brief Base class for passes that transform the IR.
	 *
	 * Transform passes modify the IR, potentially invalidating analysis results.
	 * They implement the run() method directly from the Pass interface.
	 */
	class TransformPass : public Pass
	{
		/* TransformPass uses the run() method directly from Pass
		 *
		 * The run() method is defined in the Pass class, and it is responsible for
		 * executing the pass on the given module. The TransformPass class does not
		 * need to implement any additional methods or interfaces.
		 *
		 * The run() method will be called with a Module and PassContext, and it will
		 * perform the transformation on the module.
		 */
	};
}
