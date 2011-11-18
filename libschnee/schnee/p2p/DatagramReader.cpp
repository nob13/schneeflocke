#include "DatagramReader.h"
#include <schnee/tools/Log.h>
// For htonl() and ntohl()
#ifdef WIN32
#include "WinSock.h"
#else
extern "C" {
#include <arpa/inet.h>
}
#endif


namespace sf{
DatagramReader::DatagramReader () {
	clear ();
}

Error DatagramReader::read (const ChannelPtr& channel) {
	Error e = NoError;
	for (;;){
		switch (mState) {
		case DR_WAITLENGTHS:
			e = fillBytes (channel, 8);
			if (e) return e;
			mNextHeaderLength  = ntohl (*((const uint32_t*) (mBuffer.c_array())));
			mNextContentLength = ntohl (*((const uint32_t*) (mBuffer.c_array()+4)));
			if (mNextHeaderLength > 2147483647 || mNextContentLength > 2147483647) {
				Log (LogError) << LOGID << "Header/Content length too big" << std::endl;
				mState = DR_ERROR;
				return error::TooMuch;
			}
			mState = DR_WAITHEADER;
			mBuffer.clear();
			break;
		case DR_WAITHEADER:
			e = fillBytes (channel, mNextHeaderLength);
			if (e) return e;
			mDatagram.mHeader = createByteArrayPtr();
			mDatagram.mHeader->swap (mBuffer);
			mState = DR_WAITCONTENT;
			break;
		case DR_WAITCONTENT:
			e = fillBytes (channel, mNextContentLength);
			if (e) return e;
			mDatagram.mContent = createByteArrayPtr();
			mDatagram.mContent->swap (mBuffer);
			mState = DR_WAITLENGTHS;
			return NoError;
			break;
		case DR_ERROR:
			return error::WrongState;
			break;
		}
	}
	assert (!"Cannot go here!");
	return NoError;
}

void DatagramReader::clear () {
	mState = DR_WAITLENGTHS;
	mNextHeaderLength  = 0;
	mNextContentLength = 0;
	mBuffer.clear();
}


Error DatagramReader::fillBytes (const ChannelPtr & channel, uint32_t count) {
	assert (mBuffer.size() <= count);
	if (mBuffer.size() == count) return NoError;
	uint32_t missing = count - mBuffer.size();
	ByteArrayPtr data = channel->read(missing);
	if (!data || data->empty()) return error::NotEnough;
	if (mBuffer.empty()){
		mBuffer.swap(*data);
	} else {
		mBuffer.append(*data);
	}
	if (mBuffer.size() < count) {
		return error::NotEnough;
	}
	return NoError;
}


}
