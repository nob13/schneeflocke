#include <schnee/schnee.h>
#include <schnee/test/test.h>
#include <schnee/test/initHelpers.h>
#include <schnee/im/xmpp/XMPPClient.h>
#include <flocke/hardcodedLogin.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/tools/ResultCallbackHelper.h>
using namespace sf;

/**
 * @file
 * Tests adding / removing contacts within XMPP.
 *
 * This testcase only works with hardcoded logins.
 */

/// Test peer for this testcase
class Peer : public DelegateBase {
public:
	Peer (const String& connectionString) {
		SF_REGISTER_ME;
		mClient = new XMPPClient ();
		mClient->setConnectionString(connectionString);
		mAcceptContacts = false;
		mClient->subscribeRequest() = dMemFun (this, &Peer::onContactAddRequest);
	}

	~Peer () {
		SF_UNREGISTER_ME;
		delete mClient;
	}

	bool connect () {
		ResultCallbackHelper helper;
		mClient->connect(helper.onResultFunc());
		bool suc = helper.waitReadyAndNoError(10000);
		if (!suc) {
			fprintf (stderr, "Could not connect %s", mClient->ownId().c_str());
		} else {
			mClient->setPresence (IMClient::PS_UNKNOWN, "");
		}
		return suc;
	}

	bool canSee (const String & other) {
		return mClient->contactRoster().count (other) > 0 &&
			   mClient->contactRoster() [other].presences.size() > 0 &&
			   !mClient->contactRoster()[other].hide;
	}

	bool canNotSee (const UserId & other) { return !canSee (other); }

	bool waitForCanSee (const String & other, int timeOutMs = 5000) {
		return test::waitUntilTrueMs(sf::bind (&Peer::canSee, this, other), timeOutMs);
	}

	bool waitForCanNotSee (const String & other, int timeOutMs = 5000) {
		return test::waitUntilTrueMs(sf::bind (&Peer::canNotSee, this, other), timeOutMs);
	}

	void updateRoster () {
		mClient->updateContactRoster();
	}

	UserId userId() {
		return mClient->ownInfo().id;
	}

	const XMPPClient * client () const { return this->mClient; }

	Error addContact (const UserId & other) {
		return mClient->subscribeContact(other);
	}

	Error removeContact (const UserId & other) {
		return mClient->removeContact (other); // also cancels subscription
	}

	void acceptContacts (bool v = true) {
		mAcceptContacts = v;
	}

	IMClient::Contacts contacts () {
		return mClient->contactRoster();
	}

private:
	void onContactAddRequest (const UserId & user) {
		mClient->subscriptionRequestReply(user,mAcceptContacts, true);
	}

	bool         mAcceptContacts;
	XMPPClient * mClient;
};

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	SF_SCHNEE_LOCK;
	Peer a (hardcodedLogin ("x1"));
	Peer b (hardcodedLogin ("x2"));
	if (!a.connect()) return 1;
	if (!b.connect()) return 2;
	a.updateRoster();
	b.updateRoster();

	// wait for a that it sees b.
	if (!a.waitForCanSee (b.userId())){
		fprintf (stderr, "A cannot see b!\n");
		fprintf (stderr, "Try to add it manually\n");
		// Add b
		b.acceptContacts(true);
		a.addContact (b.userId());
		bool suc = a.waitForCanSee (b.userId());
		if (!suc) {
			fprintf (stderr, "A cannot see b, even after adding it!");
			printf ("Roster of A: %s\n", toJSONEx (a.contacts(), INDENT).c_str());
			return 3;
		}
		b.acceptContacts(false);
	}

	// A can see B, now cancel the registration
	Error e = a.removeContact(b.userId());
	tassert (e == NoError, "removeContact failed!");

	if (!a.waitForCanNotSee (b.userId())){
		fprintf (stderr, "A can still see B, but has removed him from the contact?!\n");
		fprintf (stderr, "A's contact list: %s\n", toJSONEx (a.contacts(), INDENT).c_str());
		fprintf (stderr, "B's contact list: %s\n", toJSONEx (b.contacts(), INDENT).c_str());
		return 4;
	}
	if (!b.waitForCanNotSee (a.userId())){
		fprintf (stderr, "B can still see A, but should not have the right anymore?!\n");
		return 5;
	}
	test::sleep_locked (3);
	printf ("**** A will subscribe B Again ****\n");
	// ok, lets correct it again
	b.acceptContacts();
	e = a.addContact (b.userId());
	tassert (e == NoError, "addContact failed\n");
	// did it work?
	if (!a.waitForCanSee (b.userId())){
		fprintf (stderr, "A cannot see B again\n");
		fprintf (stderr, "As list: %s\n", toJSONEx (a.contacts(), INDENT).c_str());
		return 6;
	}
	if (!b.waitForCanSee (a.userId())){
		fprintf (stderr, "B cannot see A again\n");
		return 7;
	}

	return 0;
}
