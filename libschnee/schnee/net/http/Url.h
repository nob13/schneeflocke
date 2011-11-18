#pragma once

#include <schnee/sftypes.h>

namespace sf {

/// Represents an HTTP style URL.
/// Could be merged with sf::Uri class.
class Url {
public:

	Url () {}
	Url (const String & s);
	Url (const char * s);

	/// returns the host without a given port number
	String pureHost () const;

	/// Returns port for this URL
	/// Default values for HTTP and HTTPS included
	int port () const;

	String toString () const;

	const String& protocol() const { return mProtocol; }
	const String& host() const { return mHost; }
	const String& path() const { return mPath; }
	const String& query () const { return mQuery; }


private:
	void parse (const String & url_s);

	String mProtocol;
	String mHost;
	String mPath;
	String mQuery;
};

}
