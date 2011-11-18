#include "InterplexBeacon.h"
#include <schnee/tools/Log.h>
#include "impl/GenericInterplexBeacon.h"

#include "channels/IMDispatcher.h"
#include "channels/TCPChannelConnector.h"

#include "channels/UDTChannelConnector.h"

#include <schnee/settings.h>
namespace sf {

InterplexBeacon::InterplexBeacon () {

}

InterplexBeacon::~InterplexBeacon () {

}

InterplexBeacon * InterplexBeacon::createIMInterplexBeacon (){
	GenericInterplexBeacon * beacon = new GenericInterplexBeacon ();

	// Always use IM
	shared_ptr<IMDispatcher>  imDispatcher  = shared_ptr<IMDispatcher> (new IMDispatcher());
	beacon->setPresenceProvider (imDispatcher);
	beacon->connections().addChannelProvider  (imDispatcher, 1);

	if (!schnee::settings().disableUdt){
		shared_ptr<UDTChannelConnector> udtConnector = shared_ptr<UDTChannelConnector> (new UDTChannelConnector());
		UDTChannelConnector::NetEndpoint echoServer (schnee::settings().echoServer, schnee::settings().echoServerPort);
		Log (LogProfile) << LOGID << "Using echoserver: " << schnee::settings().echoServer << ":" << schnee::settings().echoServerPort << std::endl;

		udtConnector->setEchoServer(echoServer);
		beacon->connections().addChannelProvider (udtConnector, 10);
	} else
		Log (LogProfile) << LOGID << "Disabled UDT connections" << std::endl;

	if (!schnee::settings().disableTcp){
		shared_ptr<TCPChannelConnector> tcpConnector (new TCPChannelConnector());
		tcpConnector->start();
		beacon->connections().addChannelProvider(tcpConnector, 11);

	} else
		Log (LogProfile) << LOGID << "Disabled TCP connections" << std::endl;
	return beacon;
}

}
