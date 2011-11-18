#include "HttpResponseParser.h"
#include <stdio.h>
#include <schnee/tools/Log.h>
#include <boost/lexical_cast.hpp>

namespace sf {

HttpResponseParser::HttpResponseParser () {
	mReady  = false;
	mResult = NoError;
	mState = HP_START;
	mNextScanLinePos = 0;
	mContentLength = 0;
	mContentLengthSet = false;
}

void HttpResponseParser::push (const ByteArray & data) {
	mInputBuffer.append(data);
	if (mInputBuffer.size() > 16777216) {
		mReady = true;
		mResult = error::TooMuch;
		return;
	}
	consume ();
}

static bool splitHeader (const String & s, String * key, String * value) {
	size_t i = s.find(':');
	if (i == s.npos) return false;
	*key   = s.substr(0, i);
	i++;
	while (s[i] == ' ') { i++; }
	if (s[i] == 0) return false;
	*value = s.substr(i, s.npos);
	return true;
}

void HttpResponseParser::consume () {
	String line;
	while (true) {
		switch (mState){
		case HP_START:{
			if (mInputBuffer.empty()) return;
			bool suc = consumeLine (&line);
			if (!suc || mReady) return;
			if (sscanf (line.c_str(), "HTTP/1.0 %d", &mResponse->resultCode) == 1) {
				mResponse->httpVersion = HTTP_10;
			} else if (sscanf (line.c_str(), "HTTP/1.1 %d", &mResponse->resultCode) == 1) {
				mResponse->httpVersion = HTTP_11;
			} else {
				return fail (error::BadProtocol, "Bad protocol start");
			}
			Log (LogInfo) << LOGID << "Response Code: " << mResponse->resultCode << " version= " << mResponse->httpVersion << std::endl;
			if (mResponse->resultCode == 100) {
				// it's a plain proceed, ignoring
				break;
			}
			mState = HP_INHEADERS;
			break;
		}
		case HP_INHEADERS:{
			if (mInputBuffer.empty()) return;
			bool suc = consumeLine (&line);
			if (!suc || mReady) return;
			if (line.empty()) {
				mState = HP_INCONTENT;
				if (mResponse->headers.count("Content-Length") > 0){
					String cl = mResponse->headers["Content-Length"];
					try {
						mContentLength = boost::lexical_cast<size_t> (cl);
						mContentLengthSet = true;
					} catch (boost::bad_lexical_cast & e) {
						Log (LogInfo) << LOGID << "Could not cast content length" << std::endl;
					}
				}
				if (mResponse->headers.count("Transfer-Encoding") > 0){
					String te = mResponse->headers["Transfer-Encoding"];
					if (te == "chunked" || te == "Chunked" || te == "CHUNKED"){
						mChunkedEncoding = true;
						mState = HP_INCONTENT_CHUNKBEGIN;
					} else {
						Log (LogWarning) << LOGID << "Unknown transfer encoding " << te << std::endl;
					}
				}
				if (!mContentLengthSet && !mChunkedEncoding) {
					return fail (error::BadProtocol, "Neither length set nor chunked encoding available, aborting");
				}
				Log (LogInfo) << LOGID << "Finished with header" << std::endl;
				break;
			}
			String key, value;
			suc = splitHeader (line, &key, &value);
			if (!suc) {
				return fail (error::BadProtocol, "Strange protocol during header splitting");
			}
			mResponse->headers[key] = value;
			// Log (LogInfo) << LOGID << "Parsed Header: " << key << " -> " << value << std::endl;
			break;
		}
		case HP_INCONTENT:
			if (mInputBuffer.size() < mContentLength){
				// not enough, wait...
				return;
			}
			mResponse->data = createByteArrayPtr();
			if (mInputBuffer.size() == mContentLength) {
				mResponse->data->swap(mInputBuffer);
			} else {
				mResponse->data->append(mInputBuffer.const_c_array(), mContentLength);
				mInputBuffer.l_truncate(mContentLength);
			}
			mReady  = true;
			mResult = NoError;
			Log (LogInfo) << LOGID << "Ready, read " << mContentLength << std::endl;
			return;
		case HP_INCONTENT_CHUNKBEGIN:{
			if (mInputBuffer.empty()) return;
			bool suc = consumeLine (&line);
			if (!suc) {
				return fail (error::BadProtocol, "Awaited a chunk line");
			}
			// Log (LogInfo) << LOGID << "Read this as a chunk line: " << line << " len=" << line.size() << std::endl;
			if (line.empty())
				break; // try it again
			// Chunk: Hexadecimal Length and then possiblly a ';' and some comment
			unsigned int l;
			int parsed = sscanf (line.c_str(), "%x", &l);
			if (parsed < 1) {
				return fail (error::BadProtocol, "Awaited hexadecimal chunk length");
			}
			if (l == 0) {
				mState = HP_INFOOTERS;
				break;
			}
			// Log (LogInfo) << LOGID << "Len=" << l << std::endl;
			mContentLength = l;
			mState = HP_INCONTENT_CHUNK;
			break;
			}
		case HP_INCONTENT_CHUNK: {
			if (mInputBuffer.size() < mContentLength) return; // wait
			if (!mResponse->data)
				mResponse->data = createByteArrayPtr();
			mResponse->data->append(mInputBuffer.c_array(), mContentLength);
			mInputBuffer.l_truncate(mContentLength);
			mState = HP_INCONTENT_CHUNKBEGIN;
			break;
			}

		case HP_INFOOTERS:{
			bool suc = consumeLine (&line);
			if (!suc || mReady) return;
			if (line.empty()){
				mReady = true;
				Log (LogInfo) << LOGID << "Ready." << std::endl;
				return;
			}
			Log (LogInfo) << LOGID << "Ignoring footer line: " << line << std::endl;
			break;
			}
		}
	}
}

bool HttpResponseParser::consumeLine (String * dst) {
	for (; mNextScanLinePos < ((int)mInputBuffer.size() - 1); mNextScanLinePos++) {
		const int & i = mNextScanLinePos;
		char c = mInputBuffer[i];
		char d = mInputBuffer[i+1];
		if (c == 0) {
			// bad protocol.
			mReady  = true;
			mResult = error::BadProtocol;
			return true;
		}
		if (c == '\n' || (c == '\r' && d == '\n')) {
			dst->assign(mInputBuffer.const_c_array(), i);
			int skip = i + (c == '\r' ? 2 : 1);
			mInputBuffer.l_truncate (skip);
			mNextScanLinePos = 0;
			return true;
		}
	}
	return false;
}

void HttpResponseParser::fail (Error e, const char * msg) {
	mReady  = true;
	mResult = e;
	Log (LogWarning) << LOGID << "Failing " << toString(e) << ":" << (msg ? msg : "") << std::endl;
}


}

