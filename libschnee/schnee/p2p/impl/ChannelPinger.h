#pragma once
#include "ChannelHolder.h"
#include <schnee/p2p/com/PingProtocol.h>

namespace sf {

/// Does the pinging process on a ChannelHolder
class ChannelPinger : public DelegateBase {
public:
	typedef ChannelHolder::ChannelId ChannelId;
	ChannelPinger ();
	~ChannelPinger ();

	/// Binds to the holder
	void init (ChannelHolder * holder);

	// Starts pinging
	void start ();
	// stops pinging
	void stop  ();

	/// Received a pong
	void onPong (ChannelId id, const PingProtocol::Pong & pong);

	///@name Deleagetes
	///@{
	typedef function <void (ChannelId id, float seconds)> MeasureDelegate;

	/// We measured delay of one ping
	MeasureDelegate & measure () { return mMeasure; }
	///@}
private:
	void onDoPinging ();
	ChannelHolder * mHolder;

	int mPingInterval;   ///< Interval on pinging
	bool mIsPinging;     ///< We are pinging in the moment
	int mPingTimeoutMs;
	TimedCallHandle mPingDelayedCall; ///< Delayed call handle for the ping (to break it at the end)
	AsyncOpId mNextPingId;
	typedef std::pair<ChannelId, AsyncOpId> PingKey;
	typedef std::map<PingKey, Time> OpenPingMap; ///< yet not answered Pings
	OpenPingMap mOpenPings;
	Mutex mMutex;
	MeasureDelegate mMeasure;
};

}
