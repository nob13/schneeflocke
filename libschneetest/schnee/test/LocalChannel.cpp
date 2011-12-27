#include "LocalChannel.h"
#include <assert.h>

#include <schnee/tools/Log.h>

namespace sf {
namespace test {

LocalChannelUsageCollector::LocalChannelUsageCollector () {
	clear ();
}

void LocalChannelUsageCollector::addTransferred (const HostId & from, const HostId & to, long fullData, int hops) {
	Connection c (from, to);
	mTransferred[c] += fullData;
	mSum+=fullData;
	mPhysicalSum        += hops * fullData;
}

void LocalChannelUsageCollector::addPendingData (long data) {
	mPendingData += data;
}

void LocalChannelUsageCollector::consumePendingData (long data) {
	mPendingData -= data;
}

void LocalChannelUsageCollector::clear () {
	mTransferred.clear();
	mSum = 0;
	mPhysicalSum = 0;
	mPendingData = 0;
}

void LocalChannelUsageCollector::print (std::ostream & stream) const {
	stream << "LocalChannel Usage" << std::endl;
	for (AmountMap::const_iterator i = mTransferred.begin(); i != mTransferred.end(); i++){
		long amount (i->second);
		stream << i->first.first << "->" << i->first.second << " : Sum:" << amount << std::endl;
	}
	stream << "Summarized transferred data:          " << mSum << std::endl;
	stream << "Summarized physical transferred data: " << mPhysicalSum << std::endl;
}

long LocalChannelUsageCollector::sum () const {
	return mSum;
}

long LocalChannelUsageCollector::physicalSum() const {
	return mPhysicalSum;
}

long LocalChannelUsageCollector::pendingData () const {
	return mPendingData;
}


LocalChannel::LocalChannel (const HostId & id, LocalChannelUsageCollectorPtr collector){
	SF_REGISTER_ME;
	mBandwidth = -1.0f;
	mDelay = -1.0f;
	mIsToNeighbor = false;
	mAuthenticated = false;
	mHostId = id;
	mCollector = collector;
	mHops = 1;
}

LocalChannel::~LocalChannel () {
	SF_UNREGISTER_ME;
	if (mOther) {
		mOther->setOther (0);
	}
	if (mCollector) {
		mCollector->consumePendingData (mInputBuffer.size());
	}
}

void LocalChannel::bindChannels (LocalChannel & a, LocalChannel & b){
	a.setOther (&b);
	b.setOther (&a);
}

void LocalChannel::setBandwidth (float bandwidth) {
	mBandwidth = bandwidth;
}

void LocalChannel::setDelay (float delay) {
	mDelay = delay;
}

void LocalChannel::setIsToNeighbor (bool v) {
	mIsToNeighbor = v;
}

void LocalChannel::setHops (int hops) {
	mHops = hops;
}

void LocalChannel::setAuthenticated (bool v) {
	mAuthenticated = v;
}

Error LocalChannel::error() const {
	if (mOther) {
		return NoError;
	} else {
		return error::Closed;
	}
}

Channel::State LocalChannel::state () const {
	if (mOther) return Connected;
	else return Unconnected;
}

Error LocalChannel::write (const ByteArrayPtr& data, const ResultCallback & callback) {
	if (!mOther) return error::NotInitialized; // no target
	mOther->incoming (data);
	if (callback) xcall (abind (callback, NoError));
	if (mCollector) mCollector->addTransferred (mHostId, mOther->hostId(), data->size(), mHops);
	return NoError;
}

sf::ByteArrayPtr LocalChannel::read (long maxSize){
	// Also copy & pasted in IMChannel
	sf::ByteArrayPtr result;
	if (maxSize < 0) {
		result = sf::createByteArrayPtr();
		result->swap (mInputBuffer);
		if (mCollector) mCollector->consumePendingData(result->size());
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
	if (mCollector) mCollector->consumePendingData (result->size());
	return result;
}

void LocalChannel::close (const ResultCallback & resultCallback) {
	if (mOther){
		if (mOther->mChanged) xcall (mOther->mChanged);
		mOther->setOther (0);
	}
	setOther (0);
	notifyAsync (mChanged);
	notifyAsync (resultCallback, NoError);
}

void LocalChannel::setOther (LocalChannel * channel){
	mOther = channel;
}

void LocalChannel::incoming (ByteArrayPtr data){
	if (mCollector) mCollector->addPendingData (data->size());
	mInputBuffer.append(*data);
	if (mChanged) {
		xcall (mChanged);
	}
}

}
}
