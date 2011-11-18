#include "GlobPromise.h"

namespace sf{

GlobPromise::GlobPromise (GlobberPtr globber, const String & dirName) {
	SF_REGISTER_ME;
	mError = NoError;
	mReady = false;
	LockGuard guard (mMutex);
	Error result = globber->glob (dirName, dMemFun (this, &GlobPromise::onGlobResult));
	if (result){
		mReady = true;
		mError = result;
	}
}

GlobPromise::~GlobPromise (){
	SF_UNREGISTER_ME;
}

sf::Error GlobPromise::read (const ds::Range & range, ByteArray & dst) {
	LockGuard guard (mMutex);
	if (!mReady) {
		assert (false);
		return error::InvalidArgument;
	}
	if (mError) {
		return mError;
	}
	if (!range.isValid()) return error::InvalidArgument;
	
	
	ds::Range r = range.clipTo (ds::Range(0, mSerializedListing.size()));
	bool eof = r.to == (int64_t) mSerializedListing.size();
	dst.assign (mSerializedListing.begin() + r.from, mSerializedListing.begin() + r.to);
	if (eof) return error::Eof;
	return NoError;
}

int64_t GlobPromise::size () const {
	LockGuard guard (mMutex);
	if (!mReady || mError) return -1;
	return mSerializedListing.size();
}

void GlobPromise::onGlobResult (Error result, const RecursiveDirectoryListingPtr & listing) {
	LockGuard guard (mMutex);
	mError   = result;
	mReady   = true;
	mListing = listing; 
	if (!mError){
		mSerializedListing = ByteArray (sf::toJSONEx(*mListing, INDENT | COMPRESS));
	}
}




}
