#pragma once

#include "impl/UDTMainLoop.h"

namespace sf {
class UDTSocket;


/// Implements an server waiting for UDT connections
class UDTServer : public UDTEventReceiver {
public:
	UDTServer ();
	virtual ~UDTServer ();
	
	/// Lets the UDT server listen
	Error listen (int port = 0);

	/// Number of pending connections, < 0 in case of an error
	int pendingConnections () const;
	
	/// Returns current port of listening, < 0 in case of an error
	int port () const;

	/// Returns the next waiting connection
	UDTSocket * nextConnection();
	
	/// There is a new connection going on
	VoidDelegate & newConnection() { return mNewConnection; }
	
private:
	virtual void onReadable  ();
	virtual bool onWriteable () { return false; }
	virtual UDTSOCKET& impl () { return mSocket; }

	UDTSOCKET mSocket;
	std::deque<UDTSOCKET> mWaiting;
	Error mError;
	mutable Mutex mMutex;
	VoidDelegate mNewConnection;
};

}
