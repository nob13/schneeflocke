#pragma once

#include "../Messaging.h"

namespace sf {
///@cond DEV

struct Msg {
	SF_AUTOREFLECT_SDC;
};

/// Messaging implementation
class MessagingImpl : public Messaging {
public:
	MessagingImpl ();

	// Implementation of Messaging
	virtual sf::Error send (const HostId& receiver, const sf::ByteArray & content);
	virtual MessageReceivedDelegate & messageReceived () { return mMessageReceived;}

	// Overriding component
	SF_AUTOREFLECT_RPC;
private:
	void onRpc (const HostId & sender, const Msg & msg, const ByteArray & content);
	MessageReceivedDelegate mMessageReceived;
};

///@endcond DEV
}
