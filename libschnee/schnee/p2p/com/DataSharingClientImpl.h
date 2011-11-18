#pragma once
#include "../DataSharingClient.h"
#include <schnee/tools/async/AsyncOpBase.h>

namespace sf {

using namespace ds;


/**
 * DataSharingClient implementation
 */
class DataSharingClientImpl : public DataSharingClient, public AsyncOpBase {
public:
	DataSharingClientImpl ();
	virtual ~DataSharingClientImpl ();

	SF_AUTOREFLECT_RPC;

	// Implementation of DataSharing
	virtual Error shutdown ();

	virtual Error request (const HostId & src, const Request & request, const RequestReplyDelegate & callback, int timeOutMs = -1, int64_t * idOut = 0);
	virtual Error cancelTransmission (const HostId & src, AsyncOpId id, const Path & path);
	

	virtual Error subscribe (const HostId & src, const Subscribe & subscribe, const SubscribeReplyDelegate & callback, const NotificationDelegate & notDelegate, int timeOutMs = -1, int64_t * idOut = 0);
	virtual Error cancelSubscription (const Uri & uri);

	virtual Error push (const HostId & user, const ds::Push & pushCmd, const sf::ByteArrayPtr & data, const PushReplyDelegate & callback, int timeOutMs = -1, int64_t * idOut = 0);

	virtual void onChannelChange (const HostId & host);

private:

	enum AsyncCmdType { REQUEST = 1, SUBSCRIBE = 2, PUSH = 3 };

	struct RequestOp : public AsyncOp {
		RequestOp(const sf::Time& _timeOut) : AsyncOp (REQUEST, _timeOut) {}
		RequestReplyDelegate cb;
		bool isTransmission;
		Path path;
		virtual void onCancel (sf::Error reason)
		{ RequestReply reply; reply.path = path; reply.err = reason; cb ("", reply, ByteArrayPtr()); }
	};
	struct SubscribeOp : public AsyncOp {
		SubscribeOp (const sf::Time& _timeOut) : AsyncOp (SUBSCRIBE, _timeOut) {}
		SubscribeReplyDelegate cb;
		NotificationDelegate proposedNotificationDelegate;
		virtual void onCancel (sf::Error reason)
		{ SubscribeReply reply; reply.err = reason; cb ("", reply);}
	};
	struct PushOp : public AsyncOp {
		PushOp (const sf::Time & _timeOut) : AsyncOp (PUSH, _timeOut) {}
		PushReplyDelegate cb;
		virtual void onCancel (sf::Error reason)
		{ PushReply reply; reply.err = reason; cb ("", reply); }
	};

	/// React on notify datagrams
	void onRpc (const HostId & sender, const Notify & notify, const ByteArray & content);

	/// React on subscribeReply datagrams
	void onRpc (const HostId & sender, const SubscribeReply & subscribeReply, const ByteArray & content);

	/// React on request reply datagrams
	void onRpc (const HostId & sender, const RequestReply & requestReply, const ByteArray & content);

	/// React on push reply datagrams
	void onRpc (const HostId & sender, const PushReply & pushReply, const ByteArray & content);

	// For Mapping HostId into keys of AsyncOpBase (TODO: Move this to a general function, only use keys, Bug #52)
	typedef std::map<sf::HostId, int> HostKeyMap;
	HostKeyMap mHostKeys;
	int mNextHostKey;
	int hostKey_locked (const HostId & id) {
		HostKeyMap::const_iterator i = mHostKeys.find(id);
		if (i == mHostKeys.end()) { mHostKeys[id] = mNextHostKey; return mNextHostKey++; }
		else return i->second;
	}

	/// Information about data which is subscribed
	struct SubscriptionInfo {
		DataDescription 		desc;					///< Description
		NotificationDelegate 	notificationDelegate;	///< Where to send notifications of this data
	};
	typedef std::map<Uri, SubscriptionInfo> SubscriptionInfoMap;
	SubscriptionInfoMap mSubscriptions;					///< Contains all information about subscriptions
};

}
