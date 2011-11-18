#pragma once

///@cond DEV

#include "../TCPServer.h"
#include "IOBase.h"
#include "TCPSocketPrivate.h"
#include "IOService.h"
#include <schnee/tools/Log.h>
#include <boost/asio.hpp>

#include <deque>
#include <assert.h>

using boost::asio::ip::tcp;

#define MIN_PORT_RANGE 5000
#define MAX_PORT_RANGE 6000


namespace sf {

struct TCPServerPrivate : public IOBase {
	VoidDelegate mNewConnectionDelegate;
	int mMaxPendingConnections;
	
	tcp::acceptor * mAcceptor;
	
	std::deque<TCPSocketPrivateImpl *> mPendingConnections;
	
	TCPSocketPrivateImpl * mNextSocket;
	
	bool  mPendingAcception; ///< there is an async_accept operation working
	
	TCPServerPrivate () : IOBase (IOService::service()){
		mMaxPendingConnections = 30;
		mAcceptor = 0;
		mNextSocket = 0;
		mPendingAcception = false;
	}
	
	~TCPServerPrivate(){
	}

protected:
	virtual void onDeleteItSelf (){
		mNewConnectionDelegate.clear();
		close ();
		IOBase::onDeleteItSelf ();
	}
public:
	
	void close (){
		LockGuard guard (mStateMutex);
		if (!mAcceptor) return;
		mAcceptor->close();
		delete mAcceptor;
		mAcceptor   = 0;
		mNextSocket = 0;
		for (std::deque<TCPSocketPrivateImpl*>::iterator i = mPendingConnections.begin(); i != mPendingConnections.end(); i++){
			delete *i;
		}
		mPendingConnections.clear ();
	}
	
	bool listen (int port){
		if (mAcceptor) close ();
		{
			LockGuard guard (mStateMutex);
			if (port == 0){
				int i = MIN_PORT_RANGE;
				bool error   = false;
				String errMsg;
				bool success = false;
				for (; i < MAX_PORT_RANGE; i++){
					try {
						assert (mAcceptor == 0);
						mAcceptor = new tcp::acceptor (mService, tcp::endpoint (tcp::v4(), i), false); // no reusing
						success = true;
					} catch (boost::system::system_error & err){
						if (!(err.code() == boost::asio::error::address_in_use)){
							error  = true;
							errMsg = err.what(); 
						}
						assert (!mAcceptor);
					}
					if (success || error) break;
				}
				if (i == MAX_PORT_RANGE) {
					Log (LogError) << LOGID << "Did not found a free port in range [" << MIN_PORT_RANGE << "-" << MAX_PORT_RANGE << "]" << std::endl;
					return false;
				}
				if (error) {
					Log (LogError) << LOGID << "There was an error in listening " << errMsg << std::endl;
					return false;
				}
			} else {
				try {
					mAcceptor = new tcp::acceptor(mService, tcp::endpoint (tcp::v4(), port));
				} catch (boost::system::system_error & error){
					Log (LogWarning) << LOGID << "Error opening tcp::acceptor (port=" << port << ")" << error.what() << std::endl;
					return false;
				}
			}
		}
		Log (LogInfo) << LOGID << "Start listening on " << serverAddress() << ":" << serverPort() << std::endl;
		mStateChange.notify_all();
		startAccept ();
		return true;
	}
	
	int serverPort () const {
		LockGuard guard (mStateMutex);
		if (!mAcceptor || !mAcceptor->is_open()) return 0;
		tcp::endpoint end = mAcceptor->local_endpoint();
		return end.port();
	}
	
	String serverAddress () const {
		LockGuard guard (mStateMutex);
		if (!mAcceptor || !mAcceptor->is_open()) return "";
		tcp::endpoint end = mAcceptor->local_endpoint();
		return end.address().to_string();
	}
	
	bool isListening () const {
		return (mAcceptor != 0 && mAcceptor->is_open());
	}
	
	virtual shared_ptr<TCPSocket> nextPendingConnection (){
		shared_ptr<TCPSocket> s;
		{
			LockGuard guard (mStateMutex);
			if (mPendingConnections.size() == 0) return s;
			TCPSocketPrivateImpl * impl = mPendingConnections.front();
			mPendingConnections.pop_front();
			impl->startAsyncReading ();
			s = shared_ptr<TCPSocket> ( new TCPSocket (impl) );
		}
		mStateChange.notify_all();
		return s;
	}
	
	bool hasPendingConnections() const{
		LockGuard guard (mStateMutex);
		return (mPendingConnections.size() > 0);
	}
	
	bool waitForNewConnection (int msec, bool * timedOut){
		LockGuard guard (mStateMutex);
		Time timeOut = futureInMs (msec);
		if (timedOut) *timedOut = false;
		while (mPendingConnections.size() == 0){
			if (msec == 0){
				mStateChange.wait(mStateMutex);
			} else {
				if (!mStateChange.timed_wait(mStateMutex, timeOut)){
					if (timedOut) *timedOut = true;
					return false;
				}
				
			}
		}
		return true;
	}
private:
	void startAccept () {
		{
			LockGuard guard (mStateMutex);
			assert (mAcceptor);
			
			if (!mAcceptor->is_open()) return;
			if (mPendingConnections.size () >= (size_t) mMaxPendingConnections){
				return;
			}
			
			mNextSocket = new TCPSocketPrivateImpl (mService);
			mPendingAcception = true;
			mPendingOperations++;
			mAcceptor->async_accept(mNextSocket->mSocketImpl, boost::bind (&TCPServerPrivate::acceptHandler, this, _1));
		}
		mStateChange.notify_all();
	}
	
	void acceptHandler (const boost::system::error_code & error){
		bool con = false; // continue, not in case of an error
		{
			LockGuard guard (mStateMutex);
			mPendingOperations--;
			mPendingAcception = false;
			if (error){
				// Had error or was canceled
				if (error != boost::asio::error::operation_aborted){
					Log (LogError) << LOGID << "There was an error waiting for an accept " << error.message() << std::endl;
				}
				delete mNextSocket;
				mNextSocket = 0;
			} else if (!mAcceptor){
				// Acceptor is already killed
				delete mNextSocket;
				mNextSocket = 0;
			} else {
				// had Success
				if (mNextSocket->mSocket.is_open())
					mNextSocket->mConnected = true;
				mPendingConnections.push_back (mNextSocket);
				mNextSocket = 0;
				con = true;
			}
		}
		mStateChange.notify_all ();
		if (con){
			if (mNewConnectionDelegate) mNewConnectionDelegate ();
			// Accept the next one
			startAccept ();
		}
	}
	
};

}


///@endcond DEV
