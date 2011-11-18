#pragma once

#include <schnee/sftypes.h>
#include "Channel.h"

namespace sf {

struct TCPSocketPrivate;

/**
 * A simple synchronous/asynchronous TCP communication socket.
 * 
 * The API design is somewhat oriented to Qts sockets, but differs in some points.
 *
 * The implementation is done via d-pointer scheme, each call will be forwarded to a private implementation class.
 * 
 */
class TCPSocket : public Channel {
public:
	
	
	TCPSocket();

	///@cond DEV
		
	/// A constructor to provide TCPSocket with another implementation of TCPSocketPrivate
	/// Note: TCPSocket will kill the private object in its destructor
	TCPSocket (TCPSocketPrivate * init);
	
	///@endcond DEV
		
	virtual ~TCPSocket();

	
	
	/// @name State
	/// @{
	
	/// Open up a connection to the host at the given port.
	/// This function is asynchronous.
	/// Returns only error if already connecting
	Error connectToHost (const String & host, int port, int timeOut = 30000, const ResultCallback & callback = ResultCallback());
	/// Waits until the connection was successfull or timed out or error
	/// May not be called from the delegate thread
	/// Returns true on success
	bool waitForConnected ();
	
	/// returns state of current connection
	bool isConnected () const;
	
	/// disconnects connection from the host
	void disconnectFromHost ();
	
	/// Returns wether socket has keep alive enabled
	bool keepAlive () const;
	/// Sets keep alive status, returns true on success.
	/// Should work if socket is bound. It shall be safe to ignore the result.
	bool setKeepAlive (bool v);
	
	/// @}
	

	// Implementation of Channel
	virtual sf::Error error () const;
	virtual sf::String errorMessage () const;
	virtual State state () const;
	virtual Error write (const ByteArrayPtr& data, const ResultCallback & callback = ResultCallback());
	virtual sf::ByteArrayPtr read (long maxSize = -1);
	virtual void close (const ResultCallback & callback = ResultCallback());
	virtual ChannelInfo info () const;
	virtual const char * stackInfo () const { return "tcp";}
	virtual sf::VoidDelegate & changed ();

	///@}


	/// @name Additional IO
	/// @{
	
	/// Waits until all data is sent, returns current error code
	Error waitForAsyncWrite ();

	/// Waits until there is data to read.
	/// Note: You cannot call this method from a Delegate. Delegates are called
	/// From the underlying IO service which also does processing new data. And the IO service
	/// cannot wait for itself (this would lead into a deadlock)
	///
	/// @param timeOutMs a variable timeOut; use -1 for no timeOut
	/// @param more waits for more data, even when there is already some data.
	/// @return true if there is new data, false if there was a timeout or error
	bool waitForReadyRead (long timeOutMs = 30000, bool more = false);
	
	/// Socket receieved end of data signal
	bool atEnd () const;
	
	/// Peeks up to maxSize bytes from the input buffer
	/// maxsize < 0 --> all available is returned
	virtual sf::ByteArrayPtr peek (long maxSize = -1);

	/// How much data is in the input buffer
	/// @return num of bytes in input buffer
	virtual long bytesAvailable () const;



	/// @}
	/// @name Delegates
	/// @{ 
	
	/// Delegate informed if there is new data to be read
	VoidDelegate & readyRead ();
	/// Delegate for being disconnected
	VoidDelegate & disconnected ();
	/// @}
	
protected:
///@cond DEV

	TCPSocketPrivate * tcpImpl () { return d; }
	const TCPSocketPrivate * tcpImpl () const { return d; }
private:
	TCPSocketPrivate * d;
///@endcond DEV
};

typedef shared_ptr<TCPSocket> TCPSocketPtr;

}
