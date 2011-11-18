#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/test/StandardScenario.h>

#include <schnee/p2p/impl/GenericInterplexBeacon.h>
#include <schnee/p2p/channels/IMDispatcher.h>
#include <schnee/p2p/channels/UDTChannelConnector.h>
#include <schnee/p2p/channels/TCPChannelConnector.h>

using namespace sf;

/**
 * Test leveling facility in InterplexBeacon
 */
int leveling (const test::StandardScenario::BeaconCreator & beaconCreator) {
	test::StandardScenario scenario;
	Error e = scenario.initWithBeaconCreator (2, false, beaconCreator);
	tassert (!e, "Shall init the Scenario");
	e = scenario.connectThem (1000);
	tcheck (!e, "Shall Connect all Beacons");
	e = scenario.liftThem(5000);
	tcheck (!e, "Shall be possible to lift");
	tcheck (scenario.fastConnected(), "Shall be fast connected after lifting");
	bool suc = scenario.waitForNoRedundantConnections(10000);
	tcheck (suc, "Shall have no redundant connections within 10s");
	e = scenario.closeThem();
	tcheck (!e, "Shall be possible to close!");
	suc = scenario.waitForNoOpenConnections (10000);
	tcheck (suc, "Shall close all within 10sec");
	return 0;
}

/// A beacon which just can just use UDT to level
InterplexBeacon * createUdtInterplex () {
	GenericInterplexBeacon * beacon = new GenericInterplexBeacon ();
	shared_ptr<IMDispatcher> imDispatcher = shared_ptr<IMDispatcher> (new IMDispatcher);
	shared_ptr<UDTChannelConnector> udtConnector = shared_ptr<UDTChannelConnector> (new UDTChannelConnector);
	UDTChannelConnector::NetEndpoint echoServer ("62.48.92.13", 1234);
	udtConnector->setEchoServer(echoServer);

	beacon->setPresenceProvider (imDispatcher);
	beacon->connections().addChannelProvider(imDispatcher, 1);
	beacon->connections().addChannelProvider(udtConnector, 10);
	return beacon;
}

/// A beacon which just can just use TCP to level
InterplexBeacon * createTcpInterplex () {
	GenericInterplexBeacon * beacon = new GenericInterplexBeacon ();

	shared_ptr<IMDispatcher>  imDispatcher  = shared_ptr<IMDispatcher> (new IMDispatcher());

	shared_ptr<TCPChannelConnector> tcpConnector         = shared_ptr<TCPChannelConnector> (new TCPChannelConnector());
	tcpConnector->start();

	beacon->setPresenceProvider (imDispatcher);
	beacon->connections().addChannelProvider  (imDispatcher, 1);
	beacon->connections().addChannelProvider (tcpConnector, 10);
	return beacon;
}


int main (int argc, char * argv[]) {
	sf::schnee::SchneeApp app (argc, argv);
	int ret = 0;
	testcase (leveling(&createUdtInterplex));  // IM + UDT
	test::millisleep (1000); // needed for SLXMPP presence ?! (BUG!)
	testcase (leveling(&createTcpInterplex));  // IM + TCP
	test::millisleep (1000);
	testcase (leveling(&InterplexBeacon::createIMInterplexBeacon)); // production Interplex
	return ret;
}
