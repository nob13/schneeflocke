#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <schnee/tools/Log.h>
#include <schnee/tools/async/MemFun.h>
#include <schnee/tools/async/ABind.h>
#include <schnee/tools/async/Notify.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/sftypes.h>
#include <schnee/schnee.h>
#include "IOService.h"
#include "BufferedReader.h"
#include "../Channel.h"

using boost::asio::ip::tcp;

/// @cond DEV

namespace sf {

typedef boost::asio::deadline_timer DeadlineTimer;
typedef shared_ptr<DeadlineTimer> DeadlineTimerPtr;



/**
 *  Private code from the TCPSocket class, as it got to big and is to be used by the SSL socket too.
 * 
 *  This is not to be used from outside libschnee  
 */
struct TCPSocketPrivate : public BufferedReader {
	
	
	/// Initializes and binds the template to a asio socket / stream.
	/// Note async reading will not start by it self; either you have to connect to something
	/// or you push in your own socket stuff and call asyncRead (). (Used in TCPServer)
	TCPSocketPrivate (tcp::socket & _socket, boost::asio::io_service & ioService) :
		BufferedReader (ioService),
		mSocket (_socket),
		mConnected(false),
		mConnecting(false),
		mWaitForTimer (false),
		mWaitForResolve (false),
		mWaitForConnect (false),
		mWaitForWrite (false),
		mBytesTransferred (0),
		mResolver (ioService),
		mAsyncWriting (false)
	{ 
		mPendingOutputBuffer = 0;
	}
	
protected:
	virtual ~TCPSocketPrivate (){
		/// disconnectFromHost ();
	}

	virtual void onDeleteItSelf () {
		{
			sf::LockGuard guard (mStateMutex);
			boost::system::error_code ec; // we do not want any exceptions here..
			mSocket.cancel (ec);
			mResolver.cancel();
			mSocket.close(ec);
		}
		mDisconnectedDelegate.clear ();
		BufferedReader::onDeleteItSelf ();
	}
public:

	/// our socket
	tcp::socket & mSocket;
	bool mConnected;		// is something different than being open
	bool mConnecting;		// there is already a connection attempt
	// debug fields
	bool mWaitForTimer;		// waiting for a timer
	bool mWaitForResolve;	// wait for a resolving handler
	bool mWaitForConnect;	// wait for connect handler
	bool mWaitForWrite;		// wait for writer handler
	
	// internal state
	size_t mBytesTransferred;
			
	// networking stuff
	tcp::resolver mResolver;
	
	DeadlineTimerPtr mTimer;
	
	/// After resolving, next_endpoint holds the next possible endpoint (or no more possible)
	tcp::resolver::iterator mNextEndpoint;
	
	/// The Delegate for being disconnected
	VoidDelegate mDisconnectedDelegate;
	/// Current result callback for connecting
	ResultCallback mConnectResultCallback;
	
	size_t mPendingOutputBuffer;

	/// Output buffer for async writing
	struct OutputElement {
		OutputElement ();
		OutputElement (const ByteArrayPtr & _data, const ResultCallback & _callback)
			: data (_data), callback (_callback) {};
		ByteArrayPtr data;
		ResultCallback callback;
	};
	std::deque<OutputElement> mOutputBuffer;
	bool mAsyncWriting;

	Channel::State state () const {
		LockGuard guard (mStateMutex);
		if (mConnected)  return Channel::Connected;
		if (mWaitForConnect || mWaitForResolve) return Channel::Connecting;
		return Channel::Unconnected;
	}

	// Implementation of BufferedReader::asyncRead 
	virtual void asyncRead_locked (const boost::asio::mutable_buffers_1 & buffer, const ReadHandler & handler){
//#ifdef WIN32
		assert (IOService::isCurrentThreadService (mService));
//#endif
		assert (mStateMutex.try_lock() == false); // works only with non-recursive mutexes
		if (mSocket.is_open()){
			mPendingOperations++;
			mAsyncReading = true;
			mSocket.async_read_some (buffer, handler);
		}
		else {
			mPendingOperations++;
			mAsyncReading = true;
			mService.post (abind (handler, boost::asio::error::operation_aborted, 0));
		}
	}
	
	// Implementation of BufferedReader::stopAsyncOps
	virtual void stopAsyncRead_locked (){
		assert (IOService::isCurrentThreadService (mService));
		assert (mStateMutex.try_lock() == false); // works only with non-recursive mutexes
		if (mSocket.is_open()){ // otherwise there is already no connection anymore
			mSocket.cancel();
		}
	}

	Error connectToHost(const String & host, int port, int timeOut, const ResultCallback & resultCallback){
		LockGuard lock (mStateMutex);
		mError = NoError;
		if (mConnecting) {
			Log (LogError) << LOGID << "BAD! Already connecting..." << std::endl;
			return error::WrongState;
		}
		mConnectResultCallback = resultCallback;

		std::string ports;
		{	// port conversion (to string)
			std::ostringstream s; s << port;
			ports = s.str();
		}
		tcp::resolver::query query (host, ports);

		mTimer = DeadlineTimerPtr (new DeadlineTimer (mService, sf::regTimeOutMs (timeOut)));

		Log (LogInfo) << LOGID << "Start resolving... " << host.c_str() << std::endl;
		mPendingOperations++;
		mWaitForResolve = true;
		mConnecting = true;
		mResolver.async_resolve (query,
				memFun (this,
						&TCPSocketPrivate::resolveHandler));
		mPendingOperations++;
		mWaitForTimer = true;
		mTimer->async_wait (memFun (this, &TCPSocketPrivate::timerHandler));
		return NoError;
	}

	/// Handler after resolving (not necessary successfull)
	void resolveHandler (const boost::system::error_code& error, tcp::resolver::iterator i){
		SF_SCHNEE_LOCK
		{
			LockGuard guard (mStateMutex);
			mPendingOperations--;
			mWaitForResolve = false;
			Log (LogInfo) << LOGID << "Resolve returned " << error.message().c_str() << std::endl;
			for (tcp::resolver::iterator j = i; j != tcp::resolver::iterator (); j++){
				Log (LogInfo) << "    " << " Endpoint: " << j->host_name () << ":" << j->service_name () << " <--> " << j->endpoint().address().to_string() << std::endl;
			}
			// connectNextEndpoint will fail for itself if there was an error
			mNextEndpoint = i;
			connectNextEndpoint_locked (boost::asio::error::host_not_found);
		}
	}

	void timerHandler (const boost::system::error_code& error) {
		SF_SCHNEE_LOCK
		{
			LockGuard guard (mStateMutex);
			mWaitForTimer = false;
			mPendingOperations--;
			if (!(error == boost::asio::error::operation_aborted)){
				Log (LogInfo) << LOGID << "Received time out" << std::endl;
				setError_locked (error::TimeOut, "timed out");
				{
					mSocket.close();
					mConnected = false;
				}
				mConnecting = false;
				notifyDelegate_locked (mDisconnectedDelegate);
				notifyDelegate_locked (mChangedDelegate);
				notifyCallback_locked (&mConnectResultCallback, error::TimeOut);
			}
		// timer was cancelled; is Ok.
		}
		mStateChange.notify_all ();
	}

	/// Connect the next available endpoint, if no endpoint there, cancel the timer and set error
	void connectNextEndpoint_locked (const boost::system::error_code & lastError) {
		tcp::resolver::iterator end;
		if (mNextEndpoint == end){
			setError_locked (error::CouldNotConnectHost, lastError.message());
			mStateChange.notify_all ();
			mTimer->cancel();
			Log (LogInfo) << LOGID << "Canceling timer, there are no next ones" << std::endl;
			mPendingOperations++;
			mConnecting = false;
			mService.post (memFun (this, &TCPSocketPrivate::connectFailedHandler));
			return;
		}
		mConnected = false;
		mSocket.close();
		Log (LogInfo) << LOGID << "Starting connect..." << mNextEndpoint->endpoint().address().to_string() << std::endl;
		mPendingOperations++;
		mWaitForConnect = true;
		mSocket.async_connect (*mNextEndpoint, memFun (this,
				&TCPSocketPrivate::connectResultHandler));
	}

	void connectFailedHandler () {
		SF_SCHNEE_LOCK
		{
			LockGuard guard (mStateMutex);
			notifyDelegate_locked (mChangedDelegate);
			notifyCallback_locked (&mConnectResultCallback, error::CouldNotConnectHost);
			mPendingOperations--;
		}
		mStateChange.notify_all();
	}

	void connectResultHandler (const boost::system::error_code& cerror) {
		SF_SCHNEE_LOCK;
		bool informDelegate = false;
		{
			LockGuard guard (mStateMutex);
			mWaitForConnect = false;
			mPendingOperations--;
			if (!cerror){
				// we have it
				Log (LogInfo) << LOGID << "Connection established" << std::endl;
				informDelegate = true;
				mPendingOutputBuffer = 0;
				mOutputBuffer.clear();
				mTimer->cancel ();
				mConnecting = false;
				mConnected  = true;
				checkAndContinueReading_locked ();
			} else {
				mNextEndpoint++;
				connectNextEndpoint_locked (cerror);
			}
		}
		if (informDelegate){
			LockGuard guard (mStateMutex);
			notifyCallback_locked (&mConnectResultCallback, NoError);
			notifyDelegate_locked (mChangedDelegate);
		}
		mStateChange.notify_all ();
	}

	bool isConnected() const {
		LockGuard lock (mStateMutex);
		// if this fails, someone did not change mConnected
		// assert ((mConnected && mSocket.is_open ()) || !mConnected); // may fail, socket can do what it wants
		return mConnected;
	}
	
	bool isConnected_locked() const {
		// no need for locking, can change suddenly anyway
		return mConnected;
	}

	void disconnectFromHost (){
		{
			LockGuard lock (mStateMutex);
			bool wasOpen = isConnected_locked ();
			mConnected = false;
			mSocket.close ();
			if (wasOpen && mDisconnectedDelegate) {
				mPendingOperations++;
				mService.post (memFun (this, &TCPSocketPrivate::callDisconnectedDelegate));
			}
		}
		mStateChange.notify_all ();
	}
	
	void callDisconnectedDelegate () {
		 {
			 LockGuard guard (mStateMutex);
			 notifyDelegate_locked (mDisconnectedDelegate);
			 notifyDelegate_locked (mChangedDelegate);
			 mPendingOperations--;
		 }
		mStateChange.notify_all();
	}

	virtual void close (const ResultCallback & callback) {
		disconnectFromHost ();
		notifyAsync (callback, NoError);
	}

	Channel::ChannelInfo info () const {
		LockGuard lock (mStateMutex);
		Channel::ChannelInfo result;
		boost::system::error_code ec;
		tcp::endpoint le = mSocket.local_endpoint(ec);
		if (!ec) {
			result.laddress = le.address().to_string() + ":" + sf::toString (le.port());
		}
		tcp::endpoint re = mSocket.remote_endpoint(ec);
		if (!ec) {
			result.raddress = re.address().to_string() + ":" + sf::toString(re.port());
		}
		return result;
	}

	bool keepAlive () const {
		LockGuard lock (mStateMutex);
		
		tcp::socket::keep_alive option;
		boost::system::error_code ec;
		mSocket.get_option (option, ec);
		if (ec) return false;
		return option.value();
	}
	
	bool setKeepAlive (bool v) {
		LockGuard lock (mStateMutex);

		tcp::socket::keep_alive option (v);
		boost::system::error_code ec;
		mSocket.set_option (option, ec);	
		if (ec) {
			Log (LogError) << LOGID << "Could not set keepalive value " << ec.message() << std::endl;
		}
		return !ec;
	}
	
	Error write (ByteArrayPtr data, const ResultCallback & callback) {
		if (!data) {
			sf::Log (LogError) << LOGID << "Invalid data" << std::endl;
			return error::InvalidArgument;
		}
		LockGuard guard (mStateMutex);
		if (!isConnected_locked()) {
			return error::ConnectionError;
		}
		mPendingOutputBuffer += data->size();
		mOutputBuffer.push_back (OutputElement (data, callback));
		if (!mAsyncWriting)
			continueWriting_locked ();
		return NoError;
	}

	virtual void continueWriting_locked () {
		if (mPendingOutputBuffer == 0) {
			mAsyncWriting = false;
			return;
		}
		OutputElement  elem = mOutputBuffer.front ();
		mPendingOperations++;
		mWaitForWrite = true;
		mAsyncWriting = true;
		boost::asio::async_write (mSocket, boost::asio::buffer (*elem.data),
				memFun (this, &TCPSocketPrivate::writeHandler));
	}

	void writeHandler (const boost::system::error_code& werror, std::size_t bytesTransferred) {
		SF_SCHNEE_LOCK
		ResultCallback callback; // call write Handler if not null
		Error callbackResult = NoError;
		{
			LockGuard guard (mStateMutex);
			mWaitForWrite = false;
			mAsyncWriting = false;
			if (mOutputBuffer.empty()){
				Log (LogWarning) << LOGID << "Someone deleted output buffer" << std::endl;
				assert (mPendingOutputBuffer == 0);
			} else {
				OutputElement & elem = mOutputBuffer.front();
				ByteArrayPtr & data = elem.data;
				mPendingOutputBuffer -= bytesTransferred;
				mBytesTransferred    += bytesTransferred;
				assert (bytesTransferred <= data->size());
				if (bytesTransferred != data->size()){
					data->l_truncate (bytesTransferred);
				} else {
					if (elem.callback){
						callback = elem.callback;
						callbackResult = NoError;
					}
					mOutputBuffer.pop_front();
				}
				if (werror) {
					Log (LogInfo) << LOGID << "There was an error during writing " << werror.message() << std::endl;
					setError_locked (error::WriteError, werror.message());
				} else {
					continueWriting_locked ();
				}
			}
		}
		if (callback) {
			callback (callbackResult);
		}
		mStateMutex.lock();
		mPendingOperations--;
		mStateMutex.unlock();
		mStateChange.notify_all();
	}

};

/// Instantiable TCPSocketPrivate for TCP only
struct TCPSocketPrivateImpl : public TCPSocketPrivate {
	/// Used by TCPSocket
	TCPSocketPrivateImpl () : 
		TCPSocketPrivate (mSocketImpl, IOService::instance().getService()), 
		mSocketImpl (IOService::instance().getService()) {}
	/// Used by TCPServer
	TCPSocketPrivateImpl (boost::asio::io_service & service) :
		TCPSocketPrivate (mSocketImpl, service),
		mSocketImpl (service) {}
	
	tcp::socket mSocketImpl;
};

}

/// @endcond DEV
