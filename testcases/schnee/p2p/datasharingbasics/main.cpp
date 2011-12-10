#include <schnee/schnee.h>
#include "DataSharingBasicsScenario.h"
#include <schnee/test/timing.h>

/// @file
/// Test basic data sharing protocol / methods
using namespace sf;

// Simple share and request
int testRequest () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
	
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareData();
	Uri uri = scenario.peer(0)->uri();
	
	Error e = scenario.peer(1)->requestSync(uri);
	tcheck1 (!e);
	tcheck1 (* (scenario.peer(1)->receivedData) == scenario.peer(0)->sharedData);
	tcheck1 (scenario.peer(1)->lastRequestReply.revision == 1);
	return 0;
}

// Test that file not found is working
int testRequestNotFound() {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
	
	// peer0 shares nothing...
	
	Uri uri = scenario.peer(0)->uri();
	Error e = scenario.peer(1)->requestSync(uri);
	tcheck1 (e == error::NotFound);
	return 0;
}

// Test that subscription is working
int testSubscribe () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
	
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareData();
	
	Uri uri = scenario.peer(0)->uri();
	Error e = scenario.peer(1)->subscribeSync (uri);
	tcheck1 (!e);
	return 0;
}

// Test that subscription file-not-found is working
int testSubscribeNotFound () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
	
	// peer0 shares nothing...
	
	Uri uri = scenario.peer(0)->uri();
	Error e = scenario.peer(1)->subscribeSync (uri);
	tcheck1 (e == error::NotFound);
	return 0;
}

// Test Subscription - notify
int testSubscribeNotify() {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
	
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareData();
	
	Uri uri = scenario.peer(0)->uri();
	Error e = scenario.peer(1)->subscribeSync (uri);
	tcheck1 (!e);
	
	scenario.peer(1)->counter.mark();
	
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->updateData();
	
	scenario.peer(1)->counter.wait();
	tcheck (scenario.peer(1)->lastNotify.revision == 2, "Must get current revision");
	return 0;
}

// Test Subscription notify upon unShare
int testSubscribeNotifyOnUnshare () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
	
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareData();
	
	Uri uri = scenario.peer(0)->uri();
	Error e = scenario.peer(1)->subscribeSync (uri);
	tcheck1 (!e);
	
	scenario.peer(1)->counter.mark();
	
	scenario.peer(0)->unShareData(); // - Gets another notify
	scenario.peer(1)->counter.wait();
	tcheck1 (scenario.peer(1)->lastNotify.mark == ds::Notify::SubscriptionCancel);
	return 0;
}

// Test cancellation of subscriptions
int testSubscribeCancellation () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
	
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareData();
	
	Uri uri = scenario.peer(0)->uri();
	Error e = scenario.peer(1)->subscribeSync (uri);
	tcheck1 (!e);
	
	scenario.peer(1)->counter.mark();
	scenario.peer(0)->updateData();
	scenario.peer(1)->counter.wait();
	tcheck1 (scenario.peer(1)->lastNotify.revision == 2);
	
	scenario.peer(1)->cancelSubscription (uri);
	tcheck1 (scenario.waitForNoPendingData());
	
	scenario.peer(0)->updateData();
	tcheck1 (scenario.waitForNoPendingData());
	// would crash if there is an update (as SyncCounter::finish would be called, without someone waiting on it)
	// so: no crash == ok.
	tcheck1 (scenario.peer(1)->lastNotify.revision == 2);
	return 0;
}

// Test Simple Transmission
int testSimpleTransmission (){
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
		
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareData();
	
	Uri uri = scenario.peer(0)->uri();
	Error e = scenario.peer(1)->requestTransmissionSync(uri);
	tcheck1 (!e);
	tcheck1 (scenario.peer(1)->transmissionState == BasicDataSharingPeer::Finished);
	tcheck1 (*scenario.peer(1)->receivedData == scenario.peer(0)->sharedData);
	return 0;
}

//// Test Transmission Cancellation
int testTransmissionCancellation () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
		
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareOutstandingData();
	
	Uri uri = scenario.peer(0)->uri();
	scenario.peer(1)->counter.mark();
	Error e = scenario.peer(1)->requestTransmission(uri);
	
	tcheck1 (!e);
	
	scenario.peer(1)->cancelTransmission(uri, 1); // AsyncOpIds are starting with 1
	tcheck1 (scenario.waitForNoPendingData());    // should receive communication
	
	scenario.peer(0)->releaseOutstandingData();
	test::millisleep (250);

	// Should have received cancellation of data
	tcheck1 (scenario.peer(1)->transmissionState == BasicDataSharingPeer::Started);
	return 0;
}

//// Test Async transmission
int testAsyncTransmission () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
		
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareOutstandingData();
	
	Uri uri = scenario.peer(0)->uri();
	scenario.peer(1)->counter.mark();
	Error e = scenario.peer(1)->requestTransmission(uri);
	
	tcheck1 (!e);
	
	tcheck1 (scenario.waitForNoPendingData());    // should receive communication
	tcheck1 (scenario.peer(1)->transmissionState == BasicDataSharingPeer::Started);
	
	scenario.peer(0)->releaseOutstandingData();
	schnee::mutex().unlock();
	test::millisleep (250);
	schnee::mutex().lock();

	// Should have received all data now
	tcheck1 (scenario.peer(1)->transmissionState == BasicDataSharingPeer::Finished);
	tcheck1 (*scenario.peer(1)->receivedData == scenario.peer(0)->sharedData);
	return 0;
}

// Test sharing of sub data
int testSubData () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2,false,true);
		
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareSubData();
	
	Uri uri = scenario.peer(0)->uri();
	Path p  = uri.path() + "/subData";
	Uri finalUri = Uri (uri.host(), p);
	Error e = scenario.peer(1)->requestTransmissionSync(finalUri);
	tcheck1 (!e);
	tcheck1 (scenario.peer(1)->transmissionState == BasicDataSharingPeer::Finished);
	tcheck1 (*scenario.peer(1)->receivedData == scenario.peer(0)->sharedData);
	return 0;
}

// Test denying in request situations
int testDeny () {
	DataSharingBasicsScenario scenario;
	scenario.initConnectAndLift(2, false, true);
	scenario.peer(0)->generateShareData();
	scenario.peer(0)->shareData();

	Uri uri = scenario.peer(0)->uri();
	scenario.peer(0)->denyPermissions();
	Error e = scenario.peer(1)->requestSync(uri);
	tcheck1 (e == error::NoPerm);
	return 0;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	testcase_start();
	testcase (testRequest());
	testcase (testRequestNotFound());
	testcase (testSubscribe());
	testcase (testSubscribeNotFound());
	testcase (testSubscribeNotify());
	testcase (testSubscribeNotifyOnUnshare());
	testcase (testSubscribeCancellation());
	testcase (testSimpleTransmission());
	testcase (testTransmissionCancellation());
	testcase (testAsyncTransmission());
	testcase (testSubData());
	testcase (testDeny ());
	testcase_end();
}
