#pragma once

#include <schnee/sftypes.h>
#include <schnee/Error.h>
#include <set>
#include <sfserialization/autoreflect.h>

namespace sf {


/**
 * The interface for a generic IM Client. Get's implemented by the different protocols.
 * 
 */
class IMClient {
public:
	
	/** The state a connection to an Instant Messaging Service may have */
	enum ConnectionState {
		CS_OFFLINE,			///< No connection
		CS_CONNECTING,			///< Currently connecting 
		CS_AUTHENTICATING,		///< Authenticating with other side
		CS_CONNECTED,			///< Connected, ready to read or write
		CS_ERROR				///< There was an error
	};
	
	/** Defines the presence of an contact (and of us) */
	enum PresenceState {
		PS_UNKNOWN,			///< Unknown presence state (regular online)
		PS_CHAT,				///< Chat (client wants to chat)
		PS_AWAY,				///< Client is away
		PS_EXTENDED_AWAY,		///< Client is extended away
		PS_DO_NOT_DISTURB,		///< Client does not want to be disturbed
		PS_OFFLINE				///< Offline
	};
	
	/// Converts a PresenceState to a String  (behaves like XMPP keywords)
	static const char* presenceStateToString (PresenceState state);
	
	/** Defines the type of a message */
	enum MessageType {
		MT_CHAT,
		MT_ERROR,
		MT_GROUPCHAT,
		MT_HEADLINE,
		MT_NORMAL // if no or unknown type given, the type MUST be assumed to be normal
	};
	
	/// Converts a MessageType to a String (behaves like XMPP keywords!)
	static const char* messageTypeToString (MessageType t);

	/// A presence info about one contact's client
	struct ClientPresence {
		String id;					///< The client id (e.g. full JID like nob\@shodan/schneeflocke)
		PresenceState presence;		///< The presence of this client
		SF_AUTOREFLECT_SERIAL;
	};
	/// Different presences of the same contact
	typedef std::vector<ClientPresence> ClientPresences;
	
	enum SubscriptionState { 
		SS_NONE,			///< No subscription 
		SS_TO, 			///< User has subscription to Contact
		SS_FROM, 			///< Contact has subscription to User
		SS_BOTH 			///< Both subscribed to each other
	};
	
	/// Identifies a contact, for contact list
	struct ContactInfo {
		ContactInfo () : state (SS_TO), waitForSubscription (false), error(false), hide (false) {}
		String id;					///< contact id (in jabber: bare JID like nob\@shodan)
		String name;				///< If there is a human readable name given
		SubscriptionState state;
		bool waitForSubscription;	///< Wait for other to accept subscription
		bool error;					///< Some error about this info
		String errorText;			///< Error message
		bool hide;					///< The contact is not yet inside the roster, but we already gathered some info
		ClientPresences presences;	///< The logged in clients of that contact along with their presence state
		SF_AUTOREFLECT_SERIAL;
	};
	
	/// Base class for everything we send through the IM Service)
	struct Stanza {
		String from;		///< from (full id) must be set if received
		String to;			///< to (full id) must be set if we send something
		SF_AUTOREFLECT_SERIAL;
	};
	

	/// A message (as we support it)
	struct Message : public Stanza {
		Message () { type = MT_NORMAL; }
		MessageType type;	///< type of the message (default = MT_NORMAL)
		String id;			///< id of the message, may be sent (if need by the type)
		String subject;	///< subject of the message, may be sent
		String body;		///< body of the message, may be sent
		SF_AUTOREFLECT_SERIAL;
	};
	
	/// Contacts is a contact list
	/// String is the contact identifier, like Jabber bare JID or ICQ#
	typedef std::map<String, ContactInfo> Contacts;
	
	virtual ~IMClient () {}

	/// @name Creation
	/// @{
	/**
	 * Returns all available protocols, e.g. xmpp or slxmpp
	 */
	static std::set<String> availableProtocols ();
	
	/**
	 * Creates an IMClient with the specified protocol, returns 0 if there was no such.
	 */
	static IMClient * create (const String & protocol);

	/// @}
	
	/// @name Login Data 
	/// @{
	
	/// A login string (like xmpp://username\@server[:port][/resource]
	virtual void setConnectionString (const String & connectionString) {}
	
	/// Sets the connection password / secret
	virtual void setPassword (const String & connectionPassword) {}
	
	/// @}
	
	/// @name Connection 
	/// @{
	
	/// Returns whether it is currently connected
	virtual bool isConnected () const { return connectionState () == CS_CONNECTED; }
	
	/// returns current state of connection
	virtual ConnectionState connectionState () const = 0;

	/// If there was an error (in connection state), you can get an error message here.
	virtual String errorMessage () const { return "Unknown Error"; }
	
	/// A callback delegate informaing about the result of connecting process
	typedef function <void (sf::Error)> ResultDelegate;
	/// Connect to the IM Network
	virtual void connect (const ResultDelegate & callback = ResultDelegate ()) = 0;

	/**
	 * Blocks current thread until there is a connection
	 *
	 * Note if nobody calls connect it will wait very long...
	 *
	 * Waiting is done in 10ms steps with usleep and will take a bit longer
	 *
	 * @param timeOutMs how long to wait?
	 * @return returns true if there is a succesfull connection or false in case of an error/timeout
	 */
	virtual bool waitForConnected (int timeOutMs = 30000);

	/// Disconnect from the IM Network
	virtual void disconnect () = 0;
	
	/// @}
	
	/// @name Instant Messaging 
	/// @{
	
	/// Returns own contact information (if available)
	virtual ContactInfo ownInfo () = 0;
	
	/// Returns own full qualified (full jabber) id.
	virtual String ownId () = 0;
	
	/// Set's our own presence state (does not have to be implemented)
	/// Note: Some IMServices (XMPP!) need this in order that you get presence information from other entities.
	/// @param state - current state
	/// @param desc  - additional description (can be empty)
	virtual void setPresence (const PresenceState & state, const String & desc = String (), int priority = 0) = 0;
	
	/// Returns the current tracked contact list
	virtual Contacts contactRoster () const = 0;
	
	/// Updates the info about connected contacts (usually only necessary for one time, shall be detected automatically)
	virtual void updateContactRoster () = 0;
	
	/// Sends a message through the IM service (asynchronous!)
	/// Returns false if it is already doomed to fail (e.g. when offline)
	virtual bool sendMessage (const Message & msg) = 0;

	/// @}

	///@name Feature management
	///@{

	/// Information about the feature set of another peer
	struct FeatureInfo {
		String clientName;
		String clientType;
		std::vector<String> features;
		SF_AUTOREFLECT_SERIAL;
	};

	typedef function <void (Error, const FeatureInfo & features)> FeatureCallback;
	/// Requests the feature set of someone
	virtual Error requestFeatures (const HostId & dst, const FeatureCallback & callback) { return error::NotSupported;}

	/// Add a feature to our own feature set
	virtual Error setFeatures (const std::vector<String> & features) { return error::NotSupported; }

	/// Sets XMPP Identity (name = Program + Version) and Type (bot, pc, console)
	/// See http://xmpp.org/registrar/disco-categories.html#client
	virtual Error setIdentity (const String & name, const String & type) { return error::NotSupported; }

	///@}

	/// @{
	/// @name Contact management

	/// Sends a subscribption for someone else's presence
	/// (If the other back accepts, the subscription shall be automatically added)
	/// Will usually automatically add to the contact list
	virtual Error subscribeContact (const sf::UserId & user) { return error::NotSupported; }

	/// Reply to an add contact question
	virtual Error subscriptionRequestReply (const sf::UserId & user, bool allow, bool alsoAdd) { return error::NotSupported; }

	/// Cancel presence subscription to another client
	virtual Error cancelSubscription (const sf::UserId & user) { return error::NotSupported; }

	/// Remove a contact from the server (thus automatically canceling any subscriptions)
	virtual Error removeContact (const sf::UserId & user) { return error::NotSupported; }

	/// @}

	/// @name Delegates 
	/// @{

	/// A delegate informing about a change of the ConnectionState
	typedef function <void (sf::IMClient::ConnectionState)> ConnectionStateChangedDelegate;

	/// A delegate informing about a received message
	typedef function <void (const sf::IMClient::Message& )> MessageReceivedDelegate;

	/// The server sent a stream error
	typedef function <void (const String & msg)> ServerStreamErrorDelegate;

	/// A delegate informing about someone who wants to add you to the contact list
	/// Reply with addedContactReply
	typedef function <void (const UserId & from)> SubscribeRequestDelegate;
	virtual SubscribeRequestDelegate & subscribeRequest () { static SubscribeRequestDelegate del; return del; }
	
	/// Delegate informed at connection state change
	virtual ConnectionStateChangedDelegate & connectionStateChanged () = 0;
	
	/// Delegate informed at contact roster changed
	virtual VoidDelegate & contactRosterChanged () = 0;

	/// Delegate informed if we got a message
	virtual MessageReceivedDelegate & messageReceived () = 0;

	/// Delegate information about a stream error sent from the server
	virtual ServerStreamErrorDelegate & streamErrorReceived () { static ServerStreamErrorDelegate del; return del; }

	// @}
	
};

SF_AUTOREFLECT_ENUM (IMClient::ConnectionState);
SF_AUTOREFLECT_ENUM (IMClient::PresenceState);
SF_AUTOREFLECT_ENUM (IMClient::MessageType);
SF_AUTOREFLECT_ENUM (IMClient::SubscriptionState);

// void serialize (Serialization & s, const IMClient::Contacts & contacts);

}
