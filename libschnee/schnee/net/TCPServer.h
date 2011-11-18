#pragma once

#include <schnee/sftypes.h>

#include "TCPSocket.h"

namespace sf {

struct TCPServerPrivate;

/**
 * A TCP Server. It listens on a port and creates TCPSockets if someone connects.
 * 
 * The Server is closed by default.
 * 
 * The Server supports only ipv4 in the moment.
 * 
 * TODO:
 * - listening on selected IP addresses, not all.
 * - Adding ipv6 support.
 */
class TCPServer {
public:
	TCPServer ();
	virtual ~TCPServer ();
	
	///@name State
	///@{
	
	/// Closes any open Server
	void close ();
	
	/// Starts listening on a given port (on all ip adresses); returns true on success.
	/// @param port port for listening; if 0 there is one choosen automatically.
	bool listen (int port = 0);
	
	/// Returns true if server is currently listening
	bool isListening () const;
	
	/// returns the current associated port
	int serverPort () const;
	
	/// returns current associated server address
	String serverAddress () const;
	
	///@}
	
	///@name Connections
	///@{
	
	/// Sets number of conncections still pending (default=30)
	void setMaxPendingConnections (int num);
	
	/// Returns number of max pending connections (default=30)
	int maxPendingConnections () const;
	
	/// Next connection which is currently waiting (or 0, if no one)
	virtual shared_ptr<TCPSocket> nextPendingConnection ();
	
	/// Returns true if there are pending connections
	bool hasPendingConnections() const;
	
	/** Waits for a new connection
	 * @param msec number of milliseconds to wait, or 0 if we want to wait forever
	 * @param timedOut you can pass in a bool and will know wether function timed out
	 * @return true if there was a new connection
	 */
	bool waitForNewConnection (int msec = 0, bool * timedOut = 0);
	///@}
	
	///@name Delegates 
	///@{
	
	/// There is a new connection pending
	VoidDelegate & newConnection ();
	///@}
	
private:
	TCPServerPrivate * d;
};


}
