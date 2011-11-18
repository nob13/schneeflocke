#include "DataSharingServer.h"

namespace sf {

void DataSharingServer::SharedDataDesc::serialize (Serialization & s) const {
	s ("currentRevision", currentRevision);
	s ("subscribers", subscribers);
}

void serialize (sf::Serialization & s, const sf::DataSharingServer::SharedDataDescMap & map) {
	for (DataSharingServer::SharedDataDescMap::const_iterator i = map.begin(); i != map.end(); i++) {
		sf::String key = i->first.toString();
		s (key.c_str(), i->second);
	}
}

}
