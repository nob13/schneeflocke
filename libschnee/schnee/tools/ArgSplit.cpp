#include "ArgSplit.h"
#include <sstream>
#include <stdlib.h>
#include <assert.h>

#include <schnee/schnee.h>
#include <schnee/settings.h>

#ifndef WIN32
extern "C"{
#include <linenoise/linenoise.h>
}
#endif
#include <iostream>

namespace sf {

void argSplit (const std::string & input, ArgumentList * args) {
	assert (args);
	args->clear();
	if (input.empty()) return;
	bool quoted = false;
	bool escaped   = false;
	size_t current = 0;
	size_t length  = input.length();
	std::ostringstream build;
	for (;current < length; current++){
		char c = input[current];
		if (c == ' ' && !quoted){
			if (!build.str().empty()) {
				args->push_back (build.str());
			}
			build.str("");
			build.clear();
		} else
		if (c == '\"' && !escaped){
			quoted = !quoted;
		} else
		if (c == '\"' && escaped){
			build << "\"";
			escaped = false;
		} else
		if (c == '\\' && !escaped){
			escaped = true;
		} else
		if (c == '\\' && escaped){
			build << "\\";
			escaped = false;
		} else {
			build << c;
			escaped = false;
		}
	}
	if (!build.str().empty()){
		args->push_back(build.str());
	}
}

bool nextCommand (sf::ArgumentList * parts){
	std::string line;
#ifdef WIN32
	// C++ Style line entering, not so nice
	std::cout << "> ";
	std::getline (std::cin, line);
#else
	if (schnee::settings().noLineNoise){
		std::cout << "> ";
		std::getline (std::cin, line);
	} else {
	// More comfortable line entering
		char * cLine = ::linenoise ("> ");
		if (!cLine) { return false; } // Ctrl+C
		::linenoiseHistoryAdd (cLine);
		line = cLine;
		::free (cLine);
	}
#endif
	sf::argSplit (line, parts);
	return true;
}

}
