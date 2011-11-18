#pragma once

#include <string>

namespace sf {
class Serialization;
namespace json {
class Value;
}

/// A '/' delimited path used inside libschnee
class Path {
public:
	Path () { }
	Path (const Path & p) : mPath (p.mPath) {}
	Path (const std::string & src) : mPath (src) {}
	Path (const char * src) : mPath (src) {}
	bool operator== (const Path & p) const { return mPath == p.mPath; }
	bool operator!= (const Path & p) const { return mPath != p.mPath; }
	bool operator< (const Path & p) const { return mPath < p.mPath; }
	
	/// Converts path to a string
	const std::string & toString () const { return mPath; }
	
	/// Converts pahh to a system path
	std::string toSystemPath () const;
	
	/// Returns the part of the path before the first '/'
	std::string head () const;
	
	/// isDefault state
	bool isDefault () const { return mPath.empty(); }
	
	/// empty operator
	bool empty() const { return mPath.empty(); }
	
	/// Returns the part of the path after the last '/'
	std::string tail () const;
	
	/// Returns the part of the path after the first '/'
	Path subPath () const;
	
	/// Returns true if path has a sub path
	bool hasSubPath () const { return mPath.find ('/') != mPath.npos; }
	
	/// clears the path (set to "")
	void clear () { mPath.clear(); }
	
	/// Assigns a new path
	void assign (const char * path) { mPath.assign (path);}

	/// Replaces occurances of '//' with '/' inside the path
	void shrink ();
	
	/// Remove trailing slashes ('/' at the end)
	void removeTrailingSlashes();
	
	/// Shrink and removeTrailingSlashes
	/// Also removes trailing /.
	void normalize ();
private:
	std::string mPath;
};

// Serialization operator for path
void serialize (sf::Serialization & s, const Path & uri);
// Deserialization operator for path
bool deserialize (const json::Value & v, Path & uri);

/// Adds two paths
inline Path operator+ (const Path & a, const Path & b) {
	Path out (a.toString() + '/' + b.toString());
	out.shrink();
	return out;
}

}

std::ostream & operator<< (std::ostream & ostream, const sf::Path & path);

