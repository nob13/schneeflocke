#include "Url.h"
#include <boost/lexical_cast.hpp>

namespace sf {

Url::Url (const String & s) { parse (s); }

Url::Url (const char * s) { parse (s); }

String Url::pureHost () const {
	size_t p = mHost.find(':');
	if (p == mHost.npos) return mHost;
	return mHost.substr(0,p);
}

int Url::port () const {
	size_t p = mHost.find(':');
	if (p != mHost.npos) {
		try {
			return boost::lexical_cast<int> (mHost.substr(p + 1));
		} catch (boost::bad_lexical_cast & ){
			return 0;
		}
	}
	// default values
	if (mProtocol == "http") return 80;
	if (mProtocol == "https") return 443;
	return 0; // unknown
}

String Url::toString () const {
	return mProtocol + "://" + mHost + mPath + (mQuery.empty() ? String("") : String ("?") + mQuery);
}

void Url::parse (const String & url_s) {
	// Found: http://stackoverflow.com/questions/2616011
	// modified.
	const String prot_end("://");
	String::const_iterator prot_i = search(url_s.begin(), url_s.end(),
			prot_end.begin(), prot_end.end());
	if (prot_i == url_s.end()){
		mProtocol = "http";
		prot_i = url_s.begin();
	} else {
		mProtocol.reserve(distance(url_s.begin(), prot_i));
		transform(url_s.begin(), prot_i,
				back_inserter(mProtocol),
				tolower); // protocol is icase
		if( prot_i == url_s.end() )
			return;
		advance(prot_i, prot_end.length());
	}
	String::const_iterator path_i = find(prot_i, url_s.end(), '/');
	mHost.reserve(distance(prot_i, path_i));
	transform(prot_i, path_i,
			back_inserter(mHost),
			tolower); // host is icase
	String::const_iterator query_i = find(path_i, url_s.end(), '?');
	mPath.assign(path_i, query_i);
	if( query_i != url_s.end() )
		++query_i;
	mQuery.assign(query_i, url_s.end());
	// Log (LogInfo) << LOGID << url_s << "-->" << protocol << " " << host << " " << path << " " << query << std::endl;
}

}
