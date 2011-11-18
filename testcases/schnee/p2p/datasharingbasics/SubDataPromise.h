#pragma once
#include <schnee/p2p/DataSharingServer.h>
#include <schnee/tools/Log.h>
#include <assert.h>

// Data which will be delivered with subPath "subData"
// (Test structure)
class SubDataPromise : public sf::DataSharingServer::SharingPromise{
public:
	SubDataPromise (sf::ByteArrayPtr data) {
		assert (data);
		mData  = data;
	}
	
	virtual sf::DataPromisePtr data (const sf::Path & subPath, const sf::String & user) const { 
		if (subPath == "subData")
			return sf::createDataPromise (mData);
		else
			return sf::DataPromisePtr();
	}

	
private:
	mutable sf::Mutex mMutex;
	bool mReady;
	sf::ByteArrayPtr mData;
};

typedef sf::shared_ptr<SubDataPromise> SubDataPromisePtr;
