#pragma once 

#include <string>
#include <ostream>
#include "Path.h"

namespace sf {
class Serialization;

namespace json {
class Value;
}

/** Describes our common used Uniform Resource Identifier type,
 *  It shall be very simple
 *
 *	The layout is sf:Host:Path
 *  
 *  We do not use the slash '/' to differ between host and dataId because
 *  XMPP-Full-IDs contain already a slash.
 * 
 *	Host may be omitted, then you have to use the currnt context
 *	\"sf:\" may also be omitted
 *	
*/
class Uri {
public:
	Uri (const char * init = 0);
	Uri (const std::string & init);
	Uri (const std::string & host, const Path & path);
	
	/// Returns true on default Uri
	bool isDefault () const { return operator== (Uri()); }

	/// Set to a specific value
	/// (Valid: sf:Host/Id or Host/Id or /Id )
	/// @return true on success
	bool set (const char * init);
	
	/// Set to a specific host / id combination
	bool set (const char * host, const Path & path);
	
	// Uri is valid formed
	bool valid () const { return mValid; }

	/// Returns the host
	const std::string & host () const { return mHost; }
	
	/// Returns the path
	const Path & path () const { return mPath; }
	
	/// Writeable reference to the path
	Path & rpath() { return mPath; }

	/// Sets the host value
	void setHost (const std::string & host) { mHost = host; }
	
	/// Sets the host value - if current host is not set
	void setDefaultHost (const std::string & host) { if (mHost.empty()) mHost = host; }
	
	/// Returns the full formated URI
	std::string toString () const { return "sf:" + mHost + ":" + mPath.toString();}
	
	/// Comparison operator, to use it in std::set/std::map 
	bool operator< (const Uri & other) const {
		if (!mValid && other.mValid) return true;
		if (mValid == other.mValid){
			if (mHost < other.mHost) return true;
			if (mHost == other.mHost) {
				if (mPath < other.mPath) return true;
			}
		}
		return false;
	}
	
	/// Set-to operator
	Uri & operator= (const Uri & other) {
		mHost  = other.mHost;
		mPath  = other.mPath;
		mValid = other.mValid;
		return *this;
	}
	
	/// Equality operator
	bool operator== (const Uri & other) const {
		return mValid == other.mValid && mPath == other.mPath && mHost == other.mHost;
	}
	
	/// Unequality operator
	bool operator!= (const Uri & other) const {
		return mValid != other.mValid || mPath != other.mPath || mHost != other.mHost;
	}
private:
	std::string mHost;
	Path mPath;
	bool mValid;
};

void serialize (sf::Serialization & s, const Uri & uri);

bool deserialize (const json::Value & v, Uri & uri);

}


std::ostream & operator<< (std::ostream & ostream, const sf::Uri & uri);
