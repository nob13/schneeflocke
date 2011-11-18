#pragma once

#include "DirectoryListing.h"
#include <sfserialization/autoreflect.h>

namespace sf {

/**
 * Represents a recursive directory listing (generated by special $GLOB file)
 */
struct RecursiveDirectoryListing {
	struct RecursiveEntry;
	typedef std::vector<RecursiveEntry> RecursiveEntryVec;
	typedef DirectoryListing::Entry Entry;
	
	struct RecursiveEntry : public Entry {
		/// Empty Entry initialization
		RecursiveEntry () : Entry () {}
		/// Initialize from a regular entry
		RecursiveEntry (const Entry & e) : Entry (e) {}
		
		RecursiveEntryVec entries;	///< Sub entries
		
		SF_AUTOREFLECT_SD;
		
		/// Calculates the sum of the size of all files (recursive)
		int64_t sizeSum () const;
	};
	
	RecursiveEntryVec entries;
	
	SF_AUTOREFLECT_SD;

	class const_iterator;
	const_iterator begin() const { return const_iterator (entries);}
	const_iterator end  () const { return const_iterator (); }
	
	/// Calculates the sum of all files (recursive!);
	int64_t sizeSum () const;
	
	/// Iterator for walking through all files
	class const_iterator {
	public:
		typedef RecursiveEntryVec::const_iterator SubIterator;
		struct StackEntry {
			StackEntry () {}
			StackEntry (SubIterator _pos, SubIterator _end) : pos (_pos), end (_end) {}
			SubIterator pos;
			SubIterator end;
		};
		
		/// Empty stack
		const_iterator ();
		const_iterator (const RecursiveEntryVec & entries);
		
		const_iterator& operator++ ();
	
		bool equals (const const_iterator &b) const {
			if (mStack.empty() && b.mStack.empty()) return true;
			if (mStack.empty() != b.mStack.empty()) return false;
			return mStack == b.mStack;
		}
		
		const RecursiveEntry& operator* () const { return *(mStack.back().pos); }
		const RecursiveEntry * operator-> () const { return &*(mStack.back().pos); }

		/// Returns path of the current element
		/// In slash notation
		const String path () const;
	private:
		
		typedef std::vector<StackEntry> IteratorStack; // no std::stack because path() needs to traverse it
		IteratorStack mStack;
	};
};

inline bool operator== (const RecursiveDirectoryListing::const_iterator::StackEntry & a, const RecursiveDirectoryListing::const_iterator::StackEntry & b){
	return (a.pos == b.pos && a.end == b.end);
}

inline bool operator== (const RecursiveDirectoryListing::const_iterator & a, const RecursiveDirectoryListing::const_iterator & b){
	return a.equals(b);
}

inline bool operator!= (const RecursiveDirectoryListing::const_iterator & a, const RecursiveDirectoryListing::const_iterator & b){
	return ! (operator== (a,b));
}

typedef shared_ptr<RecursiveDirectoryListing> RecursiveDirectoryListingPtr;

}
