#include <schnee/schnee.h>
#include <schnee/tools/Log.h>

#include <schnee/test/test.h>
#include <schnee/test/StandardScenario.h>

#include <flocke/sharedlists/SharedListTracker.h>
#include <flocke/sharedlists/SharedListServer.h>

/**
 * @file
 * Tests the ability to share a list of other sharings
 * and to track this lists of other peers.
 */

using namespace sf;

/// A SharedList Scenario
class SharedListScenario : public test::DataSharingScenario {
public:
	virtual ~SharedListScenario () {}

	struct SharedListPeer : public DataSharingPeer, public sf::DelegateBase {
		SharedListServer  * listServer;
		SharedListTracker * listTracker;

		SharedListPeer (InterplexBeacon * beacon) : DataSharingPeer (beacon) {
			SF_REGISTER_ME;
			listServer  = new SharedListServer  (server);
			listTracker = new SharedListTracker (client);
			Error e = listServer->init();
			tassert (!e, "Server must init");
		}

		~SharedListPeer () {
			SF_UNREGISTER_ME;
			delete listServer;
			delete listTracker;
		}

		/// Executes suc if result is NoError, notSuc otherwise
		void afterSuccessDo (sf::Error result, const sf::VoidDelegate& suc, const sf::VoidDelegate & notSuc) {
			if (!result) suc();
			else {
				if (notSuc) notSuc();
			}
		}
		
		void reportError (Error error, const sf::String & msg) {
			sf::Log (LogError) << LOGID << "Error " << toString (error) << " happened (" << msg << ")" << std::endl;
			tassert (false, "Should not come here");
		}
		
		sf::Error trackShared (const sf::HostId & other) {
			return listTracker->trackShared(other);
		}
		
		sf::Error autoLiftTrackShared (const sf::HostId & other) {
			if (beacon->connections().channelLevel(other) < 10){
				sf::VoidDelegate suc    = abind (sf::dMemFun (this, &SharedListPeer::trackShared), other);
				sf::VoidDelegate notSuc = abind (sf::dMemFun (this, &SharedListPeer::reportError), error::LiftError, sf::String ("Could not build connection in order to get files"));
				sf::ResultCallback cb = sf::abind (memFun (this, &SharedListPeer::afterSuccessDo), suc, notSuc);
				return beacon->connections().liftConnection(other, cb, 10000);
			}
			return trackShared(other);
		}
	};
	SharedListPeer * peer (int i) { return (SharedListPeer*) mPeers[i]; }
	SharedListPeer * server () { return (SharedListPeer*) mServer; }

	virtual Peer * createPeer (InterplexBeacon * beacon) { return new SharedListPeer (beacon); }
};

/// Try to automatic connect before tracking (was bug 83)
void testAutoConnect () {
	// TEST 0 (Bug #83)
	// Try to auto connect if not shared
	SharedListScenario s;
	tassert1 (!s.init(3, true, false)); // must be not simulated
	tassert1 (!s.connectThem ());
	
	sf::HostId shost = s.server()->hostId();
	sf::Error err = s.peer(0)->autoLiftTrackShared(shost);
	tassert (!err, "Shall connect automatically now");
	test::sleep(1);
	// should have data now
	tassert (s.peer(0)->listTracker->sharedLists().count(shost) > 0, "must have data from server now");
	tassert (s.peer(0)->fastConnected(s.server()), "shall use a lifted connection now"); // must be tcp or similar
	
	// now try this two at once
	err = s.peer(1)->autoLiftTrackShared(shost);
	err = s.peer(2)->autoLiftTrackShared(shost);
	test::sleep(5);
	tassert (s.peer(1)->fastConnected(s.server()), "must be fast connected now");
	tassert (s.peer(2)->fastConnected(s.server()), "must be fast connected now");
	tassert (s.peer(1)->listTracker->sharedLists().count (shost) > 0, "must have a list from the server");
	tassert (s.peer(2)->listTracker->sharedLists().count (shost) > 0, "must have a list from the server");
}

void testTrackingListSync () {
	SharedListScenario s;
	tassert (!s.initConnectAndLift(3), "Must init and connect");

	sf::HostId shost = s.server()->hostId();

	ds::DataDescription dirDesc;
	dirDesc.user  = "dir";
	ds::DataDescription fileDesc;
	fileDesc.user = "file"; 
	
	sf::Error err = s.server()->listServer->add ("A.file", SharedElement ("testbed/a", 1234, fileDesc));
	tassert1 (!err);
	err = s.server()->listServer->add ("emptyfile", SharedElement ("testbed/emptyfile", 5678, fileDesc));
	tassert1 (!err);
	err = s.server()->listServer->add ("B.dir", SharedElement ("testbed/dir1", 0, dirDesc));
	tassert1 (!err);

	err = s.peer(0)->listTracker->trackShared(shost);
	tassert (!err, "Should be no problem to track");
	test::millisleep (500);

	SharedListTracker::SharedListMap slm = s.peer(0)->listTracker->sharedLists();
	tassert1 (slm.count(shost) > 0);

	sf::SharedList list = slm[shost];
	tassert1 (list.count ("A.file"));
	tassert1 (list.count ("B.dir"));
	tassert (list["A.file"].desc.user == "file", "wrong user data");
	tassert (list["B.dir"].desc.user == "dir", "wrong user data");

	
	// 1.1. Must hold sync
	{
		for (int i = 0; i < 5; i++) {
			// Add a directory C
			err = s.server()->listServer->add ("C", SharedElement ("testbed/dir2"));
			tassert1 (!err);

			// Wait until full transfer
			test::millisleep (100);

			// Assume C blinks on the peer now
			slm = s.peer(0)->listTracker->sharedLists();
			tassert1 (slm[shost].count("C") > 0);

			// Unshare it again
			err = s.server()->listServer->remove ("C");
			tassert1 (!err);
			test::millisleep (100);

			// Assume C doesn't blink anymore
			slm = s.peer(0)->listTracker->sharedLists();
			tassert1 (slm[shost].count ("C") == 0);
		}
	}
	
	// must hold sync on clearance
	s.server()->listServer->clear ();
	test::millisleep (100);
	slm = s.peer(0)->listTracker->sharedLists();
	tassert (slm[shost].empty(), "Must be empty now");
	
	// must lost tracking
	s.server()->beacon->disconnect();
	test::millisleep (100);
	slm = s.peer(0)->listTracker->sharedLists();
	tassert (slm.count(shost) == 0, "Should delete the server from its tracking - server is offline");
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	testAutoConnect ();
	testTrackingListSync ();
	return 0;
}
