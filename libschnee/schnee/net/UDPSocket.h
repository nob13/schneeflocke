#pragma once

#include <schnee/sftypes.h>

namespace sf {
class UDPSocketPrivate;

/// Represents a (buffered) user datagram socket
class UDPSocket {
public:
	UDPSocket ();
	~UDPSocket();

	/// Returns bound port (or -1 if not bound)
	int port () const;

	/// Binds the socket to a port (if empty, than bind it to all addresses)
	Error bind (int port = 0, const String & address = "");

	/// Closes the socket
	Error close ();

	/// Sends data to a given receiver. Done asynchronous generally.
	/// Does generally no DNS (that would be to slow and it cannot find out, which resolved IP adress would get the packet)
	Error sendTo (const String & receiver, int receiverPort, const ByteArrayPtr & data);

	/// Receive data (if no data in buffer, returns 0)
	/// If given, from and fromPort will be set to the source of the data
	ByteArrayPtr recvFrom (String * from = 0, int * fromPort = 0);

	/// Waits for some data to be received
	bool waitForReadyRead (int  timeOutMs = 30000);

	/// Returns available datagrams (-1 on error)
	int datagramsAvailable ();

	/// Returns current error state
	Error error () const;

	///@name Delegates
	///@{

	/// There is data to be read on
	VoidDelegate & readyRead ();

	/// There was an error
	VoidDelegate & errored ();

	///@}
private:
	UDPSocketPrivate * d;
};

}
