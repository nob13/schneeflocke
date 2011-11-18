#pragma once
#include <flocke/objsync/DataTracker.h>
#include <schnee/p2p/DataSharingClient.h>
#include <schnee/tools/async/DelegateBase.h>
#include "SharedList.h"


namespace sf {

/**
 * A client which tracks the shared lists of other peers automatically.
 * (Opposite of SharedListServer)
 */
class SharedListTracker : public DelegateBase {
public:
	SharedListTracker (DataSharingClient * client);
	virtual ~SharedListTracker ();
	
	/// (unused) init function
	Error init () { return NoError; }
	
	// Track shared files of a user
	Error trackShared (const sf::HostId & user);

	// Stop tracking shared files of a user
	Error untrackShared (const sf::HostId & user);

	typedef std::map<sf::HostId, sf::SharedList> SharedListMap;
	
	/// Who shared what (possible slow interface)
	SharedListMap sharedLists () const;
	
	///@name Delegates
	///@{
	typedef function <void (const sf::HostId & user, Error cause) > LostTrackingDelegate;
	typedef function <void (const sf::HostId & user, const SharedList & list)> TrackingUpdateDelegate;

	/// Lost tracking to someone
	LostTrackingDelegate   & lostTracking () { return mLostTracking; }
	
	/// Got a new shared list of someone
	TrackingUpdateDelegate & trackingUpdate () { return mTrackingUpdate; }
	
	///@}
	
	
private:
	
	// Callbacks for DataTracker
	virtual void onDataUpdate  (const sf::Uri & uri, int revision, const sf::ByteArrayPtr & data);
	virtual void onStateChange (const sf::Uri & uri, DataTracker::TrackState state);
	
	LostTrackingDelegate   mLostTracking;
	TrackingUpdateDelegate mTrackingUpdate;
	
	DataTracker * mTracker;
	mutable Mutex mMutex;

	SharedListMap mSharedLists;
};

}
