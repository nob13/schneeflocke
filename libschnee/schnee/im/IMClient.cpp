#include "IMClient.h"
#include <assert.h>

#include <boost/thread.hpp> // for waiting

#include "slxmpp/ServerlessXMPPClient.h"
#include "xmpp/XMPPClient.h"

namespace sf {

const char* IMClient::presenceStateToString (PresenceState  p) {
	switch (p){
		case PS_UNKNOWN: { return "online"; }
		case PS_CHAT: { return "chat";}
		case PS_AWAY: { return "away";}
		case PS_EXTENDED_AWAY: { return "extended away"; }
		case PS_DO_NOT_DISTURB: { return "do not disturb"; }
		case PS_OFFLINE: { return "offline";}
	}
	assert (false);
	return "unkown presence state";
}

const char* IMClient::messageTypeToString (MessageType t) {
	if (t == MT_NORMAL) return "normal";
	if (t == MT_CHAT) return "chat";
	if (t == MT_ERROR) return "error";
	if (t == MT_GROUPCHAT) return "groupchat";
	if (t == MT_HEADLINE) return "headline";
	assert (false);
	return "unknown message type";
}

std::set<String> IMClient::availableProtocols(){
	std::set<String> result;
#ifdef ENABLE_DNS_SD
	result.insert ("slxmpp");
#endif
	result.insert ("xmpp");
	return result;
}

IMClient * IMClient::create(const String & protocol){
	if (protocol == "xmpp") return new XMPPClient ();
#ifdef ENABLE_DNS_SD
	if (protocol == "slxmpp") return new ServerlessXMPPClient ();
#endif
	return 0;
}

}

