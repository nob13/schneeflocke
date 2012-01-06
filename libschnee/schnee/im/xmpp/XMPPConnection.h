#pragma once
#include "XMPPStream.h"
#include <schnee/im/IMClient.h>

namespace sf {

/**
 * Interface for a connecting to the XMPP Network.
 * Note: XMPPConnection is not needed anymore once you are connected.
 * If you are connected you can throw it away, as XMPPStream will hold
 * all necessary data.
 */
class XMPPConnection {
public:

	typedef shared_ptr<XMPPStream> XMPPStreamPtr;
	typedef IMClient::ConnectionState State;

	// For Parsing connection details
	struct XmppConnectDetails {
		XmppConnectDetails () { defaultValues (); }
		void defaultValues ();
		bool setTo (const String & connectionString); ///< Use connection string for init
		String connectionString ();

		String username;			///< the username of the connection
		String password; 			///< the password of the connection
		String server;				///< The server to whom we connect
		int     port;				///< The port of the server
		String resource;			///< The resource identification

		/// full id which which would be set after successfull connection
		String fullId () const;
	};



	virtual ~XMPPConnection() {}

	/// Returns current connection state
	virtual State state () const = 0;

	/// Set connection details
	virtual void setConnectionDetails (XmppConnectDetails & details) = 0;

//	/// Set Connection URL
//	virtual Error setConnectionString (const String & s) = 0;
//
	/// Returns full id (valid with connection string)
	virtual String fullId () const = 0;

	/// Seperately set login password (if not already included in connectionstring)
	virtual Error setPassword (const String & p) = 0;

	/// Connects the XMPP network
	virtual Error connect (const XMPPStreamPtr& stream, int timeOutMs, const ResultCallback & callback = ResultCallback()) = 0;

	/// Connects to the XMPP network, without logging in, binding resource or starting session
	virtual Error pureConnect (const XMPPStreamPtr & stream, int timeOutMs, const ResultCallback & callback = ResultCallback()) = 0;

	/// Delegate called when changing connection state
	virtual IMClient::ConnectionStateChangedDelegate & connectionStateChanged () = 0;

	/// Returns an error text in error state
	virtual String errorText () const = 0;

};

}
