#include "DataSharingClientImpl.h"
#include <assert.h>
#include <schnee/tools/Log.h>

namespace sf {

DataSharingClient * DataSharingClient::create () {
	return new DataSharingClientImpl;
}

DataSharingClientImpl::DataSharingClientImpl (){
	SF_REGISTER_ME;
	mNextHostKey       = 1;
}

DataSharingClientImpl::~DataSharingClientImpl (){
	SF_UNREGISTER_ME;
}

Error DataSharingClientImpl::shutdown () {
	sf::LockGuard guard (mMutex);
	for (SubscriptionInfoMap::const_iterator i = mSubscriptions.begin(); i != mSubscriptions.end(); i++){
		const Uri & uri (i->first);
		Subscribe s;
		s.mark = Subscribe::SubscriptionCancel;
		s.path = uri.path();
		mCommunicationDelegate->send(uri.host(), Datagram::fromCmd(s));
	}
	mSubscriptions.clear();
	return sf::NoError;
}

Error DataSharingClientImpl::request (const HostId & src, const Request & request, const RequestReplyDelegate & callback, int timeOutMs, int64_t * idOut) {
	LockGuard guard (mMutex);

	assert (request.id == 0 && "Foreign id?!");

	OpId id = genFreeId_locked ();
	request.id = id;
	
	if (idOut) *idOut = id;
	Error err = mCommunicationDelegate->send (src, Datagram::fromCmd(request));
	if (!err) {
		RequestOp * op = new RequestOp (sf::regTimeOutMs (timeOutMs));
		op->setId(id);
		op->cb = callback;
		op->setKey(hostKey_locked (src));
		op->isTransmission = (request.mark == Request::Transmission);
		op->path = request.path;
		add_locked (op);
	}
	return err;
}

Error DataSharingClientImpl::cancelTransmission (const HostId & src, AsyncOpId id, const Path & path) {
	Request r;
	r.id   = id;
	r.path = path;
	r.mark = Request::TransmissionCancel;
	return mCommunicationDelegate->send (src, Datagram::fromCmd(r));
}

Error DataSharingClientImpl::subscribe (const HostId & src, const Subscribe & subscribe, const SubscribeReplyDelegate & callback, const NotificationDelegate & notDelegate, int timeOutMs, int64_t * idOut){
	LockGuard guard (mMutex);

	OpId id = genFreeId_locked();
	subscribe.id = id;
	
	if (idOut) *idOut = id;
	Error err = mCommunicationDelegate->send (src, Datagram::fromCmd(subscribe));
	if (!err) {
		SubscribeOp * op = new SubscribeOp (sf::regTimeOutMs(timeOutMs));
		op->setId(id);
		op->cb = callback;
		op->setKey(hostKey_locked (src));
		op->proposedNotificationDelegate = notDelegate;
		add_locked (op);
	}
	return err;
}

Error DataSharingClientImpl::cancelSubscription (const Uri & uri) {
	{
		LockGuard guard (mMutex);

		SubscriptionInfoMap::iterator i = mSubscriptions.find (uri);
		if (i == mSubscriptions.end()) return error::NotFound;
		{
			const Uri & uri (i->first);
			Subscribe s;
			s.path = uri.path();
			s.mark = Subscribe::SubscriptionCancel;
			// sending it to the source
			mCommunicationDelegate->send (uri.host(), Datagram::fromCmd(s));
		}
		mSubscriptions.erase (i);
	}
	return error::NoError;
}

Error DataSharingClientImpl::push (const HostId & user, const ds::Push & pushCmd, const sf::ByteArrayPtr & data, const PushReplyDelegate & callback, int timeOutMs, int64_t * idOut) {
	if (!pushCmd.range.isDefault()){
		// TODO Add range support
		sf::Log (LogError) << LOGID << "Currently no support for range in push" << std::endl;
		return error::NotSupported;
	}
	LockGuard guard (mMutex);
	
	sf::ByteArray content;
	OpId id;
	id = genFreeId_locked();
	pushCmd.id = id;
	if (idOut) *idOut = id;
	Error result;
	result = mCommunicationDelegate->send (user, Datagram::fromCmd(pushCmd, data));

	if (!result) {
		PushOp * op = new PushOp (sf::regTimeOutMs(timeOutMs));
		op->setId(id);
		op->cb = callback;
		op->setKey (hostKey_locked (user));
		add_locked (op);
	}
	return result;
}

void DataSharingClientImpl::onChannelChange (const HostId & host){
	int hostKey = -1;
	{	
		LockGuard guard (mMutex);
		
		int level = mCommunicationDelegate->channelLevel(host);
		if (level > 0) return; // just want to see if a host gets offline
		
		// 1. Stopping / Canceling subscriptions
		SubscriptionInfoMap::iterator i = mSubscriptions.begin();
		while (i != mSubscriptions.end()){
			SubscriptionInfo & info (i->second);
			const Uri & uri (i->first);
			if (uri.host() == host){
				if (info.notificationDelegate){
					Notify brokenNotification;
					brokenNotification.mark = Notify::SubscriptionCancel;
					brokenNotification.path = uri.path();
					xcall (abind (info.notificationDelegate, host, brokenNotification));
				}
				mSubscriptions.erase(i++);
				continue;
			}
			i++;
		}
		hostKey = hostKey_locked (host);
	}
	// Canceling operations
	// Async Operations have their own lock
	cancelOperations_locked (hostKey, error::TargetOffline);
}

void DataSharingClientImpl::onRpc (const HostId & sender, const Notify & n, const ByteArray & content) {
	NotificationDelegate delegate;
	{
		LockGuard guard (mMutex);
		SubscriptionInfoMap::iterator i = mSubscriptions.find (Uri (sender, n.path));
		if (i == mSubscriptions.end()) {
			Log (LogInfo) << LOGID << "Got notifies from data / from user, I am not subscribed to" << std::endl;
			// may happen, should send them an subscription cancel
			if (n.mark != Notify::SubscriptionCancel){
				Subscribe s;
				s.path = n.path;
				s.mark = Subscribe::SubscriptionCancel;
				mCommunicationDelegate->send(sender, Datagram::fromCmd(s));
			}
			return;
		}
		delegate = i->second.notificationDelegate;
	}
	if (delegate) {
		delegate (sender, n);
	}
}

void DataSharingClientImpl::onRpc (const HostId & sender, const SubscribeReply & reply, const ByteArray & content) {
	LockGuard guard (mMutex);
	SubscribeOp * sop = 0;
	getReady_locked (reply.id, SUBSCRIBE, &sop);
	if (!sop) {
		Log (LogWarning) << LOGID << "Got subscribe reply for non-existing subscibe, maybe timeouted" << std::endl;
		return; // do not handle it.
	}


	if (reply.err == NoError) {
		SubscriptionInfoMap::const_iterator i = mSubscriptions.find (Uri (sender, reply.path));
		if (i != mSubscriptions.end()){
			if (!(i->second.desc == reply.desc)){
				Log (LogWarning) << LOGID << "Data description mismatch" << std::endl;
			}
		}
		SubscriptionInfo& info = mSubscriptions[Uri (sender, reply.path)];
		info.desc                 = reply.desc;
		info.notificationDelegate = sop->proposedNotificationDelegate;
	}
	notifyAsync (sop->cb, sender, reply);
	delete sop;
}

void DataSharingClientImpl::onRpc (const HostId & sender, const RequestReply & reply, const ByteArray & content) {
	LockGuard guard (mMutex);
	RequestOp * rop = 0;
	getReady_locked (reply.id, REQUEST, &rop);
	if (!rop) {
		Log (LogWarning) << LOGID << "Got request reply, for non existing op" << std::endl;

		if (reply.mark == RequestReply::Transmission){
			// Stop a working transmission, maybe it was timeouted.
			Request r;
			r.path  = reply.path;
			r.mark = Request::TransmissionCancel;
			r.id   = reply.id;
			mCommunicationDelegate->send (sender, Datagram::fromCmd(r));
		}
		return;
	}
	if (reply.path.isDefault()) {
		const_cast<RequestReply&> (reply).path = rop->path; // sender is allowed to avoid uri.
	}
	bool followTransmission = rop->isTransmission && (
			reply.mark == RequestReply::Transmission ||
			reply.mark == RequestReply::TransmissionStart); // Do not follow on TransmissionFinish and TransmissionCancel

	ByteArrayPtr contentPtr (sf::createByteArrayPtr(content));
	notifyAsync (rop->cb, sender, reply, contentPtr);
	if (!followTransmission)
		delete rop;
	else {
		rop->setTimeOut (futureInMs(10000)); // TODO Make this changeable.
		add_locked (rop);
	}
}

void DataSharingClientImpl::onRpc (const HostId & sender, const PushReply & reply, const ByteArray & content) {
	LockGuard guard (mMutex);
	PushOp * pop = 0;
	getReady_locked (reply.id, PUSH, &pop);
	if (!pop) {
		Log (LogWarning) << LOGID << "Got push reply, for non existing op" << std::endl;
		return;
	}
	notifyAsync (pop->cb, sender, reply);
	delete pop;
}

}
