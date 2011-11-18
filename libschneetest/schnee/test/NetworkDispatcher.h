#pragma once

#include "Network.h"
#include <schnee/p2p/impl/PresenceProvider.h>
#include <schnee/p2p/channels/ChannelProvider.h>
#include <schnee/tools/async/DelegateBase.h>
#include "LocalChannel.h"


namespace sf {
namespace test {

/**
 * A PresenceProvider/ChannelProvider implementation for the 
 * Network Simulation class.
 * 
 * It connects to itself and marks it as online.
 * 
 * The protocol in the Connectionstring is ignored,
 * but the id is checked whether it exists on the given Network
 * instance.
 */
class NetworkDispatcher : public PresenceProvider, public ChannelProvider, public DelegateBase  {
public:
	/// Creates NetworkDispatcher connected to a network. If usageCollector is not 0
	/// all created LocalChannels will be bound to it.
	NetworkDispatcher (Network & network, LocalChannelUsageCollectorPtr usageCollector = LocalChannelUsageCollectorPtr());
	virtual ~NetworkDispatcher ();
	
	// Implementation of PresenceProvider
	virtual sf::Error connect(const sf::String & connectionString, const sf::String & password = "", const ResultDelegate & callback = ResultDelegate());
	virtual sf::Error waitForConnected();
	virtual void disconnect();
	virtual HostId hostId () const;
	virtual OnlineState onlineState () const;
	virtual OnlineState onlineState (const sf::HostId & host) const;
	virtual UserInfoMap users () const;
	virtual HostInfoMap hosts () const;
	virtual HostInfoMap hosts(const UserId & user) const;
	virtual HostInfo hostInfo (const HostId & host) const;

	
	virtual VoidSignal & peersChanged () { return mPeersChanged; }
	virtual ServerStreamErrorSignal & serverStreamErrorReceived() { static ServerStreamErrorSignal s; return s; }
	virtual OnlineStateChangedSignal & onlineStateChanged () { return mOnlineStateChanged; }

	// Implementation of ChannelProvider
	virtual sf::Error createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs = -1);
	virtual bool providesInitialChannels () { return true; }
	virtual CommunicationComponent * protocol () { return 0; /* no protocol */}
	virtual void setHostId (const sf::HostId & id) {}
	virtual ChannelCreationDelegate & channelCreated () { return mChannelCreated; }

private:
	// Pushs in a Network Channel from another NetworkDispatcher instance
	void pushChannel (const HostId & source, ChannelPtr channel);

	/// Gets called if peers changed
	void onPeersChanged ();
	bool mOnline;
	Network & mNetwork;
	HostId mHostId;
	
	VoidSignal mPeersChanged;
	OnlineStateChangedSignal mOnlineStateChanged;

	ChannelCreationDelegate mChannelCreated;
	LocalChannelUsageCollectorPtr mUsageCollector;
	
};

}
}
