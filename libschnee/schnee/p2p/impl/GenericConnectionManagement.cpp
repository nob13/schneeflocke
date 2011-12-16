#include "GenericConnectionManagement.h"
#include <schnee/tools/Log.h>
#include <schnee/p2p/com/PingProtocol.h>
#include <schnee/tools/Serialization.h>
#include <assert.h>

namespace sf {

GenericConnectionManagement::GenericConnectionManagement() {
	mCommunicationMultiplex = 0;
}

GenericConnectionManagement::~GenericConnectionManagement () {
}

Error GenericConnectionManagement::init (CommunicationMultiplex * multiplex) {
	SF_REGISTER_ME;
	mCommunicationMultiplex = multiplex;

	// wiring
	mChannelPinger.init(&mChannels);
	mChannels.incomingPong ()    = dMemFun (&mChannelPinger, &ChannelPinger::onPong);
	mChannels.channelChanged()   = dMemFun (this, &GenericConnectionManagement::onChannelChanged);
	mChannels.incomingDatagram() = dMemFun (this, &GenericConnectionManagement::onIncomingDatagram);
	mChannelPinger.measure()     = dMemFun (&mChannels, &ChannelHolder::addChannelPingMeasure);

	return NoError;
}

void GenericConnectionManagement::uninit () {
	SF_UNREGISTER_ME;

	for (ChannelProviderMap::iterator i = mChannelProviders.begin(); i != mChannelProviders.end(); i++){
		if (i->second->protocol())
			mCommunicationMultiplex->delComponent(i->second->protocol());
	}
	mCommunicationMultiplex = 0;
}

void GenericConnectionManagement::clear () {
	mChannels.forceClear();
}

void GenericConnectionManagement::clear (const HostId & id) {
	mChannels.closeChannelsToHost(id);
}

void GenericConnectionManagement::setHostId (const HostId & hostId) {
	mChannels.setHostId(hostId);
	for (ChannelProviderMap::const_iterator i = mChannelProviders.begin(); i != mChannelProviders.end(); i++){
		i->second->setHostId (hostId);
	}
	mHostId = hostId;
}

void GenericConnectionManagement::startPing () {
	mChannelPinger.start();
}

void GenericConnectionManagement::stopPing () {
	mChannelPinger.stop();
}

Error GenericConnectionManagement::addChannelProvider  (ChannelProviderPtr channelProvider, int priority){
	if (mChannelProviders.find (priority) != mChannelProviders.end()){
		sf::Log (LogError) << LOGID << "Have already a ChannelProvider with priority " << priority << std::endl;
		return error::ExistsAlready;
	}
	mChannelProviders[priority] = channelProvider;
	channelProvider->channelCreated() = abind (dMemFun (this, &GenericConnectionManagement::onChannelCreated), priority);
	if (channelProvider->protocol()){
		Error e = mCommunicationMultiplex->addComponent (channelProvider->protocol());
		if (e) return e;
	}
	return NoError;
}

GenericConnectionManagement::ConnectionInfos GenericConnectionManagement::connections() const {
	return mChannels.connections();
}

Error GenericConnectionManagement::liftConnection (const HostId & hostId, const ResultCallback & callback, int timeOutMs) {
	return liftToAtLeast (1, hostId, callback, timeOutMs);
}

Error GenericConnectionManagement::liftToAtLeast  (int level, const HostId & hostId, const ResultCallback & callback, int timeOutMs) {
	AsyncOpId id = genFreeId ();
	LiftConnectionOp * op = new LiftConnectionOp (regTimeOutMs (timeOutMs));

	op->target = hostId;
	op->setId (id);
	op->setState (LiftConnectionOp::Start);
	op->callback = callback;
	op->minLevel = level;

	startLifting (op);
	return NoError;
}

Error GenericConnectionManagement::closeChannel (const HostId & host, int level) {
	ChannelId c = mChannels.findChannel (host, level);
	if (c == 0)
		return error::NotFound;
	return mChannels.close(c);
}

Error GenericConnectionManagement::send (const HostId & receiver, const sf::Datagram & datagram, const ResultCallback & callback) {
	ChannelId id = mChannels.findBestChannel(receiver);
	if (id == 0) return error::ConnectionError;
	return mChannels.send(id, datagram, true, callback);
}

Error GenericConnectionManagement::send (const HostSet & receivers, const sf::Datagram & datagram) {
	// TODO: Optimizing?
	Error result = NoError;
	for (HostSet::const_iterator i = receivers.begin(); i != receivers.end(); i++){
		Error err = send (*i, datagram);
		if (err && !result) result = err;
		else if (err && result)  result = error::MultipleErrors;
	}
	return result;
}

int GenericConnectionManagement::channelLevel (const HostId & receiver) {
	return mChannels.findBestChannelLevel(receiver);
}

void GenericConnectionManagement::startLifting (LiftConnectionOp * op){
	// do we have a channel?
	int level = mChannels.findBestChannelLevel(op->target);
	if (level == 0) {
		// create initial channel
		op->setState (LiftConnectionOp::CreateInitial);
		while (true) {
			ChannelProviderPtr provider = bestProvider (op->lastLevelTried - 1, true, &op->lastLevelTried);
			if (!provider)
				break;
			Error e = provider->createChannel(op->target, abind (dMemFun (this, &GenericConnectionManagement::onChannelCreate), true, op->id()), op->lastingTimeMs (0.66));
			if (!e) {
				addAsyncOp (op);
				return;
			}
		}
		notifyAsync (op->callback, error::CouldNotConnectHost);
		delete op;
		return;
	}
	// there is already a channel
	// start lifting
	lift (op);
}

void GenericConnectionManagement::lift (LiftConnectionOp * op) {
	int level = mChannels.findBestChannelLevel(op->target);
	if (level == 0) {
		Log (LogWarning) << LOGID << "Channel disappeared during lift" << std::endl;
		// channel disappeared, cannot lift anymore
		notifyAsync (op->callback, error::CouldNotConnectHost);
		delete op;
		return;
	}
	Log (LogInfo) << LOGID << mHostId << " continue lift to " << op->target << " current=" << level << " restMs=" << op->lastingTimeMs() << std::endl;
	op->setState (LiftConnectionOp::Lift);
	while (true) {
		ChannelProviderPtr provider = bestProvider (op->lastLevelTried - 1, false, &op->lastLevelTried);
		if (!provider)
			break; // no provider anymore
		if (op->lastLevelTried <= level)
			break; // won't get better
		Log (LogInfo) << LOGID << "Trying level con " << op->target << " to " << op->lastLevelTried << " current=" << level << std::endl;
		Error e = provider->createChannel (op->target, abind (dMemFun (this, &GenericConnectionManagement::onChannelCreate), false, op->id()), op->lastingTimeMs(0.66));
		if (!e) {
			addAsyncOp (op);
			return;
		}
	}
	// Could not go further
	// reached final level?
	Log (LogProfile) << LOGID << "Final leveling result to " << op->target << ", lasting time=" << op->lastingTimeMs() << "ms, reached level " << level << std::endl;
	if (level < op->minLevel) {
		notifyAsync (op->callback, error::CouldNotConnectHost);
	} else {
		notifyAsync (op->callback, NoError);
	}
	// stopping redundant connections
	mChannels.closeRedundantChannelsToHost (op->target);
	delete op;
	return;
}

void GenericConnectionManagement::onChannelCreate (Error result, bool wasInitial, AsyncOpId id) {
	LiftConnectionOp * op;
	if (wasInitial) {
		getReadyAsyncOpInState (id, LIFT_CONNECTION, LiftConnectionOp::CreateInitial, &op);
	} else {
		getReadyAsyncOpInState (id, LIFT_CONNECTION, LiftConnectionOp::Lift, &op);
	}
	if (!op) return;
	Log (LogInfo) << LOGID << "Leveled " << mHostId << " --> " << op->target << " to " << op->lastLevelTried << " initial=" << wasInitial << " resulted=" << toString (result) << std::endl;
	if (result) {
		if (wasInitial){
			op->setState (LiftConnectionOp::Start);
			startLifting (op);
		} else {
			lift (op);
		}
		return; // lift operations do error handling
	}
	if (wasInitial){
		// begin again (but this time not only initial channels)
		op->lastLevelTried = -1;
	}
	// begin again, does also error and result handling
	lift (op);
}

void GenericConnectionManagement::onChannelCreated (const HostId & target, ChannelPtr channel, bool requested, int level) {
	mChannels.add(channel, target, requested, level);
	mCommunicationMultiplex->distChannelChange(target);
	notify (mConDetailsChanged);
}

void GenericConnectionManagement::onIncomingDatagram (const HostId & source, const String & cmd, const Deserialization & ds, const ByteArray & data) {
#ifndef NDEBUG
	Log (LogInfo) << LOGID << mHostId << " recv " << cmd  << " " << ds << " (csize=" << data.size() << "from=" << source << ")" << std::endl;
#endif
	assert (mCommunicationMultiplex);
	mCommunicationMultiplex->dispatch(source, cmd, ds, data);
}

void GenericConnectionManagement::onChannelChanged (ChannelId id, const HostId & target, int level) {
	mCommunicationMultiplex->distChannelChange(target);
	notify (mConDetailsChanged);
}

GenericConnectionManagement::ChannelProviderPtr GenericConnectionManagement::bestProvider (int maxLevel, bool initial, int * level) {
	for (ChannelProviderMap::reverse_iterator i = mChannelProviders.rbegin(); i != mChannelProviders.rend(); i++){
		if (initial && !i->second->providesInitialChannels()) continue;
		if (maxLevel >= 0 && i->first > maxLevel) continue;
		if (level) *level = i->first;
		return i->second;
	}
	// not found
	return ChannelProviderPtr();
}


}
