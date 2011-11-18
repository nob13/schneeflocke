#include "PingProtocol.h"

#include <schnee/tools/Log.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/Deserialization.h>
#include <stdio.h>

namespace sf {

sf::Error PingProtocol::sendPing (const sf::HostId & receiver){
	Ping ping;
	genPing (receiver, ping);
	sf::Error err = mCommunicationDelegate->send (receiver, Datagram::fromCmd(ping));
	if (err) {
		mMutex.lock ();
		mOpenPings.erase (PingKey (receiver, ping.id));
		mMutex.unlock ();
	}
	return err;
}

void PingProtocol::genPing (const sf::HostId & receiver, Ping & ping) {
	mMutex.lock ();
	ping.id = mNextId++;
	Time ct = currentTime ();
	mOpenPings[PingKey (receiver, ping.id)] = ct;
	mMutex.unlock ();
}

void PingProtocol::flushOld (int thresholdInMs) {
	mMutex.lock ();
	sf::Time t = sf::currentTime() - boost::posix_time::milliseconds(thresholdInMs);
	OpenPingMap::iterator i = mOpenPings.begin();
	while (i != mOpenPings.end()){
		if (i->second < t) { mOpenPings.erase(i++);}
		else i++;
	}
	mMutex.unlock ();
}

void PingProtocol::onRpc (const HostId & sender, const Ping & ping, const ByteArray & data) {
	Pong pong;
	pong.id = ping.id;
	mCommunicationDelegate->send (sender, Datagram::fromCmd(pong));
}

void PingProtocol::onRpc (const HostId & sender, const Pong & pong, const ByteArray & data) {
	sf::Time current = sf::currentTime ();
	sf::Time sentTime;
	if (pong.id == 0) return; // anonymous pong
	{
		sf::LockGuard guard (mMutex);
		PingKey key (sender, pong.id);
		OpenPingMap::iterator i = mOpenPings.find (key);
		if ( i == mOpenPings.end()){
			// may happen...
			// sf::Log (LogWarning) << LOGID << "Received pong with unknown key " << key.first << "," << key.second << " ignoring" << std::endl;
			return;
		}
		sentTime = i->second;
		mOpenPings.erase (key);
	}
	long microseconds = (current - sentTime).total_microseconds ();
	float diff = (float) microseconds / 1000000.0f;
	if (mPongReceivedDelegate) mPongReceivedDelegate (sender, diff, pong.id);
}


}
