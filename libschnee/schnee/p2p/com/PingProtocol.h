#pragma once

#include <schnee/p2p/CommunicationComponent.h>
#include <sfserialization/autoreflect.h>

namespace sf {

/**
 * Implements simple ping{} - pong {} Protocol
 *
 * All pings carry an id which is used to find the matching pong.
 *
 * An id of 0 get's ponged, but not measured.
 *
 * @deprecated
 * Note: The ping protocol implementation itself is deprecated.
 * Only the Ping/Pong structures are still used.
 *
 */
class PingProtocol : public CommunicationComponent {
public:
	
	struct Ping {
		AsyncOpId id;
		SF_AUTOREFLECT_SDC;
	};
	struct Pong {
		AsyncOpId id;
		SF_AUTOREFLECT_SDC;
	};
	
	SF_AUTOREFLECT_RPC;

	PingProtocol () : mNextId (1) {}
	virtual ~PingProtocol () {}

	/// Send a ping to another receiver
	sf::Error sendPing (const sf::HostId & receiver);

	/// Generate a ping and register it
	/// You can use this, if you want to send the datagram by yourself
	/// The ping-datagram gets registered, so that an answer can be time measured
	void genPing (const sf::HostId & receiver, Ping & ping);

	/// Clears old ping values (older than threshold in milliseconds)
	void flushOld (int thresholdInMs = 60000);


	/// Returns the number of so far open pings
	int openPingCount () const { return (int) mOpenPings.size(); }

	///@name Delegates
	///@{

	/// Ping delegate carries the ping as floating point (to support ping in µs)
	typedef sf::function <void (const sf::HostId & id, float ping, int pingId)> PongDelegate;

	/// A ping came back, the response time is in sec (as float, so that you can support arbitrary delays)
	PongDelegate & pongReceived () { return mPongReceivedDelegate; }

	///@}

private:
	void onRpc (const HostId & sender, const Ping & ping, const ByteArray & data);
	void onRpc (const HostId & sender, const Pong & pong, const ByteArray & data);

	/// For ping time measurement
	typedef std::pair<sf::HostId, AsyncOpId> PingKey;
	typedef std::map<PingKey, sf::Time> OpenPingMap;
	OpenPingMap mOpenPings;
	/// The next ping id to use
	AsyncOpId mNextPingId;

	Mutex mMutex;
	PongDelegate    mPongReceivedDelegate;
	AsyncOpId		mNextId;
};

}

