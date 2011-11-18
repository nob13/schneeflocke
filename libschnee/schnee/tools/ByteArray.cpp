#include "ByteArray.h"
#include <assert.h>
#include <string.h>
#include <schnee/tools/Serialization.h>
#include <sstream>

namespace sf {

bool ByteArray::printable () const {
	for (const_iterator i = begin(); i != end(); i++){
		unsigned char c = (unsigned char) *i;
		if (c == '\t' || c == '\n' || c == '\r') continue; // also displayable
		if (c == 0x0 && (i+1) == end()) continue; // one terminating is allowed
		if (c < 0x20) return false;
		
	}
	return true;
}

void ByteArray::l_truncate (size_t pos) {
	// this function takes a very long time, what we really would need is a ring buffer.
	// old code (95% in callgrind!!)
//	size_t l = size ();
//	assert (pos <= l);
//	size_t r = l - pos;
//
//	for (size_t i = 0; i < r; i++){
//		this->operator[](i) = this->operator[] (pos+i);
//	}
//	resize (r);

	size_t before = size();
	size_t after  = before - pos;
	assert (pos <= before);

	if (pos == before) { clear (); return; }

	char * dst = &(*begin());
	const char * src = dst + pos;

	::memmove (dst, src, after);
	resize (after);
}

void serialize (sf::Serialization & s, const ByteArray & array){
	std::ostringstream stream;
	stream << array;
	s.insertStringValue (stream.str().c_str());
}

}

/// Output operator for ByteArray
std::ostream & operator<< (std::ostream & stream, const sf::ByteArray & array) {
	if (!array.printable()){
		return stream << "Bytearray (nonprintable, size=" << array.size() << ")" << std::endl;		
	}
	
	for (sf::ByteArray::const_iterator i = array.begin(); i != array.end(); i++){
		stream << *i;
	}
	return stream;
}
