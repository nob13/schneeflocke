#pragma once
#include <assert.h>
#include <schnee/p2p/DataPromise.h>

// Data which will be delivered after you call release ()
// (Test structure)
class OutstandingDataPromise : public sf::DataPromise{
public:
	OutstandingDataPromise (sf::ByteArrayPtr data) {
		assert (data);
		mData  = data;
		mReady = false;
	}
	
	void release () { 
		mReady = true; 
	}
	
	// Implementation of DataSharingPromise
	virtual bool ready () const { 
		return mReady; 
	}
	
	virtual sf::Error read (const sf::ds::Range & range, sf::ByteArray & dst) { 
		assert (range.isValid());
		if (!mReady) return sf::error::NotInitialized;
		if (range.from >= (int64_t) mData->size()){
			return sf::error::Eof;
		}
		if (range.to > (int64_t) mData->size()){
			dst.assign (mData->begin() + range.from, mData->end());
			return sf::error::Eof;
		}
		dst.assign (mData->begin() + range.from, mData->begin() + range.to);
		return sf::NoError;		
	}
	
	virtual int64_t size () const { 
		if (!mReady) return -1;
		return mData->size();
	}
	
private:
	bool mReady;
	sf::ByteArrayPtr mData;
};

typedef sf::shared_ptr<OutstandingDataPromise> OutstandingDataPromisePtr;
