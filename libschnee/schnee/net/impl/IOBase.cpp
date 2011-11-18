#include "IOBase.h"

#include <schnee/net/impl/IOService.h>
#include <schnee/tools/async/MemFun.h>
namespace sf {

IOBase::IOBase (boost::asio::io_service & service) :
		mService (service),
		mError (NoError),
		mToDelete(false),
		mPendingOperations (0),
		mDeletionTries(0) {
}


void IOBase::requestDelete () {
	if (sf::IOService::isCurrentThreadService(mService)){
		onDeleteItSelf ();
	} else {
		mService.post (memFun (this, &IOBase::onDeleteItSelf));
		LockGuard lock (mStateMutex);
		while (!mToDelete) {
			mStateChange.wait(mStateMutex);
		}
	}
	mService.post (memFun (this, &IOBase::realDelete));
}

void IOBase::notifyCallback_locked (ResultCallback * cb, Error result) {
	assert (IOService::isCurrentThread());
	if (*cb) {
		ResultCallback copy (*cb);
		cb->clear(); // must done before calling the callback, as it could come back
		mPendingOperations++;
		mStateMutex.unlock();
		copy (result);
		mStateMutex.lock();
		mPendingOperations--;
	}
}

void IOBase::notifyDelegate_locked (const VoidCallback & del) {
	assert (IOService::isCurrentThread());
	if (del) {
		mPendingOperations++;
		mStateMutex.unlock();
		del();
		mStateMutex.lock();
		mPendingOperations--;
	}
}


void IOBase::onDeleteItSelf (){
	mStateMutex.lock ();
	mToDelete = true;
	mStateMutex.unlock ();
	mStateChange.notify_all();
}

IOBase::~IOBase (){
	assert (mPendingOperations == 0);
}

void IOBase::realDelete () {
	assert (sf::IOService::isCurrentThreadService (mService));
	{
		sf::LockGuard guard (mStateMutex);
		if (mPendingOperations > 0) {
			mDeletionTries++;
			// wait longer
			mService.post  (memFun (this, &IOBase::realDelete));
			return;
		}
	}
	delete this;
}

}
