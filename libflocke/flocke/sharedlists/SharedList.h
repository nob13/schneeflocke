#pragma once
#include <schnee/sftypes.h>
#include <schnee/p2p/DataSharingElements.h>
#include <sfserialization/autoreflect.h>

namespace sf {

struct SharedElement {
	SharedElement (const Path & _path = Path(), int64_t _size = 0, const ds::DataDescription & _desc = ds::DataDescription()) 
	: path (_path), size (_size), desc (_desc) {}

	sf::Path    path;				///< Path of the element
	int64_t size;					///< Size of the element
	sf::ds::DataDescription desc;	///< Additional element description

	bool operator!= (const SharedElement& other) const {
		return path != other.path ||
				size != other.size ||
				!(desc == other.desc);
	}
	
	SF_AUTOREFLECT_SD;
};

/// Maps human readable name (file name) to SharedElement
typedef std::map<sf::String, SharedElement> SharedList;

}
