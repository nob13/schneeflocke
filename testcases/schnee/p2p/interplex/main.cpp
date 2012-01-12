#include <schnee/schnee.h>
#include <schnee/settings.h>
#include <schnee/test/test.h>
#include <schnee/test/StandardScenario.h>
#include <schnee/test/NetworkDispatcher.h>
#include <schnee/tools/async/MemFun.h>

#include <schnee/p2p/impl/GenericInterplexBeacon.h>
#include <schnee/p2p/channels/IMDispatcher.h>
#include <schnee/p2p/channels/UDTChannelConnector.h>
#include <schnee/p2p/channels/TCPChannelConnector.h>

using namespace sf;

// Test leveling with initialized scenario
int leveling (test::StandardScenario * scenario){
	Error e = scenario->connectThem (1000);
	tcheck (!e, "Shall Connect all Beacons");
	e = scenario->liftThem(5000);
	tcheck (!e, "Shall be possible to lift");
	tcheck (scenario->fastConnected(), "Shall be fast connected after lifting");
	bool suc = scenario->waitForNoRedundantConnections(10000);
	tcheck (suc, "Shall have no redundant connections within 10s");
	e = scenario->closeThem();
	tcheck (!e, "Shall be possible to close!");
	suc = scenario->waitForNoOpenConnections (10000);
	tcheck (suc, "Shall close all within 10sec");
	return 0;
}


int levelingWithUdt () {
	test::StandardScenario scenario;
	Error e = scenario.initWithBeaconCreator(2, false, memFun (&scenario, &test::StandardScenario::createNetAndUdtBeacon), true);
	tcheck1 (!e);
	return leveling (&scenario);
}

int levelingWithTcp () {
	test::StandardScenario scenario;
	Error e = scenario.initWithBeaconCreator(2, false, memFun (&scenario, &test::StandardScenario::createNetAndTcpBeacon), true);
	tcheck1 (!e);
	return leveling (&scenario);
}

int levelingWithIMBeacon () {
	test::StandardScenario scenario;
	Error e = scenario.initWithBeaconCreator(2, false, &InterplexBeacon::createIMInterplexBeacon, false);
	tcheck1 (!e);
	return leveling (&scenario);
}

int main (int argc, char * argv[]) {
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;

	testcase_start();
	testcase (levelingWithUdt());
	testcase (levelingWithTcp());
	testcase (levelingWithIMBeacon());
	testcase_end();
}
