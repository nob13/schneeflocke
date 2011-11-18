#pragma once
#include "../HttpResponse.h"

namespace sf{

class HttpResponseParser {
public:
	HttpResponseParser ();

	// Set destination response where to save data
	// must be done before any data pushing occurs
	void setDestination (const HttpResponsePtr & response) {
		mResponse = response;
	}

	bool   ready () const { return mReady; }
	Error  result () const { return mResult; }

	/// Write access to input buffer
	/// If HttpResponseParser is ready it is safe
	/// to remove data from this input buffer
	ByteArray& inputBuffer () { return mInputBuffer; }

	void push (const ByteArray & data);

private:
	void consume ();

	// consumes a line, returns true if enough data
	// line is returned without \r\n
	bool consumeLine (String * dst);

	// Fail (perhaps with some error message)
	void fail (Error e, const char * msg = 0);

	enum State {
		HP_START,				 // Awaiting start of HTTP code
		HP_INHEADERS,            // Currently parsing headers
		HP_INCONTENT,			 // Regular fixed size content
		HP_INCONTENT_CHUNKBEGIN, // Awaiting next chunk
		HP_INCONTENT_CHUNK,		 // Inside a chunk
		HP_INFOOTERS			 // Footers during chunk encoding
	};
	int mNextScanLinePos;
	size_t mContentLength;	 // Full content length (if mContentLengthSet) or current chunk length (if mChunkedEncoding set)
	bool   mContentLengthSet;
	bool   mChunkedEncoding; // if we are receiving chunked encoding
	State mState;
	bool mReady;
	Error mResult;
	ByteArray mInputBuffer;
	HttpResponsePtr mResponse;
};

}
