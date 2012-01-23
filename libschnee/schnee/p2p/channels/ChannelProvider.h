#pragma once

#include <schnee/net/Channel.h>
#include "../Authentication.h"

namespace sf {

class CommunicationComponent;

/**
 * ChannelProvider is the source of Channels to other Entities.
 *
 * Channels may be either initial (can be created from nothing) or
 * not - there have to be an existing channel already (like TCPDispatcher).
 */
class ChannelProvider {
public:

	virtual ~ChannelProvider ();

	/// Tries to create a channel; calls you back (only if not returning an Error)
	virtual sf::Error createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs = -1) = 0;

	/// Is the provider able to provide initial channels?
	virtual bool providesInitialChannels () = 0;

	/// If the channel provider needs an installed protocol, it can be provided here.
	/// Note: You may not delete this protocol, its' part of the ChannelProvider
	/// @return the needed protocol - 0 if there is no need for a protocol.
	virtual CommunicationComponent * protocol () { return 0;}

	/// Sets own host id, often necessary to accept/create connections
	virtual void setHostId (const sf::HostId & id) = 0;

	/// Binds with authentication
	virtual void setAuthentication (Authentication * auth) = 0;

	///@name Delegates
	///@{

	typedef sf::function<void (const HostId & target, ChannelPtr channel, bool requested)> ChannelCreationDelegate;

	/// A channel was created successfully (if not requested - the request flag will be false)
	virtual ChannelCreationDelegate & channelCreated () = 0;

	///@}

};

}
