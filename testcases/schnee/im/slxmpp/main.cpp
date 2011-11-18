#ifndef ENABLE_DNS_SD
#include <stdio.h>
int main (int argc, char * argv[]){
	printf ("SLXMPP not supported, so nothing to fail\n");
	return 0;
}
#else
#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/test/initHelpers.h>
#include <schnee/im/IMClient.h>
#include <schnee/tools/async/DelegateBase.h>


/**
 * @file
 * Stresses the SLXMPP connection a bit, as it is a often coming-back
 * regression that the SLXMPP subsystem seems not really threadsafe.
 */

using namespace sf;

class IMPeer : public DelegateBase {
public:
	IMPeer (int id) {
		SF_REGISTER_ME;
		mName = test::testNames(id);
		mClient = IMClient::create("slxmpp");
		mClient->messageReceived() = dMemFun (this, &IMPeer::onMessageReceived);
		mClient->setConnectionString (String ("slxmpp://" + mName));
		mConnectCameBack = false;
		mReceivedMessage = false;
	}
	
	~IMPeer () {
		SF_UNREGISTER_ME;
		mClient->disconnect();
		delete mClient;
	}
	
	
	void connect () {
		LockGuard guard (mMutex);
		mConnectCameBack = false;
		mClient->connect(dMemFun (this, &IMPeer::onConnectResult));
	}
	
	bool isConnected () {
		LockGuard guard (mMutex);
		return mClient->isConnected();
	}
	
	bool canSee (const String & other){
		LockGuard guard (mMutex);
		IMClient::Contacts contacts = mClient->contactRoster();
		if (contacts.count(other) == 0) return false;
		if (contacts[other].presences.empty()) return false;
		IMClient::PresenceState presenceState = contacts[other].presences.front().presence;
		if (presenceState == IMClient::PS_OFFLINE) return false;
		return true;
	}
	
	Error sendMessage (const IMClient::Message & msg){
		bool suc= mClient->sendMessage (msg);
		if (suc) return NoError;
		else return error::ConnectionError; // ??
	}
	
	bool receivedMessage () const {
		LockGuard guard (mMutex);
		return mReceivedMessage;
	}
	
	IMClient::Message lastReceivedMessage () {
		return mLastReceivedMessage;
	}
	
	String name () const {
		LockGuard guard (mMutex);
		return mName;
	}
	
	HostId hostId () {
		LockGuard guard (mMutex);
		return mClient->ownId();
	}
	
	void serialize (Serialization & s) const {
		LockGuard guard (mMutex);
		s ("connectCameBack", mConnectCameBack);
		s ("connectResult", mConnectResult);
		s ("receivedMessage", mReceivedMessage);
		s ("lastReceivedMessage", mLastReceivedMessage);
	}
	
private:
	
	void onConnectResult (Error e){
		LockGuard guard (mMutex);
		mConnectResult = e;
		mConnectCameBack = true;
	}
	
	void onMessageReceived (const IMClient::Message& message) {
		LockGuard guard (mMutex);
		mLastReceivedMessage = message;
		mReceivedMessage = true;
	}
	
	IMClient * mClient;
	Error      mConnectResult;
	bool       mConnectCameBack;
	mutable Mutex      mMutex;
	String     mName;
	IMClient::Message mLastReceivedMessage;
	bool       mReceivedMessage;
};

class IMScenario : public DelegateBase {
public:
	typedef std::vector <IMPeer*> PeerVec;
	IMScenario () {
		SF_REGISTER_ME;
	}
	
	~IMScenario () {
		SF_UNREGISTER_ME;
		for (PeerVec::const_iterator i = mPeers.begin(); i != mPeers.end(); i++){
			delete *i;
		}
	}

	/// Inits all IM Clients
	void init (int num) {
		mPeerCount = num; 
		mPeers.reserve (num);
		for (int i = 0; i < num; i++) mPeers.push_back (new IMPeer(i));
	
	}
	
	/// Let all IM Clients connect
	void allConnect () {
		for (int i = 0; i < mPeerCount; i++){
			mPeers[i]->connect ();
		}
	}
	
	// Wait until all peers connect
	// @return true: all connected in time
	bool waitForConnected (int timeOutMs) {
		return test::waitUntilTrueMs(dMemFun(this, &IMScenario::allConnected), timeOutMs);
	}
	
	bool waitForReceivedMessage (int timeOutMs, int except = -1) {
		function<bool (int)> f = dMemFun (this, &IMScenario::allReceivedMessage);
		return test::waitUntilTrueMs (bind (f, except), timeOutMs);
	}
	
	bool waitForAllSeeEachOther (int timeOutMs) {
		return test::waitUntilTrueMs (dMemFun (this, &IMScenario::allSeeEachOther), timeOutMs);
	}
	
	// Checks if all peers are connected
	bool allConnected () {
		for (int i = 0; i < mPeerCount; i++) {
			if (!mPeers[i]->isConnected()) return false;
		}
		return true;
	}
	
	bool allSeeEachOther() {
		for (int i = 0; i < mPeerCount; i++) {
				for (int j = 0; j < mPeerCount; j++) {
					if (i != j){
						if (!mPeers[i]->canSee (mPeers[j]->hostId())) return false;
				}
			}
		}
		return true;
	}
	
	// Check if all peers (except id, if id >= 0) received a message
	bool allReceivedMessage (int except = -1) {
		for (int i = 0; i < mPeerCount; i++) {
			if (i != except && !mPeers[i]->receivedMessage()) return false;
		}
		return true;
	}
	
	void dump () {
		for (int i =0 ; i < mPeerCount; i++) {
			printf ("Peer: %d: %s\n", i, toJSONEx (*(peer(i)), INDENT).c_str());
		}
	}
	
	int peerCount () { return mPeerCount; }
	
	IMPeer * peer (int i) { assert (i <= mPeerCount); return mPeers[i]; }
	
private:
	PeerVec mPeers;
	int mPeerCount;
};

void testInit () {
	IMScenario scenario;
	scenario.init(8);
}

void testConnect () {
	IMScenario scenario;
	scenario.init(8);
	scenario.allConnect();
	bool suc = scenario.waitForConnected(1000);
	tassert (suc, "All peers shall connect in time");
}

void sendMessageToAll () {
	IMScenario scenario;
	scenario.init(8);
	scenario.allConnect();
	bool suc = scenario.waitForConnected(1000);
	tassert (suc, "All peers shall connect in time");

	suc = scenario.waitForAllSeeEachOther(5000);
	if (!suc) scenario.dump();
	tassert (suc, "All peers shall see each other in time");
	
	String msgBody = "This is a nice testmessage";
	for (int i = 1; i < scenario.peerCount(); i++) {
		IMClient::Message m;
		m.to = scenario.peer(i)->hostId();
		m.body = msgBody;
		Error e = scenario.peer(0)->sendMessage(m);
		tassert1 (!e);
	}
	suc = scenario.waitForReceivedMessage(1000, 0);
	if (!suc) scenario.dump ();
	tassert (suc, "All should receive a message");
	for (int i = 1; i < scenario.peerCount(); i++){
		tassert (msgBody == scenario.peer(i)->lastReceivedMessage().body, "Should all receive the same message");
	}
}


int main (int argc, char * argv[]) {
	schnee::SchneeApp app (argc, argv);
	
	testInit ();
	// test::sleep(1); // SLXMPP needs some time to disconnect
	testConnect ();
	// test::sleep(1);
	sendMessageToAll ();
	return 0;
}
#endif
