#include "SharedListTracker.h"
#include <schnee/tools/Log.h>

namespace sf {

SharedListTracker::SharedListTracker (DataSharingClient * client) {
	SF_REGISTER_ME;
	mTracker  = new DataTracker (client);
}

SharedListTracker::~SharedListTracker () {
	SF_UNREGISTER_ME;
	delete mTracker;
}

Error SharedListTracker::trackShared (const sf::HostId & host) {
	sf::Uri uri (host, "shared");
	return mTracker->track (uri, dMemFun (this, &SharedListTracker::onDataUpdate), dMemFun (this, &SharedListTracker::onStateChange));
}

Error SharedListTracker::untrackShared (const sf::HostId & host) {
	sf::Uri uri (host, "shared");
	return mTracker->untrack (uri);
}

SharedListTracker::SharedListMap SharedListTracker::sharedLists () const {
	return mSharedLists;
}

void SharedListTracker::onDataUpdate  (const sf::Uri & uri, int revision, const sf::ByteArrayPtr & data) {
	SharedList list;
	bool suc;
	assert (uri.path() == "shared");
	sf::Log (LogInfo) << LOGID << "Got shared data " << * data << std::endl;
	suc = fromJSON (*data, list);
	if (suc){
		mSharedLists[uri.host()] = list;
	} else {
		Log (LogWarning) << LOGID << "Could not deserialize shared list, untracking" << std::endl;
		mTracker->untrack (uri);
	}
	if (suc  && mTrackingUpdate)     mTrackingUpdate (uri.host(), list);
	if (!suc && mLostTracking)       mLostTracking (uri.host(), error::BadDeserialization);
}

void SharedListTracker::onStateChange (const sf::Uri & uri, DataTracker::TrackState state) {
	bool hadError  = false;
	assert (uri.path() == "shared");
	if (state != DataTracker::TRACKING){
		hadError = true;
		mSharedLists.erase (uri.host());
	}
	if (hadError){
		sf::Log (LogProfile) << LOGID << "Lost tracking on user " << uri.host() << std::endl;
		if (mLostTracking) mLostTracking (uri.host(), error::Other);
	}
}

}
