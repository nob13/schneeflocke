#pragma once

#include "SmoothingFilter.h"
#include <schnee/tools/async/AsyncOpBase.h>
#include <schnee/p2p/Datagram.h>
#include <schnee/p2p/DatagramReader.h>
#include "../ConnectionManagement.h"
#include <schnee/tools/Deserialization.h>
#include <schnee/p2p/com/PingProtocol.h>

namespace sf {

/**
 * Holds all existing channels, does datagram decoding and channel timeout.
 *
 * Channels do have a associated level.
 *
 * Part of GenericConnectionManagement.
 */
class ChannelHolder : public AsyncOpBase{
public:
	typedef AsyncOpId ChannelId;

	ChannelHolder ();
	~ChannelHolder ();

	/// Sets own host id. Is important!
	void setHostId (const HostId & id);

	/// Adds a channel to the holder
	/// Holder overtake ownership!
	/// Returns channel id
	/// Note: if there is an existing channel with target/level, it may be closed immediately.
	ChannelId add (ChannelPtr channel, const HostId & target, bool requested, int level);

	/// Close an existing channel
	Error close (ChannelId id);

	/// Close all channels to a given host
	Error closeChannelsToHost (const HostId & host);

	/// Close all channels to a given host but not the best
	Error closeRedundantChannelsToHost (const HostId & host);

	/// Force the closing of all channels
	/// Note: can lead to data lost
	Error forceClear ();

	/// Finds a channel by HostId and level
	/// Returns 0 if not found
	ChannelId findChannel (const HostId & host, int level) const;

	/// Finds the best channel to a given host
	ChannelId findBestChannel (const HostId & host) const;

	/// Finds the best channel level to a given host
	int findBestChannelLevel (const HostId & host) const;

	/// Info about all connections (see ConnectionManagement)
	ConnectionManagement::ConnectionInfos connections () const;
	/// Info about specifc connection (see ConnectionManagement)
	ConnectionManagement::ConnectionInfo connectionInfo (const HostId & target) const;


	/// Send a datagram into a channel with given id
	/// @param highLevel   if set to true, it is a high level protocol datagram
	///                    this will also trigger timeouts to be updated.
	Error send (ChannelId id, const Datagram & d, bool highLevel, const ResultCallback & callback = ResultCallback());

	/// Add a ping measurement to  a channel
	Error addChannelPingMeasure (ChannelId, float seconds);

	/// Sets channel timeout
	void setChannelTimeout (int timeoutMs);
	/// Sets check interval for channel timeout
	void setChannelTimeoutCheckInterval (int intervalMs);


	///@name Delegates
	///@{

	typedef function<void (const HostId & source, const String & cmd, const Deserialization & ds, const ByteArray & data)> IncomingDatagramDelegate;
	IncomingDatagramDelegate & incomingDatagram () { return mIncomingDatagram; }

	typedef function<void (ChannelId id, const PingProtocol::Pong & pong)> PongDelegate;
	PongDelegate & incomingPong () { return mIncomingPong; }

	typedef function<void (ChannelId id, const HostId & target, int level)> ChannelChangedDelegate;
	ChannelChangedDelegate & channelChanged () { return mChannelChanged; }

	///@}

private:
	struct ChannelReceiver;

	/// A channel changed
	void onChannelChange (ChannelId id);

	typedef shared_ptr<SmoothingFilter> SmoothingFilterPtr;

	/// Contains the channel and associated state machines for receiving datagrams
	struct ChannelReceiver {
		ChannelReceiver () : 	closing (0), requested (false), level (0), delayMeasurement (new SmoothingFilter(10)) {}
		ChannelPtr     			channel;
		DatagramReader 			reader;
		HostId                  target;
		AsyncOpId				closing; ///< AsyncOpId of closing operation if channel is closing
		bool					requested; // this host has requested the channel
		int 					level;
		Time                    utime;	///< Last application level traffic (for timeout purposes)
		SmoothingFilterPtr delayMeasurement;
	};

	// Information about a peer
	struct PeerInfo  {
		PeerInfo () : bestChannel (0), bestLevel (0) {}
		ChannelId bestChannel; 				///< Best channel to this peer
		int bestLevel;						///< Best active channel
		typedef std::map<int, ChannelId> ChannelLevelMap;
		ChannelLevelMap channels; 			///< All channels to this peer
	};

	enum AsyncOperations { CLOSE_CHANNEL = 1};

	/// RPC command for closing a channel
	struct CloseChannel {
		SF_AUTOREFLECT_SDC;
	};

	/// Close a channel to someone
	struct CloseChannelOp : public AsyncOp {
		CloseChannelOp (Time timeOut) : AsyncOp (CLOSE_CHANNEL, timeOut) {}
		ChannelId channelId;
		ResultCallback callback;
		virtual void onCancel (sf::Error reason){
			notify (callback, reason);
		}
	};

	/// Close channel op reported an error
	void onCloseChannelError (Error err, ChannelId id);
	void onReceivedCloseChannel (ChannelId id);
	/// check for timeouted channels
	void onCheckForTimeoutedChannels ();

	/// Finally removes channel from all lists
	Error removeChannelPeerConnection (const HostId & host, ChannelId id, int level);
	Error removeChannel (ChannelId id);
	/// some channels (TLSChannel) do not like to be kicked while delegating
	/// so we do it asyncrhonous with this function
	void throwAwayChannel (ChannelPtr channel);
	/// Log all channels (debug)
	void logChannels () const;

	typedef std::map<ChannelId, ChannelReceiver> ChannelMap;
	ChannelMap mChannels;
	ChannelId mNextChannelId;
	typedef std::map<HostId, PeerInfo> PeerMap;
	PeerMap mPeers;
	HostId mHostId;

	int mCloseTimeoutMs;   				///< Timeout waiting for a close message
	int mChannelTimeoutMs; 				///< Generic timeout for channels, valid if > 0
	int mChannelTimeoutCheckIntervalMs; ///< Interval for checking channel timeouts, valid if > 0
	TimedCallHandle mChannelTimeoutId;

	IncomingDatagramDelegate mIncomingDatagram;
	PongDelegate mIncomingPong;
	ChannelChangedDelegate mChannelChanged;
};

}
