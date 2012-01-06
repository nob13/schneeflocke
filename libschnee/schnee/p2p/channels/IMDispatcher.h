#pragma once

#include <schnee/im/IMClient.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/async/AsyncOpBase.h>

#include "IMChannel.h"
#include "AuthProtocol.h"
#include "ChannelProvider.h"
#include "../impl/PresenceProvider.h"

namespace sf {

/**
 * IMDispatcher stores an IMClient and builds and manages IMChannels on top of it.
 *
 * Note:
 * - All created channels are owned by the IMDispatcher are not usable after disconnecting
 *
 */
class IMDispatcher : public ChannelProvider, public PresenceProvider, public AsyncOpBase {
public:
	typedef sf::shared_ptr<IMChannel> IMChannelPtr;
	IMDispatcher();
	virtual ~IMDispatcher();

	// Implementation of ChannelProvider
	virtual bool providesInitialChannels () { return true; }
	virtual sf::Error createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs = -1);

	/// setHostId will be ignored (as IMDispatcher is also the PresenceProvider)
	virtual void setHostId (const sf::HostId & id);
	virtual ChannelCreationDelegate & channelCreated () { return mChannelCreated; }

	// Implementation of PresenceProvider
	virtual Error setConnectionString (const String & connectionString, const String & password);
	virtual sf::Error connect(const ResultDelegate & callback = ResultDelegate());
	virtual void disconnect();
	virtual HostId hostId () const;
	virtual OnlineState onlineState () const;
	virtual OnlineState onlineState (const sf::HostId & user) const;
	virtual UserInfoMap users () const { return mUsers; }
	virtual HostInfoMap hosts () const { return mHosts; }
	virtual HostInfoMap hosts(const UserId & user) const;
	virtual HostInfo hostInfo (const HostId & host) const;
	virtual Error updateFeatures (const HostId & host, const ResultCallback & callback);
	virtual Error setOwnFeature (const String & client, const std::vector<String> & features);

	virtual Error subscribeContact (const sf::UserId & user);
	virtual Error subscriptionRequestReply (const sf::UserId & user, bool allow, bool alsoAdd);
	virtual Error removeContact (const sf::UserId & user);

	virtual SubscribeRequestDelegate & subscribeRequest () { return mSubscribeRequest; }
	virtual VoidSignal & peersChanged () { return mPeersChanged; }
	virtual ServerStreamErrorSignal & serverStreamErrorReceived () { return mServerStreamErrorReceived; }
	virtual OnlineStateChangedSignal & onlineStateChanged () { return mOnlineStateChanged; }
	


private:
	friend class IMChannel;

	enum IMChannelOperation { CREATE_CHANNEL = 1, RESPOND_CHANNEL = 2 };

	// Asynchronous operation of creating an IM Channel
	struct CreateChannelOp : public AsyncOp {
		enum State { None, AwaitAuth };

		CreateChannelOp (const sf::Time & timeOut) : AsyncOp (CREATE_CHANNEL, timeOut) {
			mState = None;
		}

		HostId 			target;
		ResultCallback 	callback;
		AuthProtocol 	auth;
		IMChannelPtr 	channel;
		VoidDelegate    cancelOp;///< Provided by IMDispatcher, to remove channel from list in case of an error

		virtual void onCancel (sf::Error reason) {
			cancelOp ();
			if (callback) callback (reason);
		}
	};

	// Asynchronous operation of responding to a generated IMChannel
	struct RespondChannelOp : public AsyncOp {
		enum State { None, AwaitAuth };

		RespondChannelOp (const sf::Time & timeOut) : AsyncOp (RESPOND_CHANNEL, timeOut) {
			mState = None;
		}

		HostId 			source;
		AuthProtocol 	auth;
		IMChannelPtr 	channel;
		State           state;
		VoidDelegate    cancelOp;///< Provided by IMDispatcher, to remove channel from list in case of an error

		virtual void onCancel (sf::Error reason) {
			cancelOp ();
		}
	};


	///@name API for IMChannel
	///@{

	/// Sends a message (Used by IMChannel)
	/// @return true if message was given successfully to the IMClient
	bool send(const sf::IMClient::Message & m);
	
	///@}

	///@name Async Operation workflow
	///@{

	/// The operation gets canceled (e.g. through an timeout): removing channel from list
	void onOpCanceled (const HostId& target, const IMChannelPtr & channel);

	/// Callback for Auth protocol during channel creation
	void onAuthFinishedCreatingChannel (Error err, AsyncOpId id);

	/// Callback for Auth protocol during channel responding
	void onAuthFinishedRespondingChannel (Error err, AsyncOpId id);
	///@}

	///@name Handlers
	///@{
	/// Handler for subscribe requests
	void onSubscribeRequest (const sf::UserId & from);
	/// Handler for changing connection states
	void onConnectionStateChanged(sf::IMClient::ConnectionState state);
	/// Handler for changed contact roster
	void onContactRosterChanged();
	/// Message received
	void onMessageReceived(const sf::IMClient::Message & m);
	/// Stream error received
	void onServerStreamErrorRecevied (const String & text);
	/// Received features for a specific client
	void onFeatureRequestReply (Error result, const IMClient::FeatureInfo & features, const HostId & host, const ResultCallback & originalCallback);

	///@}

	// Delegates
	ChannelCreationDelegate mChannelCreated;
	SubscribeRequestDelegate mSubscribeRequest;
	VoidSignal mPeersChanged;
	ServerStreamErrorSignal mServerStreamErrorReceived;
	OnlineStateChangedSignal mOnlineStateChanged;

	sf::IMClient * mClient; ///< The IM Client we are performing on
	sf::IMClient::Contacts mRoster; ///< Current online / connected Clients in the roster
	HostInfoMap mHosts;		///< Information about all hosts
	UserInfoMap mUsers;		///< Information about all peers
	HostId mHostId;			///< Own host id
	int    mTimeOutMs;		///< Timeout for responding of channels (in ms)

	String mClientName;			///< Own client name
	std::vector<String> mFeatures; ///< Own feature list

	typedef std::map<sf::String, IMChannelPtr> ChannelMap;
	ChannelMap mChannels;

	/// Forbidden copy constructor
	IMDispatcher(const IMDispatcher &) {
	}
	/// Forbidden copy operator
	IMDispatcher & operator=(const IMDispatcher &) {
		return *this;
	}
};

}
