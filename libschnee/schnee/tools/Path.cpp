#include "Path.h"
#include <schnee/tools/Serialization.h>
#include <schnee/tools/Deserialization.h>
#include "FileTools.h"

namespace sf {

std::string Path::toSystemPath () const {
	String toUse = mPath;
#ifdef WIN32
	boost::replace_all (toUse, "/", "\\");
#endif
	return toUse;
}

std::string Path::head () const {
	return mPath.substr (0, mPath.find ('/'));
}

std::string Path::tail() const {
	size_t p = mPath.rfind ('/');
	if (p == mPath.npos)
		return mPath;
	else
		return mPath.substr (p + 1);
}

Path Path::subPath () const {
	size_t p = mPath.find ('/'); 
	if (p == mPath.npos) 
		return ""; 
	else 
		return mPath.substr(p + 1,mPath.npos);
}

void Path::shrink () {
	boost::replace_all (mPath, "//", "/");
}

void Path::removeTrailingSlashes() {
	size_t c = 0;
	for (std::string::reverse_iterator i = mPath.rbegin(); i != mPath.rend() && *i == '/'; i++){
		c++;
	}
	mPath.resize (mPath.size() - c);
}

void Path::normalize () {
	shrink();
	removeTrailingSlashes();
	while (mPath.size() >= 2 && mPath[mPath.size() - 2] == '/' && mPath[mPath.size() - 1] == '.'){
		mPath.resize (mPath.size() - 2);
	}
}


void serialize (sf::Serialization & s, const Path & path){
	s.insertStringValue (path.toString().c_str());
}

bool deserialize (const json::Value & v, Path & path) {
	std::string s;
	if (!v.fetch (s)) return false;
	path = Path (s);
	return true;
}

}

std::ostream & operator<< (std::ostream & ostream, const sf::Path & path) {
	return ostream << path.toString();
}
