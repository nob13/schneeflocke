#include "BufferedReader.h"
#include "IOService.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/async/MemFun.h>

namespace sf {

BufferedReader::BufferedReader(boost::asio::io_service & service) : IOBase (service){
	mInputTransferBufferSize = 8192;
	mMaxInputBufferSize = 4194304; // 4 MB.
	mInputTransferBuffer = new char [mInputTransferBufferSize];
	mAsyncReading = false;
	mBytesReadSum = 0;
	mReceivedEof = false;
}

BufferedReader::~BufferedReader (){
	sf::LockGuard  guard (mStateMutex);
	delete [] mInputTransferBuffer;
}

ByteArrayPtr BufferedReader::read (long maxSize){
	ByteArrayPtr result;
	{
		LockGuard lock (mStateMutex);
		if (maxSize < 0) {
			result  = createByteArrayPtr();
			result->swap (mInputBuffer);
		} else {
			size_t bufferSize = mInputBuffer.size();
			if (maxSize <= 0 || bufferSize == 0) return result;
			if (maxSize < (long) bufferSize){
				result = ByteArrayPtr (new ByteArray (mInputBuffer.c_array(), maxSize));
				mInputBuffer.l_truncate (maxSize);
				assert (mInputBuffer.size() == bufferSize - maxSize);
			} else {
				// Read all
				result = ByteArrayPtr (new ByteArray ());
				result->swap (mInputBuffer);
				assert (mInputBuffer.size() == 0);
			}
		}
		if (!mAsyncReading){
			mPendingOperations++;
			mService.post (memFun (this, &BufferedReader::checkAndContinueReadingHandler));
		}
	}
	mStateChange.notify_all();
	return result;
}

ByteArrayPtr BufferedReader::peek (long maxSize) {
	ByteArrayPtr result;
	LockGuard lock (mStateMutex);
	if (maxSize < 0) {
		return sf::createByteArrayPtr (mInputBuffer);
	}
	size_t size = mInputBuffer.size();
	if ((long)size <= maxSize){
		result = ByteArrayPtr (new ByteArray (mInputBuffer));
	} else {
		result = ByteArrayPtr (new ByteArray (mInputBuffer.c_array(), maxSize));
	}
	return result;
}

bool BufferedReader::waitForReadyRead (long int timeOutMs, bool more) {
	assert (!IOService::isCurrentThreadService (mService) && "You cannot wait on a signaling thread of IO facilities; see TCPSocket::waitForReadyRead documentation");
	Time timeOut = futureInMs (timeOutMs);
	bool timed = timeOutMs >= 0;
	
	LockGuard lock (mStateMutex);
	size_t minimum = more ? mInputBuffer.size() : 0;
	if (!mAsyncReading) {
		mPendingOperations++;
		mService.post (memFun (this, &BufferedReader::checkAndContinueReadingHandler));
	}
	while (mPendingOperations > 0 && mInputBuffer.size() == minimum && !mError){
		if (timed){
			if (!mStateChange.timed_wait (mStateMutex, timeOut)){
				return false; // time out
			}
		} else mStateChange.wait (mStateMutex);
	}
	size_t s = mInputBuffer.size();
	return (s > minimum);
}

long BufferedReader::bytesAvailable() const {
	LockGuard lock (mStateMutex);
	return mInputBuffer.size();
}

bool BufferedReader::atEnd() const {
	LockGuard lock (mStateMutex);
	return mInputBuffer.size() == 0 && mReceivedEof;
}

void BufferedReader::startAsyncReading (){
	LockGuard lock (mStateMutex);
	mPendingOperations++;
	mService.post (memFun (this, &BufferedReader::checkAndContinueReadingHandler));
}

void BufferedReader::checkAndContinueReadingHandler (){
	assert (IOService::isCurrentThreadService (mService));
	LockGuard lock (mStateMutex);
	mPendingOperations--;
	checkAndContinueReading_locked ();
}

void BufferedReader::checkAndContinueReading_locked(){
	if (mInputBuffer.size() + mInputTransferBufferSize > mMaxInputBufferSize){
		Log (LogInfo) << LOGID << "Input buffer full, stopping reading" << std::endl;
		return;
	};
	if (mReceivedEof){
		return;
	}
	if (mAsyncReading) {
		// already reading...
		return;
	}
	asyncRead_locked (boost::asio::buffer (mInputTransferBuffer, mInputTransferBufferSize), memFun (this, &BufferedReader::readHandler));
}

void BufferedReader::readHandler (const boost::system::error_code & ec, std::size_t bytesRead){
	bool doClose = false;
	{
		LockGuard lock (mStateMutex);
		bool fireDelegate = false;
		mPendingOperations--;
		mAsyncReading = false;
		if (!ec){
			// Log (LogInfo) << LOGID << "Read " << bytesRead << std::endl;
			mInputBuffer.append (mInputTransferBuffer, bytesRead);
			fireDelegate = true;
		} else if (ec == boost::asio::error::operation_aborted){
			// nothing
		} else if (ec == boost::asio::error::eof){
			Log (LogInfo) << LOGID << " Received EOF. Still in buffer: " << mInputBuffer.size() << std::endl;
			mError = error::Eof;
			mReceivedEof = true;
			doClose = true;
		} else if (ec == boost::asio::error::connection_reset){
			Log (LogInfo) << LOGID << "Connection reset by peer" << std::endl;
			mError = error::ConnectionReset;
			doClose = true;
		} else {
			Log (LogWarning) << LOGID << "There was an error during reading " << ec.message() << std::endl;
			setError_locked (error::Other, ec.message());
		}
		mBytesReadSum += bytesRead;
		if (!ec)
			checkAndContinueReading_locked ();
		
		if (fireDelegate) {
			notifyDelegate_locked (mReadyReadDelegate);
		}
		notifyDelegate_locked (mChangedDelegate);
		if (doClose){
			mPendingOperations++;
		}
	}
	if (doClose){
		close ();
		LockGuard lock (mStateMutex);
		mPendingOperations--;
	}
	mStateChange.notify_all();
}

}
