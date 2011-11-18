#pragma once

#include <schnee/p2p/CommunicationComponent.h>
#include <schnee/tools/async/AsyncOpBase.h>

namespace sf {

/**
 * Implements a protocol, so that peers can ask for their connection details.
 *
 * Used indirectly by TCPChannelConnector
 *
 */
class TCPConnectProtocol : public CommunicationComponent, public AsyncOpBase {
public:
	TCPConnectProtocol ();
	virtual ~TCPConnectProtocol ();

	SF_AUTOREFLECT_RPC;

	// Protocol Elements:

	/// Connection details for a TCP Server
	struct ConnectDetails {
		ConnectDetails () : port (-1), error (sf::error::ServerError) {}
		int port;								///< Connection port, < 0 means invalid
		std::vector<std::string> addresses; 	///< Valid connection adresses, maybe IPv4 or IPv6.
		Error error;							///< An error code (e.g. ServerError, when no server is avaiable)

		SF_AUTOREFLECT_SD;
	};

	/// A request for connect details
	struct RequestConnectDetails {
		RequestConnectDetails () : id (0) {}
		AsyncOpId id;
		SF_AUTOREFLECT_SDC;
	};

	/// A reply for connect details
	struct ConnectDetailsReply : public ConnectDetails {
		ConnectDetailsReply () : id (0) {}
		AsyncOpId id;
		SF_AUTOREFLECT_SDC;
	};


	typedef sf::shared_ptr<ConnectDetails> ConnectDetailsPtr;

	/// Set own connect
	void setOwnDetails (const ConnectDetailsPtr & details);

	typedef sf::function<void (Error err, const ConnectDetails & details)> RequestConnectDetailsCallback;
	/// Request details from someone else
	Error requestDetails (const HostId & target, const RequestConnectDetailsCallback& callback = RequestConnectDetailsCallback(), int timeOutMs = -1);


private:
	void onRpc (const HostId & sender, const RequestConnectDetails & request, const sf::ByteArray & content);
	void onRpc (const HostId & sender, const ConnectDetailsReply & reply, const ByteArray & content);


	struct RequestOp : public AsyncOp {
		RequestOp (const sf::Time & time) : AsyncOp (0, time) {}
		virtual void onCancel (Error reason) {
			if (cb) cb (reason, ConnectDetails());
		}

		RequestConnectDetailsCallback cb;
	};

	ConnectDetailsPtr mOwnDetails;
	Mutex mMutex;

};

}
