#pragma once
#include "Url.h"
#include "HttpResponse.h" // for HttpVersion
namespace sf {

/// Helper class building HTTP requests
class HttpRequest {
public:
	/// Begins building an HTTP request
	HttpRequest ();

	/// Starts the request
	void start (const String & method, const Url & url, HttpVersion version = HTTP_11);

	/// Ends the request
	void end ();

	/// Adds an header element
	/// The 'Host' header will be set automatically if HttpVersion is 11
	void addHeader (const String & key, const String & value);

	/// Adds content
	/// Only add content after you added all headers
	void addContent (const ByteArrayPtr & content);

	/// Returns the packed request. Only valid after end has been called.
	/// You can call it multiple times, but do not start /end /addHeader anymore!
	ByteArrayPtr result () const;

	/// Returns given url (only valid when started)
	const Url & url () const { return mUrl; }
private:
	ByteArrayPtr mDestination;
	Url mUrl;
	bool mStarted;
	bool mEnded;
	bool mHasContent;
};

}
