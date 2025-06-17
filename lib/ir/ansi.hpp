/* this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info */

#pragma once

#include <string>

namespace blm
{
	/* colors */
	constexpr std::string ANSI_RESET = "\033[0m";
	constexpr std::string ANSI_RED = "\033[31m";
	constexpr std::string ANSI_GREEN = "\033[32m";
	constexpr std::string ANSI_YELLOW = "\033[33m";
	constexpr std::string ANSI_BLUE = "\033[34m";
	constexpr std::string ANSI_CYAN = "\033[36m";
	constexpr std::string ANSI_WHITE = "\033[37m";

	/* bright colors */
	constexpr std::string ANSI_BRIGHT_GREEN = "\033[92m";
	constexpr std::string ANSI_BRIGHT_RED = "\033[91m";
	constexpr std::string ANSI_BRIGHT_YELLOW = "\033[93m";

	/* formatting */
	constexpr std::string ANSI_BOLD = "\033[1m";
	constexpr std::string ANSI_DIM = "\033[2m";

	/* tree symbols */
	constexpr std::string TREE_BRANCH = "├── ";
	constexpr std::string TREE_LAST = "└── ";
	constexpr std::string TREE_PIPE = "│   ";
	constexpr std::string TREE_SPACE = "    ";

	/* status symbols */
	constexpr std::string CHECK_MARK = "✓";
}
