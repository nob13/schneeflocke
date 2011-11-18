#pragma once
#include <schnee/sftypes.h>
#include <schnee/tools/async/Signal.h>

namespace sf {

/// Presence management encapsulates some information
/// gathered about other Peers. Its part of the InterplexBeacon.
class PresenceManagement {
public:
	virtual ~PresenceManagement () {}

	/// Information about an other (not ourself) host, which is online
	struct HostInfo {
		/// Id for connecting purposes
		HostId 			hostId;
		/// id of the corresponding user
		UserId          userId;
		/// (Machine) name
		String			name;
		/// Client Name (empty if unknown)
		String 			client;
		/// Feature list of this client (empty if unknown or no feature supported)
		std::vector<String> features;

		SF_AUTOREFLECT_SERIAL;

	};
	typedef std::map<HostId, HostInfo> HostInfoMap;

	struct UserInfo {
		UserInfo () : /*state (OS_OFFLINE), */waitForSubscription (false), error (false) {}
		/// Id of user
		sf::UserId userId;
		/// human readable name of user
		sf::String name;
		/// currently waiting for the other to accept subscription
		bool waitForSubscription;
		/// there is some error
		bool error;
		/// error description
		String errorText;

		SF_AUTOREFLECT_SERIAL;
	};
	typedef std::map<UserId, UserInfo> UserInfoMap;


	///@name Watching other Peers
	///@{

	/// Our own host id
	virtual HostId hostId () const = 0;

	/// Our own online state
	virtual OnlineState onlineState () const = 0;

	/// Online state of another user
	virtual OnlineState onlineState (const sf::HostId & user) const = 0;

	/// Information about all users and hosts
	virtual UserInfoMap users() const = 0;

	/// Information about all hosts
	virtual HostInfoMap hosts() const = 0;

	/// Information about all hosts of a specific user
	virtual HostInfoMap hosts(const UserId & user) const = 0;

	/// Returns hostinfo for a given Host
	virtual HostInfo hostInfo (const HostId & host) const = 0;

	/// Asks for a feature/client update for a given host
	virtual Error updateFeatures (const HostId & host, const ResultCallback & callback = ResultCallback()) { return error::NotSupported; }

	/// Set own feature info (must be done before connecting!)
	virtual Error setOwnFeature (const String & client, const std::vector<String> & features) { return error::NotSupported; }

	///@}

	///@name User management
	///@{

	/// Subscribe to another contact
	virtual Error subscribeContact (const sf::UserId & user) { return error::NotSupported;}

	/// Reply on a foreign subscription request
	virtual Error subscriptionRequestReply (const sf::UserId & user, bool allow, bool alsoAdd) { return error::NotSupported; }

	//// Remove a contact from the list
	virtual Error removeContact (const sf::UserId & user) { return error::NotSupported; }


	///@}

	///@name Signals and Delegates
	///@{

	typedef Signal<void (OnlineState os)> OnlineStateChangedSignal;
	typedef Signal<void ()> VoidSignal;
	typedef Signal<void (const String& text)> ServerStreamErrorSignal;
	typedef function<void (const UserId & from)> SubscribeRequestDelegate;

	/// A delegate that someone wants to subscribe you (answer with subscibeRequestReply)
	virtual SubscribeRequestDelegate & subscribeRequest () { static SubscribeRequestDelegate del; return del; }

	/// The peer list changed
	virtual VoidSignal & peersChanged () = 0;

	/// The server sent some stream error, you should display that
	virtual ServerStreamErrorSignal & serverStreamErrorReceived () = 0;

	/// Our own online state changed
	virtual OnlineStateChangedSignal & onlineStateChanged () = 0;
	///@}

};

}
