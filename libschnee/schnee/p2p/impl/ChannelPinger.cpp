#include "ChannelPinger.h"
#include <schnee/tools/Log.h>

namespace sf {

ChannelPinger::ChannelPinger () {
	SF_REGISTER_ME;
	mHolder = false;

	mPingInterval   = 10000;
	mIsPinging = false;
	mNextPingId = 1;
	mPingTimeoutMs = 60000;
}

ChannelPinger::~ChannelPinger() {
	SF_UNREGISTER_ME;
	sf::cancelTimer(mPingDelayedCall);
}

void ChannelPinger::init (ChannelHolder * holder) {
	mHolder = holder;
}

void ChannelPinger::start () {
	if (mIsPinging) return;
	mIsPinging = true;
	mPingDelayedCall = xcallTimed (dMemFun (this, &ChannelPinger::onDoPinging), regTimeOutMs (10000));
}

void ChannelPinger::stop  () {
	if (mIsPinging) {
		sf::cancelTimer(mPingDelayedCall);
		mPingDelayedCall = TimedCallHandle();
		mIsPinging = false;
	}
}

void ChannelPinger::onDoPinging () {
	mPingDelayedCall = TimedCallHandle();
	if (!mHolder) {
		Log(LogError) << LOGID << "No holder!" << std::endl;
		mIsPinging = false;
		return;
	}
	typedef ConnectionManagement::ConnectionInfos  InfoVec;
	InfoVec infos = mHolder->connections();

	for (InfoVec::iterator i = infos.begin(); i != infos.end(); i++) {
		ChannelId channelId = i->id;
		const Channel::ChannelInfo & info = i->cinfo;
		if (!info.virtual_){
			PingProtocol::Ping ping;
			ping.id = mNextPingId++;;
			Error e = mHolder->send(channelId, Datagram::fromCmd(ping), false);
			if (!e) {
				mOpenPings[std::make_pair(ping.id, channelId)] = sf::currentTime();
			}
		}
	}

	// cleanup old pings
	// TODO: Could be slow.
	OpenPingMap::iterator i = mOpenPings.begin();
	Time ct = currentTime ();
	while (i != mOpenPings.end()) {
		if ((ct - i->second).total_milliseconds() > mPingTimeoutMs) {
			mOpenPings.erase (i++);
		} else
			i++;
	}
	mPingDelayedCall = xcallTimed (dMemFun (this, &ChannelPinger::onDoPinging), futureInMs (mPingInterval));
}

void ChannelPinger::onPong (ChannelId channelId, const PingProtocol::Pong & pong) {
	PingKey key (std::make_pair(pong.id, channelId));
	OpenPingMap::iterator i = mOpenPings.find (key);
	if (i == mOpenPings.end()) {
		Log (LogWarning) << LOGID << "Receaved unrequested ping?" << std::endl;
		return;
	}
	long microseconds = (sf::currentTime() - i->second).total_microseconds ();
	float seconds = (float) microseconds / 1000000.0f;
	notifyAsync (mMeasure, channelId, seconds);
}



}
