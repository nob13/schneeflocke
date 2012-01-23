#pragma once
#include <schnee/sftypes.h>

namespace sf {

typedef std::map<String,String> HeaderMap;
enum HttpVersion { HTTP_UNKNOWN, HTTP_10, HTTP_11 };

/// Result structure of Http Requests
struct HttpResponse {
	HttpResponse () : resultCode (0), httpVersion (HTTP_UNKNOWN), authenticated (false) {}
	int resultCode;				///< Result code sent by server
	HttpVersion httpVersion;	///< Http Version the server used
	HeaderMap headers;			///< Http Headers the server sent
	ByteArrayPtr data;			///< Received data
	bool authenticated;			///< Operation was done via authenticated channel
};
typedef shared_ptr<HttpResponse> HttpResponsePtr;

}

