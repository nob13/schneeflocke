#include "MessagingImpl.h"
#include <schnee/tools/Log.h>

namespace sf {

Messaging * Messaging::create () {
	return new MessagingImpl ();
}

MessagingImpl::MessagingImpl (){

}

sf::Error MessagingImpl::send (const HostId & receiver, const sf::ByteArray & content){
	assert (mCommunicationDelegate);
	if (!mCommunicationDelegate) return error::NotInitialized;

	return mCommunicationDelegate->send (receiver, Datagram::fromCmd(Msg(), sf::createByteArrayPtr(content)));
}

void MessagingImpl::onRpc (const HostId & sender, const Msg & msg, const ByteArray & content) {
	if (mMessageReceived){
		mMessageReceived (sender, content);
	} else {
		sf::Log (LogError) << LOGID << "No message received delegate" << std::endl;
	}
}


}
