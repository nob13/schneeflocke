#include <schnee/schnee.h>
#include <schnee/settings.h>
#include <schnee/test/test.h>
#include <schnee/test/StandardScenario.h>

using namespace sf;

// Test leveling with initialized scenario
int leveling (test::StandardScenario::InitMode mode){
	test::StandardScenario scenario;
	Error e = scenario.initWithInitMode(mode, 2, false);
	tcheck1 (!e);
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

int main (int argc, char * argv[]) {
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;

	testcase_start();
	testcase (leveling (test::StandardScenario::IM_SIMULATED));
	testcase (leveling (test::StandardScenario::IM_SIMULATED_AND_TCP));
	testcase (leveling (test::StandardScenario::IM_SIMULATED_AND_UDT));
	testcase (leveling (test::StandardScenario::IM_FULLSTACK));
	testcase_end();
}
