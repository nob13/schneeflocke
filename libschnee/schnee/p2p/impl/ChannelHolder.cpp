#include "ChannelHolder.h"
#include <schnee/tools/Serialization.h>

namespace sf {

ChannelHolder::ChannelHolder () {
	SF_REGISTER_ME;
	mNextChannelId = 1;
	mCloseTimeoutMs   = 60000;
	mChannelTimeoutMs = 600000;
	mChannelTimeoutCheckIntervalMs  = 60000;
//	// debug values:
//	mChannelTimeoutMs = 10000;
//	mChannelTimeoutCheckIntervalMs = 1000;
	if (mChannelTimeoutCheckIntervalMs > 0){
		mChannelTimeoutId = xcallTimed (dMemFun (this, &ChannelHolder::onCheckForTimeoutedChannels), sf::futureInMs(mChannelTimeoutCheckIntervalMs));
	}
}

ChannelHolder::~ChannelHolder () {
	SF_UNREGISTER_ME;
	sf::cancelTimer(mChannelTimeoutId);
}

void ChannelHolder::setHostId (const HostId & id) {
	mHostId = id;
}

ChannelHolder::ChannelId ChannelHolder::add (ChannelPtr channel, const HostId & target, bool requested, int level) {
	assert (channel);
	ChannelId id = mNextChannelId++;

	// Insert into channel map
	ChannelReceiver & recv = mChannels[id];
	recv.channel = channel;
	recv.closing = false;
	recv.level = level;
	recv.requested = requested;
	recv.target = target;
	recv.utime = currentTime();

	// insert into peer map
	PeerInfo & info = mPeers[target];
	if (info.channels.count(level) > 0){
		// D'oh, there is already such a leveled channel
		ChannelId oldChannelId = info.channels[level];
		assert (mChannels.count(oldChannelId) > 0 && "Inconsistency detected");
		ChannelReceiver & existing = mChannels[oldChannelId];

		// Algorithm:
		// If the channel was not requested and we have the lower id, throw it away
		// If the channel was requested and we have the higher id, throw it away.
		// otherwise we overwrite the existing channel.
		// overwrite errornous channels always.
		bool oldHadError = existing.channel->error() != NoError;
		if (!oldHadError && ((requested && mHostId < target) || (!requested && mHostId > target))){
			// close this new channel
			close(id);
			return id;
		} else {
			// close the old channel
			close (info.channels[level]);
		}
	}
	if (level > info.bestLevel) {
		info.bestChannel = id;
		info.bestLevel = level;
	}
	info.channels[level] = id;
	channel->changed() = abind (dMemFun (this, &ChannelHolder::onChannelChange), id);
	// prove channel change
	xcall (abind (dMemFun (this, &ChannelHolder::onChannelChange), id));
	return id;
}

Error ChannelHolder::close (ChannelId id) {
	ChannelMap::iterator i = mChannels.find (id);
	if (i == mChannels.end()) return error::NotFound;
	ChannelReceiver & receiver (i->second);

	if (receiver.closing) {
		return error::ExistsAlready;
	}
	// Removing from peer list
	removeChannelPeerConnection (receiver.target, id, receiver.level);
	notifyAsync (mChannelChanged, id, receiver.target, receiver.level);

	// notify someone?
	CloseChannel cc;
	Error e = Datagram::fromCmd(cc).sendTo (receiver.channel);
	if (e) {
		Log (LogWarning) << LOGID << "Could not close the channel to " << receiver.target << "(" << receiver.level << ") as I could not send the close message!" << std::endl;
		removeChannel (id);
		return e;
	}
	CloseChannelOp * op = new CloseChannelOp (sf::regTimeOutMs(mCloseTimeoutMs));
	op->setId(genFreeId());
	op->channelId = id;
	op->callback = abind (dMemFun (this, &ChannelHolder::onCloseChannelError), id);
	receiver.closing = op->id();
	addAsyncOp (op);
	return NoError;
}

Error ChannelHolder::closeChannelsToHost (const HostId & host) {
	PeerMap::iterator i = mPeers.find (host);
	if (i == mPeers.end()) return error::NotFound;
	PeerInfo & info (i->second);
	PeerInfo::ChannelLevelMap channels = info.channels; // copy it, otherwise it may be destroyed.
	for (PeerInfo::ChannelLevelMap::const_iterator i = channels.begin(); i != channels.end(); i++) {
		close (i->second);
	}
	return NoError;
}

Error ChannelHolder::closeRedundantChannelsToHost (const HostId & host) {
	PeerMap::iterator i = mPeers.find (host);
	if (i == mPeers.end()) return error::NotFound;
	PeerInfo & info (i->second);
	PeerInfo::ChannelLevelMap channels = info.channels; // copy it, otherwise it may be destroyed.
	bool first = true;
	for (PeerInfo::ChannelLevelMap::reverse_iterator i = channels.rbegin(); i != channels.rend(); i++){
		if (!first)
			close (i->second);
		first = false;
	}
	return NoError;
}

Error ChannelHolder::forceClear () {
	for (ChannelMap::iterator i = mChannels.begin(); i != mChannels.end(); i++) {
		ChannelReceiver & receiver (i->second);
		if (!receiver.closing) {
			CloseChannel cc;
			Datagram::fromCmd (cc).sendTo (receiver.channel);
		}
		receiver.channel->changed().clear();
		xcall (abind (dMemFun (this, &ChannelHolder::throwAwayChannel), receiver.channel));
	}
	mChannels.clear();
	mPeers.clear();
	// all operations do have key 0
	cancelAsyncOps (0, error::Canceled);
	return NoError;
}

ChannelHolder::ChannelId ChannelHolder::findChannel (const HostId & host, int level) const {
	PeerMap::const_iterator i = mPeers.find (host);
	if (i == mPeers.end()) {
		return 0;
	}
	PeerInfo::ChannelLevelMap::const_iterator j = i->second.channels.find(level);
	if (j == i->second.channels.end()) return 0;
	return j->second;
}

ChannelHolder::ChannelId ChannelHolder::findBestChannel (const HostId & host) const {
	PeerMap::const_iterator i = mPeers.find (host);
	if (i == mPeers.end()) {
		return 0;
	}
	return i->second.bestChannel;
}

int ChannelHolder::findBestChannelLevel (const HostId & host) const {
	PeerMap::const_iterator i = mPeers.find (host);
	if (i == mPeers.end()) {
		return 0;
	}
	return i->second.bestLevel;
}

/// Constructs comma-separated stack
static String getStack (ChannelPtr channel) {
	String result = channel->stackInfo();
	while (channel->next()){
		channel = channel->next();
		result = result + "," + channel->stackInfo();
	}
	return result;
}


ConnectionManagement::ConnectionInfos ChannelHolder::connections () const {
	ConnectionManagement::ConnectionInfos result;
	for (ChannelMap::const_iterator i = mChannels.begin(); i != mChannels.end(); i++) {
		const ChannelReceiver & r (i->second);
		if (!r.closing) {
			ConnectionManagement::ConnectionInfo info;
			info.target = r.target;
			info.level  = r.level;
			info.cinfo  = r.channel->info();
			info.id     = i->first;
			if (info.cinfo.delay < 0 && r.delayMeasurement->hasAvg()){
				info.cinfo.delay = r.delayMeasurement->avg();
			}
			info.stack  = getStack (r.channel);
			result.push_back (info);
		}
	}
	return result;
}

Error ChannelHolder::send (ChannelId id, const Datagram & d, bool highLevel, const ResultCallback & callback) {
	ChannelMap::iterator i = mChannels.find(id);
	if (i == mChannels.end()) return error::NotFound;
	if (i->second.closing) return error::Closed;
	ByteArrayPtr encoded = d.encode();
	if (!encoded) return error::TooMuch;
	if (highLevel)
		i->second.utime = currentTime();
	return i->second.channel->write(encoded, callback);
}

Error ChannelHolder::addChannelPingMeasure (ChannelId id, float seconds) {
	ChannelMap::iterator i = mChannels.find(id);
	if (i == mChannels.end()) return error::NotFound;
	if (i->second.closing) return error::Closed;
	i->second.delayMeasurement->add(seconds);
	return NoError;
}

void ChannelHolder::setChannelTimeout (int timeoutMs) {
	mChannelTimeoutMs = timeoutMs;
}

void ChannelHolder::setChannelTimeoutCheckInterval (int intervalMs) {
	mChannelTimeoutCheckIntervalMs = intervalMs;
	cancelTimer (mChannelTimeoutId);
	mChannelTimeoutId = TimedCallHandle();
	if (mChannelTimeoutCheckIntervalMs > 0) {
		mChannelTimeoutId = xcallTimed (dMemFun (this, &ChannelHolder::onCheckForTimeoutedChannels), futureInMs (mChannelTimeoutCheckIntervalMs));
	}
}

void ChannelHolder::onChannelChange (ChannelId id) {
	HostId sender;
	std::vector<Datagram> received; // we can store them, they are cheap.
	{
		ChannelMap::iterator i = mChannels.find (id);
		if (i == mChannels.end()) {
			Log (LogWarning) << LOGID << "Channel change from unknown channel?!" << std::endl;
			return;
		}
		ChannelReceiver & receiver (i->second);
		Error e = NoError;
		sender = receiver.target;

		while (( e = receiver.reader.read(receiver.channel)) == NoError){
			received.push_back (receiver.reader.datagram());
		}
		if (e != error::NotEnough) {
			Log (LogWarning) << LOGID << "Got error on decoding datagram " << toString (e) << " will close channel" << std::endl;
			close (id);
		}
		if (receiver.channel->error()){
			Log (LogWarning) << LOGID << "Channel reports an error, trying to close" << std::endl;
			close (id); // should not harm if two times.
		}
	}
	bool updatedUTime = false; // we already updated utime

	// Pump out received commands (must be non-locked, answer could come immediately)
	for (std::vector<Datagram>::const_iterator i = received.begin(); i != received.end(); i++) {
		String cmd;
		Deserialization ds (*i->header(), cmd);
		if (ds.error()) {
			Log (LogWarning) << LOGID << "Received corrupted package from " << sender << std::endl;
			close (id);
		} else {
			// check for low level protocols (TODO: CLEANUP)
			if (cmd == CloseChannel::getCmdName()){
				onReceivedCloseChannel (id);
			} else if (cmd == PingProtocol::Ping::getCmdName()){
				PingProtocol::Ping ping;
				if (!ping.deserialize (ds)){
					Log (LogWarning) << LOGID << "Received invalid ping" << std::endl;
				} else {
					// answer with pong
					PingProtocol::Pong pong;
					pong.id = ping.id;
					send (id, Datagram::fromCmd(pong), false);
				}
			} else if (cmd == PingProtocol::Pong::getCmdName()){
				PingProtocol::Pong p;
				if (p.deserialize(ds)){
					notify (mIncomingPong, id, p);
				}else {
					Log (LogWarning) << LOGID << "Received invalid pong" << std::endl;
				}
			} else {
				if (!updatedUTime) {
					ChannelMap::iterator j = mChannels.find(id);
					if (j != mChannels.end()){
						j->second.utime = currentTime();
					}
					updatedUTime = true;
				}
				// high level protocol
				notify (mIncomingDatagram, sender, cmd, ds, *i->content());
			}
		}
	}
}

void ChannelHolder::onCloseChannelError (Error err, ChannelId id) {
	Log (LogWarning) << LOGID << "Received " << toString(err) << " during waiting on channel close" << std::endl;
	if (mChannels.count(id) == 0){
		Log (LogWarning) << LOGID << "Did not found channel for close op?" << std::endl;
		return;
	}
	mChannels.erase(id);
}

void ChannelHolder::onReceivedCloseChannel (ChannelId id) {
	ChannelMap::iterator i = mChannels.find(id);
	if (i == mChannels.end()) {
		Log (LogWarning) << LOGID << "Received closeChannel from nonexisting channel" << std::endl;
		return;
	}
	ChannelReceiver & receiver (i->second);
	if (receiver.closing) {
		// we asked for closing
		// it must be removed from peer info connections
		CloseChannelOp * op;
		getReadyAsyncOp (receiver.closing, CLOSE_CHANNEL, &op);
		if (!op) {
			Log (LogWarning) << LOGID << "Received close after timeout?!" << std::endl;
		} else {
			delete op;
		}
		// finally closing channel
		removeChannel (id);
	} else {
		// the other wants to close the channel, aknowleding
		// he must have sent everything already now...
		CloseChannel cc;
		Datagram::fromCmd(cc).sendTo (receiver.channel);
		removeChannel (id);
	}
}

void ChannelHolder::onCheckForTimeoutedChannels () {
	mChannelTimeoutId = TimedCallHandle();
	Time t = currentTime ();
	if (mChannelTimeoutMs > 0) {
		for (ChannelMap::const_iterator i = mChannels.begin(); i != mChannels.end(); i++) {
			const ChannelReceiver & receiver (i->second);
			if (!receiver.closing){
				int dt = (t - receiver.utime).total_milliseconds();
				if (dt > mChannelTimeoutMs){
					Log (LogProfile) << LOGID << "Closing channel " << i->first << " to " << i->second.target << " due channel timeout" << std::endl;
					// do it asynchronous, under certain circumstances it is possible
					// that close will change data structures.
					xcall (abind (dMemFun (this, &ChannelHolder::close), i->first));
				}
			}
		}
	}
	// repeat
	if (mChannelTimeoutCheckIntervalMs > 0){
		mChannelTimeoutId =	xcallTimed (dMemFun (this, &ChannelHolder::onCheckForTimeoutedChannels), futureInMs (mChannelTimeoutCheckIntervalMs));
	}
}


Error ChannelHolder::removeChannelPeerConnection (const HostId & host, ChannelId id, int level)  {
	PeerMap::iterator i = mPeers.find(host);
	if (i == mPeers.end())
		return error::NotFound;

	PeerInfo & info (i->second);
	info.channels.erase(level);
	if (info.bestChannel == id) {
		// search new best channel
		if (info.channels.empty()){
			// no connections anymore, deleting...
			mPeers.erase (i);
		} else {
			info.bestChannel = info.channels.rbegin()->second;
			info.bestLevel   = info.channels.rbegin()->first;
		}
	}
	return NoError;
}

Error ChannelHolder::removeChannel (ChannelId id) {
	ChannelMap::iterator i = mChannels.find (id);
	if (i == mChannels.end())
		return error::NotFound;

	ChannelReceiver & receiver (i->second);
	removeChannelPeerConnection (receiver.target, id, receiver.level); // doesn't harm if done to often
	notifyAsync (mChannelChanged, id, receiver.target, receiver.level);
	ChannelPtr channel = receiver.channel;
	mChannels.erase (id);
	channel->changed().clear();
	xcall (abind (dMemFun (this, &ChannelHolder::throwAwayChannel), channel));
	return NoError;
}

void ChannelHolder::throwAwayChannel (ChannelPtr channel) {
	// intentionally nothing
}

void ChannelHolder::logChannels () const {
	Log (LogInfo) << LOGID << "PeerInfos" << std::endl;
	for (PeerMap::const_iterator i = mPeers.begin(); i != mPeers.end(); i++) {
		Log (LogInfo) << LOGID << " To " << i->first << " Best: " << i->second.bestChannel << " level: " << i->second.bestLevel << std::endl;
	}
	Log (LogInfo) << LOGID << "Channels: " << std::endl;
	for (ChannelMap::const_iterator i = mChannels.begin(); i != mChannels.end(); i++) {
		Log (LogInfo) << LOGID << i->first << " to " << i->second.target << " level= " << i->second.level << std::endl;
	}
}


}


