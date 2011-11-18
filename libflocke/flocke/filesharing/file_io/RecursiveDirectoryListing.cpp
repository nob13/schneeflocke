#include "RecursiveDirectoryListing.h"
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

namespace sf {

static int64_t sizeSum (const RecursiveDirectoryListing::RecursiveEntryVec & entries){
	int64_t sum = 0;
	foreach (const RecursiveDirectoryListing::RecursiveEntry & entry, entries){
		if (entry.type == DirectoryListing::File){
			sum+=entry.size;
		} else {
			sum+=entry.sizeSum ();
		}
	}
	return sum;
}

int64_t RecursiveDirectoryListing::RecursiveEntry::sizeSum () const {
	return sf::sizeSum (entries);
}

int64_t RecursiveDirectoryListing::sizeSum () const {
	return sf::sizeSum (entries);
}


RecursiveDirectoryListing::const_iterator::const_iterator () {
	
}

RecursiveDirectoryListing::const_iterator::const_iterator (const RecursiveEntryVec & entries) {
	if (entries.empty()) return;
	mStack.push_back (StackEntry (entries.begin(), entries.end()));
}


	
RecursiveDirectoryListing::const_iterator& RecursiveDirectoryListing::const_iterator::operator++ () {
	if (mStack.empty()) return *this;
	{
		StackEntry & current = mStack.back ();
		assert (current.pos != current.end);
		// One step further
		if (current.pos->type == DirectoryListing::Directory){
			mStack.push_back (StackEntry (current.pos->entries.begin(), current.pos->entries.end()));
		} else {
			++current.pos;
		}
	}
	while (true) {
		if (mStack.empty()) return *this;
		StackEntry & current = mStack.back();
		if (current.pos == current.end){
			mStack.pop_back();
			if (mStack.empty()) return *this;
			++mStack.back().pos;
			continue;
		}
		break;
	}
	return *this;
}

const String RecursiveDirectoryListing::const_iterator::path () const {
	bool first = true;
	String result; result.reserve (128);
	foreach (const StackEntry & entry, mStack){
		if (!first) result.append("/");
		result.append (entry.pos->name);
		first = false;
	}
	return result;
}

	
}
