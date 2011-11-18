#pragma once
#include <schnee/p2p/CommunicationComponent.h>
#include <schnee/sftypes.h>

namespace sf {

/// A simple CommunicationComponent which enables Peers to simply send messages
/// Directly to each other. It uses the msg-Keyword.
struct Messaging : public CommunicationComponent {
	/// Creates a Messaging instance.
	static Messaging * create ();

	/// Send a generic message to the receiver
	virtual sf::Error send (const HostId& receiver, const sf::ByteArray & content) = 0;

	// Delegates
	typedef sf::function < void (const sf::String & sender, const sf::ByteArray & message)> MessageReceivedDelegate;

	/// Delegate which will be called if a message was received
	virtual MessageReceivedDelegate & messageReceived () = 0;
};

}
