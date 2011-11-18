#include "XMLStreamDecoder.h"
namespace sf {

XMLStreamDecoder::XMLStreamDecoder () {
	mState = XS_Start;
	mError = NoError;
}

XMLStreamDecoder::~XMLStreamDecoder () {
}

Error XMLStreamDecoder::onWrite (const ByteArray & data) {
	mInputBuffer.append(data);
	handleData ();
	if (mState == XS_Error) return mError;
	return NoError;
}

const XMLChunk& XMLStreamDecoder::opener () {
	return mOpener;
}

Error XMLStreamDecoder::error () {
	return mError;
}

const String& XMLStreamDecoder::errorText () {
	return mErrorText;
}

void XMLStreamDecoder::reset () {
	mState = XS_Start;
	mError = NoError;
	mErrorText.clear();
	mInputBuffer.clear();
	mOpener = XMLChunk::errChunk();
}


void XMLStreamDecoder::resetToPureStream () {
	mState = XS_ReadOpener;
	mError = NoError;
	mErrorText.clear();
	mInputBuffer.clear();
	mOpener = XMLChunk::errChunk();
}

static void skipWhiteSpaces (ByteArray & buffer) {
	size_t white = 0;
	while (white < buffer.size() && (buffer[white] == ' ' || buffer[white] == '\t' || buffer[white] == '\n'))
		white++;
	buffer.l_truncate (white);
}

size_t findWhiteSpace (const String & s) {
	size_t p = 0;
	for (String::const_iterator i = s.begin(); i != s.end(); i++){
		if (*i == ' ' || *i == '\t' || *i == '\n') return p;
		p++;
	}
	return s.npos;
}

/// Tries to build an XMLChunk of just opening Element
static Error fillElement (const char * start, size_t len, XMLChunk * dest) {
	assert (dest);
	String prep (start, start + len);

	if (prep.length() < 2) return error::BadDeserialization;
	if (prep.substr (prep.length() - 2) == "/>") return error::BadDeserialization;
	if (prep[0] != '<') return error::BadDeserialization;
	size_t np = findWhiteSpace (prep);
	String name;
	if (np == prep.npos){
		// no attributes
		name = prep.substr (1, prep.length() - 2);
	} else {
		name = prep.substr (1, np - 1);
	}
	prep += "</" + name + ">";
	XMLChunk chunk = xml::parseDocument(prep.c_str(), prep.length());
	if (chunk.error() || chunk.children().size() != 1) return error::BadDeserialization;
	*dest = chunk.children()[0];
	return NoError;
}

void XMLStreamDecoder::handleData () {
	State before = mState;
	while (true) {
		if (mState != before && mStateChange) {
			mStateChange ();
		}
		before = mState;
		if (mState == XS_Closed || mState == XS_Error) return;

		skipWhiteSpaces (mInputBuffer);

		switch (mState) {
		case XS_Start:{
			int code = xml::xmlBeginning(mInputBuffer);
			if (code == 0) return;
			if (code < 0) {
				mErrorText = "Invalid XML Begin";
				mError = error::BadDeserialization;
				mState = XS_Error;
				continue;
			}
			// we have enough
			mInputBuffer.l_truncate(code);
			mState = XS_ReadXmlBegin;
			continue;
		}
		case XS_ReadXmlBegin: {
			int code = xml::fullTag (mInputBuffer);
			if (code < 0) {
				mErrorText = "Invalid Start Element";
				mError = error::BadDeserialization;
				mState = XS_Error;
				continue;
			}
			if (code == 0) return;
			// we have enough
			Error e = fillElement (mInputBuffer.const_c_array(), code, &mOpener);
			if (e) {
				mError = e;
				mErrorText = "Invalid Start element";
				mState = XS_Error;
				continue;
			}
			mInputBuffer.l_truncate(code);
			mState = XS_ReadOpener;
			continue;
		}
		case XS_ReadOpener:{
			int code = xml::fullTag (mInputBuffer);
			if (code < 0) {
				mErrorText = "InvalidElement";
				mError = error::BadDeserialization;
				mState = XS_Error;
				continue;
			}
			if (code == 0) return;
			if (mInputBuffer[0] == '<' && mInputBuffer[1] == '/') {
				mInputBuffer.l_truncate(code);
				mState = XS_Closed;
				continue;
			}
			code = xml::completionDetection(mInputBuffer);
			if (code < 0) {
				mErrorText = "Invalid Element";
				mError = error::BadDeserialization;
				mState = XS_Error;
				continue;
			}
			if (code ==  0) return;
			XMLChunk chunk = xml::parseDocument(mInputBuffer.const_c_array(), code);
			if (chunk.error() || chunk.children().size() != 1){
				mErrorText = "Invalid Element";
				mError = error::BadDeserialization;
				mState = XS_Error;
				continue;
			}
			if (mChunkRead){
				mChunkRead (chunk.children()[0]);
			}
			mInputBuffer.l_truncate(code);
			continue;
		}
		default:
			assert (!"Should not come here!");
			return;
		}
	}
}


}
