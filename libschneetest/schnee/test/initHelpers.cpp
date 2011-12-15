#include "initHelpers.h"
#include "timing.h"
#include "test.h"
#include <schnee/tools/Log.h>
#include <schnee/p2p/impl/GenericInterplexBeacon.h>
#include "NetworkDispatcher.h"
#include "PseudoRandom.h"

namespace sf {
namespace test {

bool waitUntilTrue (const sf::function<bool ()>& testFunction, int maxSeconds){
	int seconds = 0;
	for (; seconds < maxSeconds && !testFunction (); seconds++){
		schnee::mutex().unlock();
		sf::test::sleep (1);
		schnee::mutex().lock();
	}
	return seconds < maxSeconds;
}

bool waitUntilTrueMs (const sf::function<bool ()>& testFunction, int maxMilliSeconds) {
	int milliSeconds = 0;
	for (; milliSeconds < maxMilliSeconds && !testFunction(); milliSeconds+=10){
		schnee::mutex().unlock();
		sf::test::millisleep(10);
		schnee::mutex().lock();
	}
	return milliSeconds < maxMilliSeconds;
}

/// Structure to check for pending data inside the work thread.
struct NoPendingData {
	sf::Condition condition;
	bool          done;
	long          value;
	sf::test::LocalChannelUsageCollectorPtr collector;

	void exec () {
		value = collector->pendingData();
		done = true;
		condition.notify_all ();
	}
};

bool noPendingData (sf::test::LocalChannelUsageCollectorPtr collector) {

	// Just being zero is not enough
	if (collector->pendingData() != 0) return false;
	// IOService (Work Thread) could still modify it.
	// So to be sure, we just let the IOService thread do the task for us ;)
	// Note this works if Messages are not send via multiple Xcalls
	// Which is not yet the case.
	static NoPendingData x; // static, because it must be alive when 2nd thread comes back
	x.done = false;
	x.collector = collector;
	sf::xcall (sf::bind (&NoPendingData::exec, &x));
	while (!x.done) x.condition.wait (schnee::mutex());
	return x.value == 0;
}

const char * testNames (int id) {
	const char * names[] = {"Alice","Bob","Carol","Dave", "Eve", "Fred", "Gustav", "Hank", "Ina", "John" };
	const int num = (int) (sizeof (names) / sizeof (const char*));
	if (id < 0 || id >= num) return 0;
	return names[id];
}

InterplexBeacon * createGenericInterplexBeacon () {
	return InterplexBeacon::createIMInterplexBeacon();
}

InterplexBeacon * createNetworkInterplexBeacon (test::Network * network, test::LocalChannelUsageCollectorPtr collector) {
	GenericInterplexBeacon * beacon = new GenericInterplexBeacon ();
	shared_ptr<test::NetworkDispatcher> networkDispatcher = shared_ptr<test::NetworkDispatcher> (new test::NetworkDispatcher(*network, collector));
	beacon->setPresenceProvider(networkDispatcher);
	beacon->connections().addChannelProvider (networkDispatcher, 10);
	return beacon;
}

static float randDelay (){
	int r = pseudoRandom (90) + 10;
	return 0.001f * (float) r;
}

void genStarNetwork (test::Network & network, int nodeCount, bool withServer, bool randomDelay) {

	for (int i = 0; i < nodeCount; i++){
		const char * testName = test::testNames(i);
		tassert (testName, "Not enough testnames");
		float d = randomDelay ? randDelay() : 0;
		network.addConnection (test::Connection (testName, "R1", d));
	}
	float d = randomDelay ? randDelay() : 0;
	if (withServer) network.addConnection (test::Connection (test::serverName(), "R1", d));
}

float smallRandDelay () {
	int r = pseudoRandom (50) + 30;
	return 0.0001f * (float) r;
}

float mediumRandDelay () {
	int r = pseudoRandom (10) + 5;
	return 0.001f * (float) r;
}

float workGroupRandDelay () {
	int r = pseudoRandom (30) + 30;
	return 0.001f * (float) r;
}

void genDoubleStarNetwork (test::Network & network, int nodeCount, bool withServer, bool randomDelay) {
	for (int i = 0; i < nodeCount; i++) {
		const char * testName = test::testNames(i);
		tassert (testName, "Not enough testnames");
		float d = randomDelay ?  smallRandDelay () : 0.0f;
		if (i % 2 == 0) network.addConnection (test::Connection (testName, "R1", d));
		else network.addConnection (test::Connection (testName, "R2", d));
	}
	float d = randomDelay ? workGroupRandDelay () : 0.0f;
	network.addConnection (test::Connection ("R1", "R0", d));
	d = randomDelay ? workGroupRandDelay () : 0.0f;
	network.addConnection (test::Connection ("R2", "R0", d));
	if (withServer) {
		d = randomDelay ? mediumRandDelay () : 0.0f;
		network.addConnection (test::Connection ("R0", "RS", d));
		d = randomDelay ? smallRandDelay () : 0.0f;
		network.addConnection (test::Connection ("RS", test::serverName(), d));
	}

}

}
}
