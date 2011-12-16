#include "IOBase.h"

#include <schnee/net/impl/IOService.h>
#include <schnee/tools/async/MemFun.h>
#include <schnee/schnee.h>

namespace sf {

IOBase::IOBase (boost::asio::io_service & service) :
		mService (service),
		mError (NoError),
		mToDelete(false),
		mPendingOperations (0),
		mDeletionTries(0) {
}


void IOBase::requestDelete () {
	onDeleteItSelf();
	mService.post (memFun (this, &IOBase::handleRealDelete));
}

void IOBase::notifyCallback (ResultCallback * cb, Error result) {
	assert (IOService::isCurrentThread());
	if (*cb) {
		ResultCallback copy (*cb);
		cb->clear(); // must done before calling the callback, as it could come back
		mPendingOperations++;
		copy (result);
		mPendingOperations--;
	}
}

void IOBase::onDeleteItSelf (){
	mToDelete = true;
}

IOBase::~IOBase (){
	assert (mPendingOperations == 0);
}

void IOBase::handleRealDelete () {
	SF_SCHNEE_LOCK;
	assert (sf::IOService::isCurrentThreadService (mService));
	if (mPendingOperations > 0) {
		mDeletionTries++;
		// wait longer
		mService.post  (memFun (this, &IOBase::handleRealDelete));
		return;
	}
	delete this;
}

}
