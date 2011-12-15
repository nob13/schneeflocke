#include "XMPPClient.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/Serialization.h>
#include <assert.h>
#include <set>
#include <schnee/settings.h>
#include "DirectXMPPConnection.h"
#include "BoshXMPPConnection.h"

namespace sf {

XMPPClient::XMPPClient() {
	SF_REGISTER_ME;
	mConnectionState = CS_OFFLINE;
	mStream = shared_ptr<XMPPStream> (new XMPPStream ());
	mStream->incomingMessage()     = dMemFun (this, &XMPPClient::onIncomingMessage);
	mStream->incomingPresence()    = dMemFun (this, &XMPPClient::onIncomingPresence);
	mStream->incomingIq()          = dMemFun (this, &XMPPClient::onIncomingIq);
	mStream->incomingStreamError() = dMemFun (this, &XMPPClient::onIncomingStreamError);
	mStream->closed() = dMemFun (this, &XMPPClient::onStreamClosed);
	mStream->asyncError() = dMemFun (this, &XMPPClient::onStreamError);
}

XMPPClient::~XMPPClient() {
	SF_UNREGISTER_ME;
}

void XMPPClient::setConnectionString(const String & connectionString) {
	mConnectionString = connectionString;
	mPassword.clear();
	mToAuthorize.clear();
}

void XMPPClient::setPassword(const String & password) {
	mPassword = password;
}

void XMPPClient::connect(const ResultDelegate & callback) {
	XMPPConnectionPtr connector;
	if (schnee::settings().forceBoshXmpp) {
		connector = XMPPConnectionPtr (new BoshXMPPConnection());
	} else {
		connector = XMPPConnectionPtr (new DirectXMPPConnection());
	}
	connector->connectionStateChanged() = dMemFun (this, &XMPPClient::onConnectionStateChanged);

	connector->setConnectionString(mConnectionString);
	if (!mPassword.empty()){ // overwriting password in connection string...
		connector->setPassword(mPassword);
	}

	Error e = connector->connect (mStream, 10000, abind (dMemFun (this, &XMPPClient::onConnect), connector, callback));
	if (e) xcall (abind (callback, e));
}

void XMPPClient::disconnect() {
	if (mStream) {
		mStream->close();
		mStream.reset();
	} else {
		Log (LogWarning) << LOGID << "Not connected" << std::endl;
	}
}

IMClient::ContactInfo XMPPClient::ownInfo() {
	String fullJid = mStream->ownFullJid();
	String bareJid = fullJidToBareJid(fullJid);
	Contacts::iterator i = mContacts.find(bareJid);
	if (i != mContacts.end()) {
		return i->second;
	}
	ContactInfo info;
	info.id = bareJid; // we have no online information
	return info;
}

String XMPPClient::ownId () {
	return mStream->ownFullJid();
}

void XMPPClient::setPresence (const PresenceState & state, const String & desc, int priority) {
	xmpp::PresenceInfo p;
	p.state  = state;
	p.status = desc;
	p.priority = priority;
	mStream->sendPresence(p);
}

IMClient::Contacts XMPPClient::contactRoster() const {
	return mContacts;
}

struct RosterIq : public xmpp::Iq {
	virtual String encode () const {
		return "<iq id='" + id + "' type='get'><query xmlns='jabber:iq:roster'/></iq>";
	}
};

void XMPPClient::updateContactRoster() {
	if (!isConnected()) {
		Log (LogInfo) << LOGID << "Not connected, aborting" << std::endl;
		return;
	}
	RosterIq iq;
	mStream->requestIq(&iq, dMemFun (this, &XMPPClient::onRosterIqResult));
}

bool XMPPClient::sendMessage(const Message & msg) {
	if (!isConnected()) {
		Log (LogWarning) << LOGID
				<< "No connection, will throw message away";
		return false;
	}
	xmpp::Message m (msg);
	mStream->sendMessage(m);
	return true;
}

// Some minimal checking of a user id (having an @, does not contain any \")
static bool checkUserId (const UserId & user) {
	if (user.find('\"') != user.npos) return false;
	if (user.find('@')  == user.npos) return false;
	return true;
}


struct RequestFeatureIq : public xmpp::Iq {
	virtual String encode () const {
		return "<iq id='" + id + "' to='" + to + "' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>";
	}
};

static void onRequestFeatureResult (const Error result, const xmpp::Iq & iq, const XMLChunk & base, const IMClient::FeatureCallback & originalCallback){
	// decoding response
	if (!originalCallback) return;
	typedef std::vector<String> StringVec;

	xmpp::DiscoInfoIq disco;
	IMClient::FeatureInfo info;
	if (result) return originalCallback (result, info);
	if (!disco.decode(base)) return originalCallback (error::BadDeserialization, info);
	info.features = disco.features;
	info.clientName = disco.clientName;
	info.clientType = disco.clientType;
	return originalCallback (NoError, info);
}

Error XMPPClient::requestFeatures (const HostId & dst, const FeatureCallback & callback) {
	if (!isConnected()) {
		Log (LogInfo) << LOGID << "Not connected, aborting" << std::endl;
		return error::ConnectionError;
	}
	if (!checkUserId (dst)) return error::InvalidArgument;
	xmpp::DiscoInfoIq iq;
	iq.to = dst;
	iq.type = "get";
	mStream->requestIq(&iq, abind (&onRequestFeatureResult, callback));
	return NoError;
}

Error XMPPClient::setFeatures (const std::vector<String> & features) {
	mFeatures = features;
	return NoError;
}

Error XMPPClient::setIdentity (const String & name, const String & type) {
	mClientName = name;
	mClientType = type;
	return NoError;
}


Error XMPPClient::subscribeContact (const sf::UserId & user) {
	return subscribeContact_locked (user);
}

Error XMPPClient::subscriptionRequestReply (const sf::UserId & user, bool allow, bool alsoAdd) {
	if (!isConnected()) {
		return error::ConnectionError;
	}
	if (!checkUserId (user))
		return error::InvalidArgument;

	const char * type = allow ? "subscribed" : "unsubscribed";

	xmpp::PresenceInfo p;
	p.to   = user;
	p.from = mStream->ownFullJid();
	p.type = type;

	mStream->sendPresence(p);

	if (alsoAdd && allow) {
		subscribeContact_locked (user);
	}
	return NoError;
}

Error XMPPClient::cancelSubscription (const sf::UserId & user) {
	if (!isConnected()) {
		return error::ConnectionError;
	}
	if (!checkUserId (user)) return error::InvalidArgument;
	const char * type = 0;
	Contacts::const_iterator i = mContacts.find (user);
	if (i != mContacts.end()) {
		// removing from contact list
		const ContactInfo & ci (i->second);
		switch (ci.state) {
		case SS_NONE:
			return error::NotFound;
			break;
		case SS_TO:
			type = "unsubscribe"; // "requesting for unsubscription"
			break;
		case SS_FROM:
			type = "unsubscribed";
			break;
		case SS_BOTH:
			type = "unsubscribed";
			break;
		}
	}
	if (!type) type = "unsubscribed";
	mToAuthorize.erase (user);

	xmpp::PresenceInfo p;
	p.from = mStream->ownFullJid();
	p.to = user;
	p.type = type;
	mStream->sendPresence(p);
	return NoError;
}

Error XMPPClient::removeContact (const sf::UserId & user) {
	return removeContact_locked (user);
}

String XMPPClient::fullJidToBareJid(const String & fullJid) {
	// split at '/'
	if (fullJid.empty())
		return String();
	String::size_type i = fullJid.find('/');
	if (i == fullJid.npos) {
		Log (LogWarning) << LOGID << fullJid << " is not a valid full jid" << std::endl;
		return fullJid;
	}
	return String(fullJid.begin(), fullJid.begin() + i);
}

bool XMPPClient::isFullJid (const String & jid) {
	return jid.find ('/') != jid.npos;
}

struct RemoveContactIq : public xmpp::Iq {
	String whom;
	String encode () const {
		return "<iq type='set' id='" + id + "'><query xmlns='jabber:iq:roster'><item jid='" + whom + "' subscription='remove'/></query></iq>";
	}
};

Error XMPPClient::subscribeContact_locked (const sf::UserId & user) {
	if (!isConnected()){
		return error::ConnectionError;
	}
	if (!checkUserId (user))
		return error::InvalidArgument;

	const char * type = 0;
	{
		Contacts::const_iterator i = mContacts.find (user);
		if (i != mContacts.end()){
			const ContactInfo & ci (i->second);
			switch (ci.state) {
			case SS_NONE:
					type = "subscribe";	 // regular path
				break;
			case SS_TO:
					type = "subscribed"; // just grant subscription (we already have others data)
				break;
			case SS_FROM:
					type = "subscribe";  // plain request
				break;
			case SS_BOTH:
					return error::ExistsAlready; // already subscribed
				break;
			default:
				assert (!"may not go here");
				break;
			}
		}
		if (!type)
			type = "subscribe"; // did not existed yet.
		mToAuthorize.insert (user);
	}
	xmpp::PresenceInfo p;
	p.to   = user;
	p.from = mStream->ownFullJid();
	p.type = type;
	mStream->sendPresence(p);
	return NoError;
}

Error XMPPClient::removeContact_locked (const UserId & user) {
	// if (!mConnection.isConnected()) return error::ConnectionError;
	if (!checkUserId (user)) return error::InvalidArgument;
	Contacts::const_iterator i = mContacts.find(user);
	if (i == mContacts.end()) return error::NotFound;

	RemoveContactIq iq;
	iq.whom = user;
	return mStream->requestIq(&iq);
}

void XMPPClient::onConnect (Error result, const XMPPConnectionPtr& connector, const ResultCallback & originalCallback) {
	if (!result) {
		mConnectionState = CS_CONNECTED;
		notifyAsync (mConnectionStateChangedDelegate, mConnectionState);
	} else {
		mErrorText = connector->errorText();
	}
	if (originalCallback) {
		originalCallback (result);
	}
}

void XMPPClient::onConnectionStateChanged (ConnectionState state) {
	mConnectionState = state;
	// forward
	if (mConnectionStateChangedDelegate) mConnectionStateChangedDelegate(state);
}

void XMPPClient::onIncomingRosterIq(const xmpp::RosterIq & rosterIq) {
	bool deleteNotFound = false;
	if (rosterIq.type == "result"){
		deleteNotFound = true; // Behavior on full reload.
	}
	if (rosterIq.type != "result" && rosterIq.type != "set") {
		Log (LogWarning) << LOGID << "Strange RosterIq type: " << rosterIq.type << std::endl;
	}

	std::set<String> existingJids;
	for (std::vector<xmpp::RosterIq::Item>::const_iterator i = rosterIq.items.begin(); i != rosterIq.items.end(); i++) {
		const xmpp::RosterIq::Item & item = *i;

		String bareJid = item.jid;
		Log (LogInfo) << LOGID << "(RemoveMe) Item " << toJSON (item) << std::endl;
		if (item.remove) {
			Log (LogInfo) << LOGID << "Removing " << bareJid << std::endl;
			mContacts.erase(bareJid);
		} else {
			ContactInfo & info = mContacts[bareJid];
			info.id    = item.jid;
			info.name  = item.name;
			info.state = item.subscription;
			info.waitForSubscription = item.waitForSubscription;
			if (info.waitForSubscription) {
				// trust the server
				// he saves that over session borders for us
				mToAuthorize.insert(bareJid);
			}
			info.hide = false;
			existingJids.insert (bareJid);
		}
	}
	if (deleteNotFound) {
		Contacts::iterator i = mContacts.begin();
		while (i != mContacts.end()){
			if (existingJids.count(i->first) == 0){
				// remove that contact, doesn't exist anymore
				mContacts.erase(i++);
				continue;
			} else
				i++;
		}
	}
	if (mContactRosterChangedDelegate) mContactRosterChangedDelegate();
	Log (LogInfo) << LOGID << "Ready updating roster\n" << toJSONEx (mContacts, sf::COMPRESS | sf::COMPACT) << std::endl;
}

void XMPPClient::onRosterIqResult (Error result, const xmpp::Iq & iq, const XMLChunk & base) {
	// mapping to default handler.
	if (!result) onIncomingIq (iq, base);
}

void XMPPClient::onIncomingPresence(const xmpp::PresenceInfo & elem, const XMLChunk & base) {
	if (elem.type == "subscribe"){
		bool toAuthorize = false;

		// We added it?
		toAuthorize = mToAuthorize.count (bareJid (elem.from)) > 0;
		if (toAuthorize) mToAuthorize.erase (bareJid (elem.from));
		// Ask the user?
		if (!toAuthorize && mContactAddRequestDelegate) {
			mContactAddRequestDelegate (elem.from);
			return;
		} else {
			Log (LogWarning) << LOGID << "No contact add request handler, denying " << elem.from << std::endl;
		}
		subscriptionRequestReply (elem.from, toAuthorize, false);
		return;
	}
	if (elem.type == "unsubscribe" || elem.type == "unsubscribed") {
		xmpp::PresenceInfo p;
		p.from = elem.to;
		p.to = elem.from;
		p.type = "unsubscribed";
		mStream->sendPresence(p);

		mToAuthorize.erase (bareJid (elem.from));
		// calling some delegate?
		removeContact (bareJid (elem.from));
		return;
	}
	if (elem.type == "error") {
		if (isFullJid (elem.from)){
			// we add error info to the user, thus error presence from a host is a bit wired.
			Log (LogWarning) << LOGID << "Strange error message from full jid " << elem.from << ":" << toJSON (elem) << std::endl;
			return;
		}
		{
			if (mContacts.count(elem.from) == 0) {
				mContacts[elem.from].hide = true;
			}
			ContactInfo & info = mContacts[elem.from];
			if (info.id.empty()) info.id   = elem.from;
			info.error     = true;
			info.errorText = elem.errorText;
		}
		if (mContactRosterChangedDelegate) mContactRosterChangedDelegate();
		return;
	}

	// regular presence...
	bool change = false;

	/*
	 * PresenceInfos from not full jid are usually offline
	 * informations. We add them to the contact list but with
	 * no active clients connected to
	 */

	String bareJid;
	bool containsFullJid;
	if (!isFullJid (elem.from)){
		containsFullJid = false;
		bareJid = elem.from;
	} else {
		bareJid = fullJidToBareJid (elem.from);
		containsFullJid = true;
	}

	if (mContacts.find (bareJid) == mContacts.end()) {
		if (elem.type == "error" || elem.state == PS_OFFLINE){
			// don't care.
			return;
		}
		Log (LogInfo) << LOGID << bareJid << " doesn't exist in roster so far, inserting it as hidden" << std::endl;
		change = true;
		ContactInfo & info (mContacts[bareJid]);
		info.hide = true;
		info.state = SS_NONE;
	}
	ContactInfo & info = mContacts[bareJid];
	if (info.id.empty()){
		info.id = bareJid;
	}

	if (containsFullJid) {
		// Modifying state of the peer, we have full jid

		bool found = false;
		for (std::vector<ClientPresence>::iterator i = info.presences.begin(); i != info.presences.end(); i++) {
			if (i->id == elem.from) {
				found = true;
				if (i->presence != elem.state) {
					if (elem.state == IMClient::PS_OFFLINE) {
						// client went offline, removing it from list
						info.presences.erase(i);
						change = true;
					} else {
						// client just changed state
						i->presence = elem.state;
						change = true;
					}
				}
				break;
			}
		}
		if (!found) {
			// client is new
			ClientPresence p;
			p.id       = elem.from;
			p.presence = elem.state;
			info.presences.push_back (p);
			change = true;
		}
	}
	if (change && mContactRosterChangedDelegate) {
		mContactRosterChangedDelegate ();
	}
}

void XMPPClient::onIncomingMessage(const xmpp::Message & msg, const XMLChunk & base) {
	if (mMessageReceivedDelegate) mMessageReceivedDelegate(msg);
}

void XMPPClient::onIncomingIq (const xmpp::Iq & iq, const XMLChunk & base) {
	const XMLChunk * query = base.getHasChild ("query");
	if (query) {
		if (query->ns() == "jabber:iq:roster"){
			xmpp::RosterIq result;
			if (!result.decode(base)) {
				Log (LogWarning) << LOGID << "Could not decode iq roster iq" << base << std::endl;
			}
			Log (LogInfo) << LOGID << "Decoded roster iq " << toJSONEx (result, sf::COMPRESS) << std::endl;
			onIncomingRosterIq (result);
			return;
		}
		if (iq.type == "get" && query->ns() == "http://jabber.org/protocol/disco#info") {
			xmpp::DiscoInfoIq result;
			result.type = "result";
			result.features = mFeatures;
			result.id = iq.id;
			result.to = iq.from;
			result.clientName = mClientName;
			result.clientType = mClientType;
			mStream->sendPlainIq(result);
			return;
		}
	}
	Log (LogWarning) << LOGID << "Unhandled iq: " << base << std::endl;
	if (iq.type == "get" || iq.type == "set") {
		// see rfc 3921 2.2
		Log (LogWarning) << LOGID << "Sending fail message" << std::endl;
		xmpp::ErrorIq result;
		result.id   = iq.id;
		result.errorCondition = "<service-unavailable/>";
		result.errorType = "cancel";
		mStream->sendPlainIq (result);
	}
}

void XMPPClient::onIncomingStreamError (const String & text, const XMLChunk & base) {
	if (mServerStreamErrorReceived) mServerStreamErrorReceived (text);
}

void XMPPClient::onStreamClosed () {
	if (mConnectionStateChangedDelegate) mConnectionStateChangedDelegate (CS_OFFLINE);
}

void XMPPClient::onStreamError () {
	xcall (dMemFun (this, &XMPPClient::onAsyncCloseStream));
	if (mConnectionStateChangedDelegate) mConnectionStateChangedDelegate (CS_ERROR);
}

void XMPPClient::onAsyncCloseStream () {
	mStream->close();
}

}
