#pragma once

#include <schnee/tools/async/AsyncOpBase.h>
#include "ChannelProvider.h"

#include <schnee/net/TCPServer.h>
#include <schnee/net/TCPSocket.h>
#include <schnee/p2p/com/TCPConnectProtocol.h>
#include <schnee/p2p/channels/AuthProtocol.h>
#include <schnee/net/TLSChannel.h>

namespace sf {

/**
* TCPChannelConnector manages the creation of TCP Channels.
* It acts as a TCPServer and the opposite client.
*
*/
class TCPChannelConnector : public AsyncOpBase, public ChannelProvider {
public:
	typedef TCPConnectProtocol::ConnectDetailsPtr ConnectDetailsPtr;
	typedef TCPConnectProtocol::ConnectDetails ConnectDetails;
	typedef std::vector<String> AddressVec;
	typedef shared_ptr<TCPSocket> TCPSocketPtr;


	TCPChannelConnector ();
	virtual ~TCPChannelConnector();

	///@name State
	///@{

	/// Start the TCP Server
	/// @param port listening port for the TCP Server, 0 and the server will choose one
	sf::Error start (int port = 0);

	/// Stops the TCP Server
	void stop ();

	/// Returns current state of TCPServer
	bool isStarted () const;

	/// Returns TCP Server port
	int port () const;

	/// Returns the associated on which we listen
	AddressVec addresses () const;

	/// Returns host id setHostId parameter
	HostId hostId () const;

	/// Set the timeout connecting entities do have (in ms)
	/// default = 10000ms
	void setTimeOut (int timeOutMs);

	///@}


	// Implementation of ChannelProvider
	virtual sf::Error createChannel (const HostId & target, const ResultCallback & callback, int timeOut);
	virtual bool providesInitialChannels () { return false; }
	virtual CommunicationComponent * protocol () { return &mProtocol; }
	virtual void setHostId (const sf::HostId & id);
	virtual void setAuthentication (Authentication * auth);
	virtual ChannelCreationDelegate & channelCreated () { return mChannelCreated; }

private:
	struct CreateChannelOp;
	struct AcceptConnectionOp;

	///@name Methods for connecting
	///@{

	/// Start the connection process
	void startConnecting (CreateChannelOp * op);

	/// Callback for TCPConnectProtocol
	void onTcpConnectResult (CreateChannelOp * op, Error result, const ConnectDetails& details);

	/// Connect next possible address
	void connectNext (CreateChannelOp * op);

	/// Callback for TCPSocket::conect (Op must be in state Connecting)
	void onConnect (CreateChannelOp * op, Error result);

	/// Callback for TLSChannel::handshake
	void onTlsHandshake (CreateChannelOp * op, Error result);

	// Callback for Auth protocol
	void onAuthProtocolFinished (CreateChannelOp * op, Error result);

	///@}

	///@name Methods for accept connections
	///@{

	/// There is a new connection attempt
	void onNewConnection ();
	/// TLSChannel encryption finished
	void onAcceptTlsHandshake (AcceptConnectionOp * op, Error result);
	/// Authentication finished
	void onAcceptAuthFinished (AcceptConnectionOp * op, Error result);

	///@}

	/// Checks if authentication is enabled
	bool authenticationEnabled () const { return mAuthentication ? true : false; }

	enum ChannelOpId { CREATE_CHANNEL = 1, ACCEPT_CONNECTION };

	/// Operation on building a channel
	struct CreateChannelOp : public AsyncOp {
		enum State {
			Null,				///< Basic state
			Start,				///< 0. Start up
			AwaitTcpInfo,		///< 1. Awaiting connect details via connect protocol
			Connecting,			///< 2. Connecting tcp socket
			TlsHandshaking,		///< 3. Doing TLS Handshake
			Authenticating		///< 4. Authenticating connection
		};
		CreateChannelOp (const sf::Time & timeOut) : AsyncOp (CREATE_CHANNEL, timeOut) {
			mState = Null;
			hasFailedAuthentication = false;
			hasFailedEncryption = false;
		}
		virtual void onCancel (sf::Error reason) {
			if (callback) callback (reason);
		}
		virtual int type () const { return CREATE_CHANNEL; }

		ResultCallback callback;
		HostId target;
		ConnectDetails connectDetails;			///< the connect details (if already received)
		AddressVec::iterator nextAddress;		///< Next address to connect to
		TCPSocketPtr   socket;					///< Current socket
		TLSChannelPtr  tlsChannel;				///< Encrypting Channel
		AuthProtocol   authProtocol;			///< Authentication protocol
		bool           hasFailedAuthentication;	///< Had failed authentication during process
		bool           hasFailedEncryption;		///< Had failed encryption handshake during process
	};

	/// Operation on accepting a channel
	struct AcceptConnectionOp : public AsyncOp {
		enum State {
			Null,				///< Basic state
			TlsHandshaking,			///< Encrypting connection
			Authenticating		///< Authenticating connection
		};
		AcceptConnectionOp (const sf::Time & timeOut) : AsyncOp (ACCEPT_CONNECTION, timeOut) {
			mState = Null;
			hasFailedAuthentication = false;
		}
		virtual void onCancel (sf::Error reason) {
			Log (LogWarning) << LOGID << "Canceling connection accept attempt due " << toString (reason) << std::endl;
			// do nothing
		}

		HostId target;
		TCPSocketPtr   socket;					///< Current socket
		TLSChannelPtr  tlsChannel;				///< Encrypted channel
		AuthProtocol   authProtocol;			///< Authentication protocol
		bool           hasFailedAuthentication;	///< Had failed authentication during process
	};


	TCPConnectProtocol mProtocol;	 ///< Protocol implementation for getting IP address / port

	TCPServer mServer;				 		///< Main TCP Server
	HostId    mHostId;				 		///< Set host id
	AddressVec mAddresses; 					///< Address of own host
	int       mTimeOutMs;					///< Timeout for incoming connections


	// Delegates
	ChannelCreationDelegate mChannelCreated;
	Authentication * mAuthentication;
};



}
