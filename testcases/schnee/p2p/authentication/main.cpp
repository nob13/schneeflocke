#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/p2p/InterplexBeacon.h>
#include <schnee/test/StandardScenario.h>

using namespace sf;

/**
 * Tests whether two clients know of authenticated connections
 */
int simpleAuth (test::StandardScenario::InitMode initMode){
	test::StandardScenario scenario;
	scenario.setAuthenticatedForSimulatedNetwork(true);
	Error e = scenario.initWithInitMode(initMode, 2, false);
	tcheck1(!e);
	scenario.enableAuthentication();
	e = scenario.connectThem(1000);
	tcheck1(!e);
	e = scenario.liftThem(1000);
	tcheck1(!e);

	// Check that connections are authenticated
	tcheck1 (scenario.peer(0)->beacon->connections().connectionInfo(scenario.peerId(1)).cinfo.authenticated == true);
	tcheck1 (scenario.peer(1)->beacon->connections().connectionInfo(scenario.peerId(0)).cinfo.authenticated == true);
	return 0;
}

int main (int argc, char * argv[]){
	schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	testcase (simpleAuth(test::StandardScenario::IM_SIMULATED));
	testcase (simpleAuth(test::StandardScenario::IM_SIMULATED_AND_TCP));
	testcase (simpleAuth(test::StandardScenario::IM_SIMULATED_AND_UDT));
	testcase_end();
}
