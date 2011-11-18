#pragma once

#include <set>
#include <assert.h>
#include <stddef.h>

namespace sf {
namespace test {

/// Generates a pseudeo random number (shall be the same on each call!)
int pseudoRandom (int max);

/// Generates a pseudo random data block, target must be allocated already 
void pseudoRandomData (size_t byteCount, char * target);

/// Generates a pseudo random data block, target must be allocated already; same id will have same data
void pseudoRandomData (size_t byteCount, char * target, int id);

/// Generates a the same pseudo random data again and again
class PseudoRandomData {
public:
	PseudoRandomData (int id);
	~PseudoRandomData ();
	void next (size_t byteCount, char * target);
private:
	PseudoRandomData (PseudoRandomData & );
	PseudoRandomData & operator= (const PseudoRandomData&);
	void * impl;
};

/// Gives you a random element of the set (not fast!)
template <class T> T pseudoRandomElement (std::set<T> set) {
	size_t size = set.size();
	assert (size > 0);
	int num = pseudoRandom (size);
	typename std::set<T>::const_iterator i = set.begin();
	std::advance(i, num);
	return *i;
}


}
}
