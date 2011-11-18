#include "HttpRequest.h"
#include <schnee/tools/Log.h>

namespace sf {

ByteArray& operator << (ByteArray & ba, const char * s) {
	ba.append(s, strlen(s));
	return ba;
}

ByteArray& operator << (ByteArray & ba, const String & s) {
	ba.append(s.c_str(), s.length());
	return ba;
}

HttpRequest::HttpRequest () {
	mStarted = false;
	mEnded = false;
	mHasContent = false;
	mDestination = sf::createByteArrayPtr();
	mDestination->reserve(2048);
}

void HttpRequest::start (const String & method, const Url & url, HttpVersion version) {
	String sMethod = boost::to_upper_copy(method);
	if (sMethod != "GET" && sMethod != "POST" && sMethod != "HEAD" && sMethod != "PUT") {
		Log (LogWarning) << LOGID << "Unsupported method " << sMethod << std::endl;
	}
	*mDestination << sMethod << " ";
	mUrl = url;
	if (url.query().empty()){
		*mDestination << url.path() << " ";
	} else {
		*mDestination << url.path() << "?" << url.query();
	}
	if (version == HTTP_11) {
		*mDestination << "HTTP/1.1";
	} else {
		*mDestination << "HTTP/1.0";
	}
	*mDestination << "\r\n";
	mStarted = true;
	if (version == HTTP_11)
		addHeader ("Host", url.host());
}

void HttpRequest::end () {
	*mDestination << "\r\n";
	mEnded = true;
}


void HttpRequest::addHeader (const String & key, const String & value) {
	if (!mStarted) {
		Log (LogError) << LOGID << "Adding header without having started the request will lead to invalid HTTP request!" << std::endl;
	}
	if (mEnded) {
		Log (LogError) << LOGID << "Adding header after ending will lead to invalid HTTP" << std::endl;
	}
	*mDestination << key << ": " << value << "\r\n";
}

void HttpRequest::addContent (const ByteArrayPtr & content) {
	if (!mStarted || mEnded) {
		Log (LogError) << LOGID << "Bad state!" << std::endl;
	}
	addHeader ("Content-Length", sf::toString(content->size()));
	*mDestination << "\r\n";
	mDestination->append(*content);
	mHasContent = true;
}

ByteArrayPtr HttpRequest::result () const {
	if (!mEnded) {
		Log (LogError) << LOGID << "Data is not yet valid, end not called!" << std::endl;
	}
	return mDestination;
}

}
