#pragma once
#include "DataSharingElements.h"
#include "CommunicationComponent.h"

namespace sf {

/** CommunicationComponent for getting data from a DataSharingServer.
 *  You can request, subscribe, push data and request sharing information
 *  or follow the distribution path via the traverse command.
 *
*/
struct DataSharingClient : public CommunicationComponent {
	/// Creates a DataSharing Implementation
	static DataSharingClient * create ();

	/// Cancels subscriptions
	virtual Error shutdown () = 0;

	///@name Requesting Data
	///@{

	typedef sf::function <void (const HostId & sender, const ds::RequestReply & reply, const sf::ByteArrayPtr & data)> RequestReplyDelegate;

	/// Requests some URI from someone. Will call you back (only if this function returns NoError)
	/// idOut will be mapped to the internal used AsyncOp id (if non null)
	virtual Error request (const HostId & src, const ds::Request & request, const RequestReplyDelegate & callback, int timeOutMs = -1, AsyncOpId * idOut = 0) = 0;
	
	/// Cancel a transmission (will not callback, may not be received by the source)
	virtual Error cancelTransmission (const HostId & src, AsyncOpId id, const Path & uri = Path()) = 0;

	typedef sf::function <void (const HostId & sender, const ds::SubscribeReply & reply)> SubscribeReplyDelegate;
	typedef sf::function <void (const HostId & sender, const ds::Notify &)> NotificationDelegate;

	/// Requests for a subscription of specific data. Will gives you notifications on changes and updates for streams, specific notification variant
	/// Note: Broken subscriptions will be sent by Notification::SubscriptionCancel mark
	/// idOut will be mapped to the internal used AsyncOp id (if non null)
	virtual Error subscribe (const HostId & src, const ds::Subscribe & subscribe, const SubscribeReplyDelegate & callback, const NotificationDelegate & notDelegate, int timeOutMs = -1, AsyncOpId * idOut = 0) = 0;

	/// Cancel a Subscription, does not have a callback as it comes back immediately
	virtual Error cancelSubscription (const Uri & uri) = 0;

	///@}

	///@name Pushing data to a Server
	///@{

	typedef sf::function <void (const HostId & sender, const ds::PushReply & reply)> PushReplyDelegate;

	/// Pushes some data to a server, calls you back.
	/// idOut will be mapped to the internal used AsyncOp id (if non null)
	virtual Error push (const HostId & host, const ds::Push & pushCmd, const sf::ByteArrayPtr & data, const PushReplyDelegate & callback, int timeOutMs = -1, AsyncOpId * idOut = 0) = 0;

	///@}
};

}
