#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/p2p/InterplexBeacon.h>
#include <schnee/test/StandardScenario.h>

using namespace sf;

/**
 * Sets whether two clients know of authenticated connections
 */
int simpleAuth (){
	test::StandardScenario scenario;
	scenario.setAuthenticatedForSimulatedNetwork(true);
	Error e = scenario.init(2, false, true);
	// bad API? They don't know their API before connecting?
	scenario.peer(0)->beacon->authentication().setIdentity(scenario.peerId(0));
	scenario.peer(0)->beacon->authentication().enable(true);
	scenario.peer(1)->beacon->authentication().setIdentity(scenario.peerId(1));
	scenario.peer(1)->beacon->authentication().enable(true);
	tcheck1(!e);
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
	testcase (simpleAuth());
	testcase_end();
}
