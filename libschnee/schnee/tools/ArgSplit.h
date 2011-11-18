#pragma once

#include <string>
#include <vector>


namespace sf {

typedef std::vector<std::string> ArgumentList;

/**
 * Easy to use command argument splitter with escaping functionality.
 *
 * Used in the command line demonstration programs.
 */
void argSplit (const std::string & input, ArgumentList * args);

/**
 * Let the user enters a command. The command line starts with '> '.
 * In windows it is realized through iostream, unix is using
 * the more comfortable linenoise library.
 *
 * @return true if users enters something, false if user enters Ctrl+D or similar.
 */
bool nextCommand (sf::ArgumentList * parts);

}
