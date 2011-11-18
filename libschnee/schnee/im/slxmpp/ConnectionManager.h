#pragma once
#include <schnee/sftypes.h>
#include <schnee/im/xmpp/XMPPStream.h>
#include <schnee/net/TCPSocket.h>
#include <schnee/net/TCPServer.h>
#include <schnee/tools/ByteArray.h>
#include <schnee/tools/XMLChunk.h>
#include <schnee/tools/async/DelegateBase.h>

///@cond DEV

namespace sf {

/// Connection to another peer
class Connection : public DelegateBase {
public:
	typedef shared_ptr<TCPSocket> TCPSocketPtr;
	enum State { OFFLINE, PREPARE, INIT, RESPOND, TO_OPENED, FINISHED };
	
	Connection(String ownJid, String dstJid = "", String addr = "", int port = 0);
	// Creating connection with existing socket
	Connection (String ownJid, TCPSocketPtr _socket);
	~Connection ();	

	State state () const { return mState; }

	/// Sets prepare state, in this state it is possible to add messages, and it is necessary, before you init/Answerstream
	bool prepare (); 
	
	/// Builds up connection, initializes XMPP stream (beginning from this Side)
	Error initStream (const ResultCallback & result);
	
	/// Answers a stream opening from someone else; fills in id/address/port
	Error answerStream (const ResultCallback & result);
	
	/// Connects incoming chunk handlers & starts session (can be done after initializing stream and security checks)
	/// Also brings out waiting data
	Error startSession ();
	
	/// Returning destination id.
	String dstJid () const { return mStream->dstJid(); }
	
	/// Update connection data 
	void updateNet (const sf::String & addr, int port) { mAddress = addr; mPort = port; }
	
	/// Closes connection
	void close ();
	
	/// Send some message
	bool send (const xmpp::Message & m);
	
	const sf::String address () const { return mAddress; }
	
	int port () const { return mPort; }
	
	IMClient::MessageReceivedDelegate & messageReceived () { return mMessageReceivedDelegate; }
private:
	/// Result of TCP connect during connection routine
	void onTcpConnectResult  (Error result, const ResultCallback & originalCallback);
	void onStreamInitResult  (Error result, const ResultCallback & originalCallback);

	void init ();
	void setState_locked (State s) {mState = s;}
	
	String mAddress;
	int mPort;
	TCPSocketPtr mSocket;
	shared_ptr<XMPPStream> mStream;
	State mState;
	
	/// Waiting queue, created during build up of connection
	std::vector<xmpp::Message> mToSend;
	
	/// responds if stream is closed
	void onStreamClosed ();
	
	void onMessage (const xmpp::Message & m, const XMLChunk & base) {
		IMClient::Message imMessage (m);
		if (imMessage.from.empty()) imMessage.from = mStream->dstJid();
		if (mMessageReceivedDelegate) mMessageReceivedDelegate (imMessage);
	}
	IMClient::MessageReceivedDelegate mMessageReceivedDelegate;
	sf::Mutex mMutex;
};


/**
 * Handles connections for serverless XMPP, can be connected to the XMPPDispatcher.
 * 
 * The connection manager itself is completely different to XMPPConnection, as it acts 
 * as a server and builds tcp connections to clients to whom it sends messages.
 * 
 * Id is a bit different than the regular jabber id; as it is from the form user@host, where host
 * are the different client machines. There is no server.
 */
class ConnectionManager : public DelegateBase {
public:
	typedef shared_ptr<TCPSocket> TCPSocketPtr;
	
	ConnectionManager ();
	~ConnectionManager ();
	
	///@name State
	///@{
	
	/** Starts the Server for incoming connections, port = 0 means that it shall choose a port automatically.
	 * @param id our own jabber identifaction (user\@host)
	 * @param port network port on which the manager shall listen, 0 to let it choose by itself
	 * @return true on success
	 */
	bool startServer (const String & jid, int port = 0);
	/// Stops the server
	void stop ();
	/// returns current online status
	bool isOnline () const;
	
	///@}
	
	///@name Network
	///@{
	
	/// Returns the current used port; returns 0 if there is an error
	int serverPort () const { return mServer.serverPort (); }
	/// Returns the current address of the server
	String serverAddress () const { return mServer.serverAddress(); }
	/// Updates/Adds a target (other reachable client).
	void updateTarget (const String & id, const String & address, int port);
	/// Removes a target
	void removeTarget (const String & id);
	///@}
	
	///@name IO
	///@{
	
	/// Sends data async to the target
	bool send (const xmpp::Message & m);

	///@}
	
	///@name Delegate
	///@{
	IMClient::MessageReceivedDelegate & messageReceived () { return mMessageReceivedDelegate; }
	// IncomingChunkDelegate& incomingChunk() { return mIncomingChunkDelegate; }
	///@}

private:
	typedef shared_ptr<Connection> ConnectionPtr;
	
	/// Maps id to the connection
	typedef std::map<String, ConnectionPtr> ConnectionMap;
		
	
	/// Someone connects to us; starts answerNew connction in a different thread to not block the ioService
	void onNewConnection ();
	
	/// Callback for answering an incoming connection
	void onAnswerResult (Error result, const ConnectionPtr & con);

	/// Creates a connection and sends out first data on it
	Error buildConnection_locked (ConnectionPtr con);

	/// Callback for init stream on new/responding connections
	void onStreamInitResult (Error result, ConnectionPtr con);

	/// Creates/Gets a new connection to the specific target; returns it on success
	ConnectionPtr createGetConnection_locked (const String & target);

	/// We gont a message
	void onIncomingMessage (const xmpp::Message & m) {
		if (mMessageReceivedDelegate)
			mMessageReceivedDelegate (m);
	}

	String mJid;					///< Own jid
	TCPServer mServer;				///< TCP Server to listen	
	ConnectionMap mConnections;		///< Map of open connections
	mutable Mutex mMutex;			///< For locking of internal data structures
	
	// Delegates
	IMClient::MessageReceivedDelegate mMessageReceivedDelegate;
};

}

///@endcond DEV
