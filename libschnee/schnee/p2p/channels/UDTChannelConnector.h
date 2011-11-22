#pragma once
#include <schnee/tools/async/AsyncOpBase.h>
#include <schnee/net/UDTSocket.h>
#include <schnee/net/UDPSocket.h>
#include <schnee/net/UDPEchoClient.h>
#include <schnee/net/TLSChannel.h>
#include <schnee/p2p/CommunicationComponent.h>
#include "ChannelProvider.h"
#include "AuthProtocol.h"
#include <sfserialization/autoreflect.h>
#include <assert.h>

namespace sf {

/**
 * UDTChannelConnector manages creation of (potentially) NAT traversed UDT Channels.
 *
 * During establishing the connection, UDP sockets will be used.
 *
 * There is no server as this is a pure P2P protocol.
 *
 * Wording
 * - local  : current local isntance
 * - remote : instance to connect to
 * - intern : internal address of UDP port (address we can figure out by ourself)
 * - extern : external address of UDP port (address which gets figured out using an echo server)
 *
 * Its pure IPv4.
 */
class UDTChannelConnector : public AsyncOpBase, public ChannelProvider, public CommunicationComponent {
public:
	typedef shared_ptr<UDTSocket> UDTSocketPtr;

	/// A networking endpoint (ip + port)
	struct NetEndpoint {
		NetEndpoint (): port (0) {}
		NetEndpoint (const String & _address, int _port) : address (_address) , port (_port) {}
		String address;	///< Address in dot-Form
		int port;		///< Port number
		bool valid () const { return !address.empty() && port > 0; }
		SF_AUTOREFLECT_SD;
	};
	typedef std::vector<NetEndpoint> NetEndpointVec;

	/// Base class for all connect details operations
	struct UDTConnectDetails {
		NetEndpointVec intern;
		NetEndpoint    extern_;
		Error echoResult;	///< Result of echoing
		SF_AUTOREFLECT_SD;
	};

	/// Command for requesting an UDT Connection
	struct RequestUDTConnect : public UDTConnectDetails {
		AsyncOpId id;
		SF_AUTOREFLECT_SDC;
	};

	/// Reply for RequestUDTConnect
	struct RequestUDTConnectReply : public UDTConnectDetails {
		AsyncOpId id;		///< Same like RequestUDTCOnnects.id
		AsyncOpId localId;	///< Associated id of peer
		SF_AUTOREFLECT_SDC;
	};

	/// A single packet used during punching process
	struct PunchUDP {
		AsyncOpId id;			///< Own id (always)
		AsyncOpId otherId;		///< What system think is remote id
		int num;
		HostId local;
		HostId remote;
		SF_AUTOREFLECT_SDC;
	};

	/// Answer to punchUDPReply
	struct PunchUDPReply {
		AsyncOpId id;
		AsyncOpId remoteId;
		int num;
		HostId local;
		HostId remote;
		SF_AUTOREFLECT_SDC;
	};

	/// Ackknowledge for UDP
	struct AckUDP {
		AsyncOpId id;
		AsyncOpId remoteId;
		int num;
		HostId local;
		HostId remote;
		SF_AUTOREFLECT_SDC;
	};

	SF_AUTOREFLECT_RPC;


	UDTChannelConnector();
	virtual ~UDTChannelConnector();

	NetEndpoint echoServer () { LockGuard guard (mMutex); return mEchoServer; }
	void setEchoServer (const NetEndpoint & server) { LockGuard guard (mMutex); mEchoServer = server; }

	// Implementation of ChannelProvider
	virtual sf::Error createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs = -1);
	virtual bool providesInitialChannels () { return false; }
	virtual CommunicationComponent * protocol ();
	virtual void setHostId (const sf::HostId & id);
	virtual ChannelCreationDelegate & channelCreated () { return mChannelCreated; }

private:
	enum ChannelOpId { CREATE_CHANNEL = 1};

	struct CreateChannelOp : public AsyncOp {
		enum State {
			Null,				///< Not yet stareted
			WaitOwnAddress,		///< Waiting for identification of own UDP address
			WaitForeign,		///< Waiting for foreign data
			Connecting,			///< Try to connect foreign address
			ConnectingUDT,		///< Moving UDP->UDT
			TlsHandshaking,		///< TLS Handshaking
			Authenticating,		///< Authenticating channel
		};

		CreateChannelOp (const sf::Time & timeOut) : AsyncOp (CREATE_CHANNEL, timeOut), echoClient (udpSocket) {
			mState   = Null;
			remoteId    = 0;
			punchCount = 0;
			sentAck = false;
			recvAck = false;
		}

		virtual void onCancel (sf::Error reason) {
			if (callback) callback (reason);
		}

		bool           connector; // if being initiator of connection

		HostId         target;
		// local data
		NetEndpoint     externAddress;
		NetEndpointVec  internAddresses;

		// remote's data
		AsyncOpId      remoteId;   // if being reactor, the others' id.
		NetEndpointVec  remoteExternAddresses; // extern adresses we got to know or guessed.
		NetEndpointVec  remoteInternAddresses;

		NetEndpoint     remoteWorkingAddress;

		int otherEndpointCount () const { return remoteInternAddresses.size() + remoteExternAddresses.size() ; }
		const NetEndpoint & otherEndpoint (int i) const {
			assert (i >= 0 && i < otherEndpointCount());
			if ((size_t) i < remoteInternAddresses.size()) return remoteInternAddresses[i];
			return remoteExternAddresses[(i - remoteInternAddresses.size())];
		}

		// networking
		UDPSocket      udpSocket;
		UDTSocketPtr   udtSocket;
		TLSChannelPtr  tlsChannel;
		UDPEchoClient  echoClient;
		ResultCallback callback;
		AuthProtocol   authProtocol;
		bool sentAck;
		bool recvAck;
		int punchCount;
	};
	
	/// Begins connecting process
	void startConnecting_locked (CreateChannelOp * op);
	/// Got echo data from echoserver
	void onEchoClientResult (Error result, AsyncOpId id);
	/// Start with pure udp connecting
	void udpConnect (AsyncOpId id);
	/// There is some UDP Data readable
	void onUdpReadyRead (AsyncOpId id);

	/// Received PunchUDP messsage
	void onUdpRecvPunch_locked (const NetEndpoint & from, CreateChannelOp * op, const PunchUDP & punch);
	/// Received PunchUDPReply message
	void onUdpRecvPunchReply_locked (const NetEndpoint & from, CreateChannelOp * op, const PunchUDPReply & reply);
	/// Received ACK
	void onUdpRecvAck_locked (const NetEndpoint & from, CreateChannelOp * op, const AckUDP & ack);
	
	/// Result of UDT connect
	void onUdtConnectResult_locked (CreateChannelOp* op, Error result);
	/// Result of TLS Handshake
	void onTlsHandshake_locked (CreateChannelOp * op, Error result);
	/// Result of authentication
	void onAuthResult_locked (CreateChannelOp * op, Error result);

	void onRpc (const HostId & sender, const RequestUDTConnect & request, const ByteArray & data);
	void onRpc (const HostId & sender, const RequestUDTConnectReply & reply, const ByteArray & data);

	// Varies the port [-1,+3] based on some access point port mapping experience
	void guessSomeExternAddresses_locked (CreateChannelOp * op);

	ChannelCreationDelegate mChannelCreated;
	HostId mHostId;
	NetEndpoint mEchoServer;
	int    mRemoteTimeOutMs;	///< Timeout for remote connection creation
	int    mPunchRetryTimeMs;	///< Time until retrying punching process
	int    mMaxPunchTrys;		///< Maximum trys to get punching working
};

}
