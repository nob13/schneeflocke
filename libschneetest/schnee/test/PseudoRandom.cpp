#include "PseudoRandom.h"

#include <boost/random.hpp>

namespace sf {
namespace test {

int pseudoRandom (int max){
	static boost::rand48 rand48;
	boost::uniform_smallint<> dist (0, max - 1);
	return dist (rand48);
}

void pseudoRandomData (size_t byteCount, char * target){
	static boost::rand48 rand48;
	boost::uniform_smallint<> dist (0, 255);
	for (size_t i = 0; i < byteCount; i++){
		*(target + i) = dist (rand48);
	}
}

struct PseudoRandomHolder {
	PseudoRandomHolder (int seed) : rand48 (seed), dist (0, 255) {}
	boost::rand48 rand48;
	boost::uniform_smallint<> dist;
};

PseudoRandomData::PseudoRandomData (int id) {
	impl = new PseudoRandomHolder (id);
}

void PseudoRandomData::next (size_t byteCount, char * target) {
	PseudoRandomHolder * h = static_cast<PseudoRandomHolder*>( impl);
	for (size_t i = 0; i < byteCount; i++){
		*(target + i) = h->dist (h->rand48);
	}
}

PseudoRandomData::~PseudoRandomData () {
	delete static_cast<PseudoRandomHolder*> (impl);
}

}
}
