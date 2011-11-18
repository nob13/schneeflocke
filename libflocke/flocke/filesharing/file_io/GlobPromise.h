#pragma once

#include <schnee/p2p/DataPromise.h>
#include <schnee/tools/async/DelegateBase.h>
#include "Globber.h"


namespace sf {

/// Asynchronous promise which returns Globbing Info (recursive data structure)
class GlobPromise : public DataPromise, public DelegateBase {
public:
	GlobPromise (GlobberPtr globber, const String & dirName);
	~GlobPromise ();

	// Implementation of DataSharingPromise
	virtual bool ready () const { LockGuard guard (mMutex); return mReady; }
	virtual sf::Error read (const ds::Range & range, ByteArray & dst);
	virtual int64_t size () const;

private:
	/// Result handler for the globbing process
	void onGlobResult (Error result, const RecursiveDirectoryListingPtr & listing);
	
	bool  mReady;
	Error mError;
	RecursiveDirectoryListingPtr mListing;
	ByteArray mSerializedListing;
	GlobberPtr  mGlobber;
	mutable Mutex mMutex;
};
typedef shared_ptr<GlobPromise> GlobPromisePtr;


}
