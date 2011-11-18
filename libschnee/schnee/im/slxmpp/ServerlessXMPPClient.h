#pragma once

#include <schnee/sftypes.h>
#include <schnee/im/IMClient.h>
#include <schnee/net/ZeroConfService.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/async/DelegateBase.h>

#include "ConnectionManager.h"
// #include "../xmpp/XMPPDispatcher.h"

///@cond DEV

namespace sf {

/**
 * IMClient for Serverless XMPP Messaging (aka iChat or Bonjour Chat).
 * 
 * Form of the connection string:
 * slxmpp://username
 * 
 * The Standard is XEP-0174.
 */
class ServerlessXMPPClient : public IMClient, public DelegateBase {
public:
	ServerlessXMPPClient ();
	virtual ~ServerlessXMPPClient ();
	
	// Implementation of IMClient
	virtual void setConnectionString (const String & connectionString);
	virtual ConnectionState connectionState () const { return mConnectionState; }
	virtual String errorMessage () const { return mErrorMessage; }
	virtual void connect (const ResultDelegate & callback);
	virtual void disconnect ();
	virtual ContactInfo ownInfo ();
	virtual String ownId ();
	virtual void setPresence (const PresenceState & state, const String & desc, int priority);
	virtual Contacts contactRoster () const;
	virtual void updateContactRoster ();
	virtual bool sendMessage (const Message & msg);

	virtual ConnectionStateChangedDelegate & connectionStateChanged () { return mConnectionStateChangedDelegate; }
	virtual VoidDelegate & contactRosterChanged () { return mContactRosterChangedDelegate; }
	virtual MessageReceivedDelegate & messageReceived () { return mConnectionManager.messageReceived();}
private:
	
	/// updates mRegisteredService
	void updateService ();
	/// updates connection state
	void connectionStateChange (ConnectionState state);
	/// called when there is an error in the connection
	void connectionError (const String & msg);

	/// Decodes Service into a ContactInfo
	ContactInfo decodeInfo (const ZeroConfService::Service & service);
	
	// Handler for ZeroConf Messages
	void onServiceOnline   (const ZeroConfService::Service & service);
	void onServiceOffline  (const ZeroConfService::Service & service);
	void onServiceUpdated  (const ZeroConfService::Service & service);
	
	
	// Delegates
	ConnectionStateChangedDelegate 	mConnectionStateChangedDelegate;
	VoidDelegate					mContactRosterChangedDelegate;
	MessageReceivedDelegate			mMessageReceivedDelegate;
	
	// Access to ZeroConf
	ZeroConfService mZeroConf;
	
	// State
	String mConnectionString;			///< Information about connection
	ConnectionState mConnectionState;	///< Current connection state
	String mErrorMessage;				///< Error information if there was one
	ContactInfo mInfo;					///< Info about our own
	ZeroConfService::Service mService;	///< Service which is to be registered by mZeroConf
	Contacts mContacts;					///< Contacts database
	
	// Submoduls
	ConnectionManager mConnectionManager;	///< Manages connection to the peers.
	
	static const String serviceType;	///< mDNS service Type (_presence._tcp)
};

}

///@endcond DEV
