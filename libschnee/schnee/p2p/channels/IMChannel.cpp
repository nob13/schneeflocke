#include "IMChannel.h"
#include "IMDispatcher.h"
#include <schnee/p2p/Datagram.h>
#include <schnee/tools/Log.h>
#include <schnee/tools/Base64.h>
#include <schnee/tools/Deserialization.h>

namespace sf {

IMChannel::IMChannel (IMDispatcher * dispatcher, const sf::String & id, OnlineState state){
	mMutex.lock();
	mDispatcher = dispatcher;
	mId = id;
	mState = state;
	mMutex.unlock();
}

IMChannel::~IMChannel (){
}

sf::Error IMChannel::error () const {
	LockGuard guard (mMutex);
	if (!mDispatcher) return sf::error::Closed;
	return sf::NoError;
}

sf::String IMChannel::errorMessage () const {
	// No support
	return "";
}

Channel::State IMChannel::state () const {
	LockGuard guard (mMutex);
	if (!mDispatcher) return Channel::Unconnected;
	return Channel::Connected;
}

Error IMChannel::write (const ByteArrayPtr& data, const ResultCallback & callback) {
	if (!mDispatcher) {
		sf::Log (LogError) << LOGID << "Already invalidated" << std::endl;
		return error::ChannelError;
	}
	// Encode it into a message
	sf::IMClient::Message m;
	m.to = mId;
	if (data->size() < 256){
		// Try if we can use it without encoding...
		Datagram datagram;
		sf::Error err = datagram.decodeFrom (*data);
		if (err) {
			sf::Log (LogError) << LOGID << "Invalid message" << std::endl;
			return error::InvalidArgument;
		}
		if (datagram.contentSize() == 0 && datagram.header()->printable()){
			// we can send it text-only
			m.body.assign (datagram.header()->begin(), datagram.header()->end());
			bool ret = mDispatcher->send(m);
			if (callback) xcall (abind (callback, NoError)); // simulating...
			return ret ? NoError : error::WriteError;
		}
	}
	// Let's go binary, in 1024 Byte chunks
	size_t pos = 0;
	while (pos < data->size()){
		size_t length = std::min (data->size() - pos, size_t (1024));
		sf::ByteArray part;
		part.assign (data->begin() + pos, data->begin() + pos + length);

		sf::String base64 = sf::Base64::encodeFromArray (part);
		m.body = "BNRY" + base64;
		bool ret = mDispatcher->send (m);
		if (!ret) return error::WriteError;
		pos += 1024;
	}
	return NoError;
}

sf::ByteArrayPtr IMChannel::read (long maxSize) {
	// Copy & Paste from LocalChannel
	sf::ByteArrayPtr result;
	{
		sf::LockGuard guard (mMutex);
		if (maxSize < 0) {
			result = sf::createByteArrayPtr();
			result->swap(mInputBuffer);
			return result;
		}
		size_t size = mInputBuffer.size();
		if (maxSize <= 0 || size == 0) return result;
		if (maxSize < (long) size){
			// read up to maxSize
			result = sf::ByteArrayPtr (new sf::ByteArray (mInputBuffer.c_array(), maxSize));
			mInputBuffer.l_truncate (maxSize);
			assert (mInputBuffer.size() == size - maxSize);
		} else {
			// read all
			result = sf::ByteArrayPtr (new sf::ByteArray());
			result->swap (mInputBuffer);
			assert (mInputBuffer.size() == 0);
		}
	}
	return result;
}

void IMChannel::close (const ResultCallback & resultCallback) {
	Log (LogWarning) << LOGID << "TODO, implement IMChannel::close!" << std::endl;
	notifyAsync (resultCallback, NoError);
}

void IMChannel::invalidate () {
	mMutex.lock();
	mDispatcher = 0;
	mState = OS_OFFLINE;
	mMutex.unlock();
	if (mChanged) xcall (mChanged);
}

void IMChannel::pushMessage (const sf::IMClient::Message & m){
	if (m.from != mId){
		sf::Log (LogError) << LOGID << "Source of message doesn't fit to me!" << std::endl;
		// still going on...
	}
	
	const sf::String & body = m.body;
	if (body.size() > 3 && body.substr(0,4) == "BNRY"){
		// Its a binary encoded message
		sf::ByteArray data;
		sf::String rest = body.substr (4, body.length() - 4);
		sf::Base64::decodeToArray(rest, data);
		mMutex.lock();
		mInputBuffer.append (data);
		mMutex.unlock();
		if (mChanged) mChanged();
		return;
	}
	/// Its a text message consisting only with an header, let's recreate it as the same as it was before
	Datagram datagram (sf::createByteArrayPtr(body));
	// re-encode it to get it right into the pipeline
	ByteArrayPtr data = datagram.encode();
	assert (data);
	mMutex.lock();
	mInputBuffer.append (*data);
	mMutex.unlock();
	if (mChanged) mChanged ();
}

void IMChannel::setState (OnlineState state){

	if (state != mState){
		mMutex.lock();
		mState = state;
		mMutex.unlock();
		if (mChanged) mChanged ();
	}
}

}
