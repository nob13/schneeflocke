#include "Uri.h"

#include <string.h>

#include <schnee/tools/Log.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/Deserialization.h>

namespace sf {

Uri::Uri(const char * init){
	set (init);
}

Uri::Uri (const std::string & init){
	set (init.c_str());
}

Uri::Uri (const std::string & host, const Path & path) {
	set (host.c_str(), path);
}

bool Uri::set (const char * init) {
	mHost.clear ();
	mPath.clear();
	if (!init) { mValid = false; return false;}
	while (*init == ' ') init++;
	// does it begin with sf:/  then strip it.
	int len = strlen (init);
	if (len >= 3 && strncmp (init, "sf:", 3) == 0) {
		init += 3;
		len  -= 3;
	}
	// is there a ":"
	const char * part = strchr (init, ':');
	if (part == NULL){
		// no host part given
		mPath.assign (init);
		mValid = true;
		return true;
	}
	mHost.assign(init, part);
	mPath.assign (part + 1);
	mValid = true;
	return true;
}

bool Uri::set (const char * host, const Path & path) {
	if (!host) { 
		sf::Log (LogError) << LOGID << "Invalid values" << std::endl;
		mHost.clear ();
		mPath.clear();
		mValid = false;
		return false;
	}
	if (strchr (host, ':') != NULL){
		sf::Log (LogError) << LOGID << "The host " << host << " may not cantain any ':'" << std::endl;
		mValid = false;
		return false;
	}
	mHost  = host;
	mPath  = path;
	mValid = true;
	return true;
}

void serialize (sf::Serialization & s, const Uri & uri){
	s.insertStringValue (uri.toString().c_str());
}

bool deserialize (const json::Value & v, Uri & uri) {
	std::string s;
	if (!v.fetch (s)) return false;
	uri = Uri (s);
	return uri.valid() || uri.isDefault(); // empty uri is also correct deserialized
}


}

std::ostream & operator<< (std::ostream & ostream, const sf::Uri & uri) {
	return ostream << uri.toString ();
}
