#include "StandardScenario.h"

#include <schnee/tools/Log.h>

namespace sf {
namespace test {

StandardScenario::~StandardScenario(){
	for (PeerVec::iterator i = mPeers.begin(); i != mPeers.end(); i++){
		delete *i;
	}
	delete mServer;
}

sf::Error StandardScenario::init(int nodeCount, bool withServer, bool simulated) {
	this->mSimulated = simulated;
	if (simulated) {
		genDoubleStarNetwork (mNetwork, nodeCount, withServer, true);
		return initWithBeaconCreator (nodeCount, withServer, sf::bind (&test::createNetworkInterplexBeacon, &mNetwork, mCollector));
	} else {
		return initWithBeaconCreator (nodeCount, withServer, &test::createGenericInterplexBeacon);
	}
}

sf::Error StandardScenario::initWithBeaconCreator (int nodeCount, bool withServer, const BeaconCreator & beaconCreator) {
	this->mNodeCount = nodeCount;
	for (int i = 0; i < nodeCount; i++){
		Peer * p = createPeer (beaconCreator());
		mPeers.push_back (p);
		p->beforeConnect();
	}
	if (withServer) {
		mServer = createPeer (beaconCreator());
		mServer->beforeConnect();
	}
	return NoError;
}


sf::Error StandardScenario::connectThem (int timeOutMs) {
	String prefix = mSimulated ? "" : "slxmpp://";
	if (mServer){
		sf::Error err = mServer->beacon->connect (prefix + serverName());
		if (err) return err;
	}
	for (int i = 0; i < mNodeCount; i++) {
		sf::Error err = mPeers[i]->beacon->connect (prefix + testNames(i));
		if (err) return err;
	}
	// Connecting
	bool suc = waitUntilTrueMs (bind (&StandardScenario::allOnline, this), timeOutMs);
	if (!suc) return error::CouldNotConnectHost;

	// They have to see each other
	suc = waitUntilTrueMs (bind (&StandardScenario::peersSeeEachOther, this), timeOutMs);
	if (!suc) return error::ConnectionError;
	return NoError;
}


sf::Error StandardScenario::liftThem(int timeOutMs){

	// Lifting each other
	for (int i = 0; i < mNodeCount; i++) {
		for (int j = i + 1; j < mNodeCount; j++) {
			Error e = mPeers[i]->beacon->connections().liftConnection (mPeers[j]->hostId());
			if (e && e != error::NotSupported) {
				Log (LogWarning) << LOGID << "Lifting failed for " << mPeers[i]->hostId() << " to " << mPeers[j]->hostId() << ": " << toString(e) << std::endl;
				return e;
			}
		}
		if (mServer) {
			Error e = mPeers[i]->beacon->connections().liftConnection (mServer->beacon->presences().hostId());
			if (e && e != error::NotSupported) {
				Log (LogWarning) << LOGID << "Lifting failed for " << mPeers[i]->hostId() << " to " << mServer->hostId() << ": " << toString(e) << std::endl;
				return e;
			}
		}
	}
	bool suc = waitUntilTrueMs (bind (&StandardScenario::fastConnected, this), timeOutMs);
	if (!suc) {
		Log (LogWarning) << LOGID << "Could not lift all Peers within timeOut" << std::endl;
		return error::LiftError;
	}
	return NoError;
}

sf::Error StandardScenario::closeThem () {
	Error e = NoError;
	for (int i = 0; i < mNodeCount; i++) {
		typedef std::vector<ConnectionManagement::ConnectionInfo> ConVec;
		Peer * p = mPeers[i];
		ConVec infos = p->beacon->connections().connections();
		for (ConVec::const_iterator j = infos.begin(); j != infos.end(); j++) {
			Error f = p->beacon->connections().closeChannel(j->target,j->level);
			if (f) e = f;
		}
	}
	return e;
}

bool StandardScenario::noOpenConnections () const {
	for (int i = 0; i < mNodeCount; i++) {
		Peer * p = mPeers[i];
		if (!p->beacon->connections().connections().empty()) return false;
	}
	return true;
}

bool StandardScenario::waitForNoOpenConnections (int timeOutMs) const {
	return waitUntilTrueMs (bind (&StandardScenario::noOpenConnections, this), timeOutMs);
}

bool StandardScenario::noRedundantConnections () const {
	for (int i = 0; i < mNodeCount; i++) {
		Peer * p = mPeers[i];
		typedef std::vector<ConnectionManagement::ConnectionInfo> ConVec;
		ConVec cons = p->beacon->connections().connections();
		std::set<HostId> already;
		for (ConVec::const_iterator i = cons.begin(); i != cons.end(); i++) {
			if (already.count(i->target) > 0) return false;
			already.insert(i->target);
		}
	}
	return true;
}

bool StandardScenario::waitForNoRedundantConnections (int timeOutMs) const {
	return waitUntilTrueMs (bind (&StandardScenario::noRedundantConnections, this), timeOutMs);
}


sf::Error StandardScenario::initConnectAndLift (int nodeCount, bool withServer, bool simulated) {
	sf::Error err = init (nodeCount, withServer, simulated);
	if (err) return err;
	err = connectThem ();
	if (err) return err;
	err = liftThem ();
	return err;
}

bool StandardScenario::allOnline () const {
	for (int i = 0; i < mNodeCount; i++){
		if (!mPeers[i]->beacon->presences().onlineState() == sf::OS_ONLINE) return false;
	}
	if (mServer && !mServer->beacon->presences().onlineState() == sf::OS_ONLINE) return false;
	return true;
}

bool StandardScenario::peersSeeEachOther () const {
	for (int i = 0; i < mNodeCount; i++){
		for (int j = 0; j < mNodeCount; j++) {
			if (i != j)
				if (!(mPeers[i]->canSee (mPeers[j]))) return false;
		}
	}
	if (mServer){
		for (int i = 0; i < mNodeCount; i++) {
			if (!mServer->canSee (mPeers[i])) return false;
			if (!mPeers[i]->canSee (mServer)) return false;
		}
	}
	return true;
}


bool StandardScenario::fastConnected () const {
	for (int i = 0; i < mNodeCount; i++){
		for (int j = 0; j < mNodeCount; j++){
			if (i != j){
				if (!mPeers[i]->fastConnected(mPeers[j])) return false;
			}
		}
	}
	if (mServer){
		for (int i = 0; i < mNodeCount; i++){
			if (!mServer->fastConnected(mPeers[i])) return false;
		}
	}
	return true;
}

bool StandardScenario::waitForNoPendingData (int timeOutMs) const {
	if (!mSimulated) {
		Log (LogError) << LOGID << "waitForNoPendingData does not work in non-simulated networks" << std::endl;
		return false;
	}
	return waitUntilTrueMs (sf::bind (&noPendingData, mCollector), timeOutMs);
}


}
}

