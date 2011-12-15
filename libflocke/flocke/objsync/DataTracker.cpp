#include "DataTracker.h"

namespace sf{

DataTracker::DataTracker (DataSharingClient * client) {
	SF_REGISTER_ME;
	mSharingClient = client;
	mTimeOutMs = 10000;
}

DataTracker::~DataTracker () {
	SF_UNREGISTER_ME;
}

Error DataTracker::track (const sf::Uri & uri, const DataUpdateCallback & dataUpdateCallback, const StateChangeCallback & stateChangeCallback){
	TrackInfoPtr info (new TrackInfo);
	Error subscribeResult = mSharingClient->subscribe (
			uri.host(),
			ds::Subscribe(uri.path()),
			abind (sf::dMemFun (this, &DataTracker::onSubscribeReply), info),
			abind (sf::dMemFun (this, &DataTracker::onNotify), info),
			mTimeOutMs
	);
	if (subscribeResult) return subscribeResult;
	info->awaiting    = AwaitSubscribe;
	info->state       = ESTABLISHING; // No Track change info
	info->dataUpdated  = dataUpdateCallback;
	info->stateChanged = stateChangeCallback;
	info->uri         = uri;
	mTracked[uri]     = info;
	return NoError;
}

Error DataTracker::untrack (const sf::Uri & uri) {
	TrackInfoMap::iterator i = mTracked.find (uri);
	if (i == mTracked.end()) return error::NotFound;

	// something awaiting?
	TrackInfoPtr info (i->second);
	info->state = TO_DELETE;
	mSharingClient->cancelSubscription(i->first);
	mTracked.erase (i);
	return sf::NoError;
}

void DataTracker::onSubscribeReply (const HostId & sender, const ds::SubscribeReply & reply, TrackInfoPtr info) {
	bool doNotifyState = false;
	do {
		info->awaiting &= ~AwaitSubscribe;
		if (info->state == TO_DELETE || info->state == ERROR){
			return;
		}
		if (reply.err){
			info->state = ERROR;
			doNotifyState = true;
			break;
		}

		// do first request now
		sf::Error err = mSharingClient->request (
				info->uri.host(),
				ds::Request (info->uri.path()),
				abind (sf::dMemFun (this, &DataTracker::onRequestReply), info), mTimeOutMs);
		if (err) {
			info->state = ERROR;
			doNotifyState = true;
			break;
		} else {
			info->awaiting |= AwaitRequest;
		}

	} while (false);
	if (doNotifyState) {
		info->stateChanged (info->uri, info->state);
	}
}

void DataTracker::onRequestReply   (const HostId & sender, const ds::RequestReply & reply, const ByteArrayPtr & data, TrackInfoPtr info){
	bool doNotifyState  = false;
	bool doNotifyUpdate = false;
	do {
		info->awaiting &= ~AwaitRequest;
		if (info->state == TO_DELETE || info->state == ERROR){
			return;
		}
		if (reply.err) {
			info->state = ERROR;
			doNotifyState = true;
			break;
		}
		if (info->state == ESTABLISHING){
			info->state = TRACKING;
			info->revision = reply.revision;
			doNotifyState = true;
		}
		doNotifyUpdate = true;
	} while (false);
	if (doNotifyState) {
		info->stateChanged(info->uri, info->state);
	}
	if (doNotifyUpdate){
		info->dataUpdated(info->uri, reply.revision, data);
	}
}

void DataTracker::onNotify         (const HostId & sender, const ds::Notify & notify, TrackInfoPtr info) {
	bool doNotifyState  = false;
	do {
		if (notify.mark == ds::Notify::SubscriptionCancel){
			info->state = LOST;
			doNotifyState = true;
			break;
		}
		// request an update..
		if (notify.revision > info->revision){ // may happen that notify is slower than requestreply
			Error err = mSharingClient->request (
					info->uri.host(),
					ds::Request (info->uri.path()),
					abind (sf::dMemFun (this, &DataTracker::onRequestReply), info),
					mTimeOutMs);
			if (err){
				info->state = ERROR;
				doNotifyState = true;
			} else {
				info->awaiting |=AwaitRequest;
			}
		}

	} while (false);
	if (doNotifyState) {
		info->stateChanged(info->uri, info->state);
	}
}

}
