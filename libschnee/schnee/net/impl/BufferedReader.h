#pragma once

#include "IOBase.h"
#include <schnee/sftypes.h>
///@cond DEV

namespace sf {

/// Base class for buffered reading implementation
/// Also provides boost::asio::io_service connection and state mutex / change condition
/// This encapsulates all input buffering and stops/starts reading if main buffer is full
class BufferedReader : public IOBase {
public:
	/// Read handler (with error code and bytes_read)
	typedef boost::function<void (const boost::system::error_code &, std::size_t)> ReadHandler;
	
	BufferedReader (boost::asio::io_service & service);
	virtual ~BufferedReader ();

	// Interface (see TCPSocket and above for documentation)
	// This interfaces work only if async Reading is on (should be always on; just not during SSL handshake)
	ByteArrayPtr read (long maxSize);
	ByteArrayPtr peek (long maxSize);
	long bytesAvailable () const;
	bool atEnd () const;
	
	VoidDelegate & readyRead () { return mReadyReadDelegate; }
	VoidDelegate & changed () { return mChangedDelegate; }

	/// Starts asynchronous reading
	void startAsyncReading ();

protected:
	virtual void onDeleteItSelf () {
		mReadyReadDelegate.clear ();
		mChangedDelegate.clear ();
		IOBase::onDeleteItSelf ();
	}
	
	/**
	 * Implemented by ancestors; start async read operation and give result to the handler
	 * Will be started in ioservice thead in Win32
	 * 
	 * Lock is already set.
	 */
	virtual void asyncRead_locked (const boost::asio::mutable_buffers_1 & buffer, const ReadHandler & handler) = 0;

	/// Stop async operations, implemented by ancestors
	/// Will be  started in ioservice thread in Win32
	/// 
	/// Must not lock.
	virtual void stopAsyncRead_locked () = 0; 
	
	/// Closes channel, may lock. Get's called if there are errors on reading detected.
	virtual void close(const ResultCallback & resultCallback = ResultCallback()) = 0;
	
protected:
	
	/// Checks current buffer size and continues with reading we must be in ioService thread
	void checkAndContinueReadingHandler ();
	
	/// Checks current buffer size and continues reading, internal data already locked.
	void checkAndContinueReading_locked();
	
	/// Comes back in async Reading
	void readHandler (const boost::system::error_code & ec, std::size_t bytesRead);
	
	size_t    	mInputTransferBufferSize;		///< Size of input buffer
	char * 	  	mInputTransferBuffer;			///< Smaller input buffer for current async reading process 
	
	ByteArray 	mInputBuffer;					///< Already received buffer
	size_t 		mMaxInputBufferSize;			///< How much data is allowed in inputbuffer
	bool		mAsyncReading;					///< We are async reading in the momment
	long		mBytesReadSum;					///< Sum of all bytes read
	bool		mReceivedEof;					///< Received EOF
	
	/// Data arrived
	VoidDelegate mReadyReadDelegate;
	/// Something changed
	VoidDelegate mChangedDelegate;
};


}


///@endcond DEV
