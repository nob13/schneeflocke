#pragma once 
#include "XMPPConnection.h"
#include "XMPPStream.h"

#include <schnee/im/IMClient.h>
#include <schnee/tools/async/DelegateBase.h>

namespace sf {

/**
 * IMConnection implementation for XMPP. Encapsulates XMPPConnection and XMPPDispatcher
 */
class XMPPClient : public IMClient, public DelegateBase {
public:
	XMPPClient ();
	virtual ~XMPPClient ();
	
	// Implementation of IMClient
	void setConnectionString (const String & connectionString);
	void setPassword (const String & password);
	virtual void connect (const ResultDelegate & callback = ResultDelegate ());
	virtual void disconnect ();
	ContactInfo ownInfo ();
	String ownId ();
	virtual bool isAuthenticated () const {
		return mStream ? mStream->channelInfo().authenticated : false;
	}
	virtual ConnectionState connectionState () const {
		return mConnectionState;
	}
	virtual String errorMessage () const {
		return mErrorText;
	}

	virtual void setPresence (const PresenceState & state, const String & desc = "", int priority = 0);
	virtual Contacts contactRoster () const;
	virtual void updateContactRoster ();
	virtual bool sendMessage (const Message & msg);
	virtual Error requestFeatures (const HostId & dst, const FeatureCallback & callback);
	virtual Error setFeatures (const std::vector<String> & features);
	virtual Error setIdentity (const String & name, const String & type);


	Error subscribeContact (const sf::UserId & user);
	Error subscriptionRequestReply (const sf::UserId & user, bool allow, bool alsoAdd);
	Error cancelSubscription (const sf::UserId & user);
	Error removeContact (const sf::UserId & user);


	virtual SubscribeRequestDelegate & subscribeRequest () { return mContactAddRequestDelegate; }
	virtual ConnectionStateChangedDelegate & connectionStateChanged () {
		return mConnectionStateChangedDelegate;
	}
	virtual VoidDelegate & contactRosterChanged () { return mContactRosterChangedDelegate; }
	virtual MessageReceivedDelegate & messageReceived () { return mMessageReceivedDelegate; }
	virtual ServerStreamErrorDelegate & streamErrorReceived () { return mServerStreamErrorReceived; }


	
	/// Extracts bare jid from a full one
	static String fullJidToBareJid (const String & fullJid);
	/// Checks whether jid is a full or bare jid
	static bool isFullJid (const String & jid);
	/// Converts to bare jid if its full jid, if its already bare jid just returns
	static String bareJid (const String & jid) {
		if (isFullJid(jid)) return fullJidToBareJid (jid);
		else return jid;
	}
private:
	typedef shared_ptr<XMPPConnection> XMPPConnectionPtr;

	// Forbid copy constructor/operator
	XMPPClient (const XMPPClient&);
	XMPPClient & operator= (const XMPPClient&);
	
	// Callback for XMPPConnection::connect
	void onConnect (Error result, XMPPConnectionPtr& connector, const ResultCallback & originalCallback);
	void onConnectionStateChanged (ConnectionState state);

	void onIncomingRosterIq (const xmpp::RosterIq & result);
	void onRosterIqResult (Error result, const xmpp::Iq & iq, const XMLChunk & base);

	// regular handlers
	void onIncomingPresence (const xmpp::PresenceInfo & elem, const XMLChunk & base);
	void onIncomingMessage (const xmpp::Message & msg, const XMLChunk & base);
	void onIncomingIq (const xmpp::Iq & , const XMLChunk & base);
	void onIncomingStreamError (const String & text, const XMLChunk & base);
	void onStreamClosed ();
	void onStreamError ();
	void onAsyncCloseStream (); /// < Close stream asyncronously (on StreamErrors)

	// Login data
	XMPPConnection::XmppConnectDetails mConnectDetails;

	typedef shared_ptr<XMPPStream> XMPPStreamPtr;
	String mErrorText;
	XMPPStreamPtr mStream;

	Contacts 		 mContacts;
	
	// Delegates
	SubscribeRequestDelegate    mContactAddRequestDelegate;
	MessageReceivedDelegate 	 mMessageReceivedDelegate;
	VoidDelegate		  		 mContactRosterChangedDelegate;
	ServerStreamErrorDelegate    mServerStreamErrorReceived;
	ConnectionStateChangedDelegate mConnectionStateChangedDelegate;
	
	std::set<UserId> mToAuthorize; ///< Recently added Users which will get authorized automatically.
	std::vector<String> mFeatures; ///< Own features
	String mClientType;
	String mClientName;
	ConnectionState mConnectionState;
};

}

