#include "WorkThread.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>

namespace sf {

WorkThread::WorkThread () {
	mState = NotStarted;
	mThread    = 0;
	mIoService = 0; 
}

WorkThread::~WorkThread () {
	stop ();
}

void WorkThread::start (const String & name) {
	LockGuard guard (mMutex);
	if (mState == Finished) mState = NotStarted; // restart
	assert (mState == NotStarted);
	if (mState == Started) return;
	mName = name;
	
	// initializing
	mIoService = new boost::asio::io_service();
	mThread = new boost::thread (bind (&WorkThread::onThreadStart, this));
	
	// wait until started
	mIoService->post (bind (&WorkThread::startup, this));
	while (mState != Started){
		mCondition.wait (mMutex);
	}
}

void WorkThread::stop () {
	{
		LockGuard guard (mMutex);
		if (mState != Started) {
			assert (!mIoService);
			return;
		}
	}
	
	mIoService->stop ();
	mThread->join();
	delete mIoService;
	delete mThread;
	mIoService = 0;
	mThread =   0;
	mState = Finished;
}

Error WorkThread::add (const VoidDelegate & f) {
	LockGuard guard (mMutex);
	if (mState != Started){
		return error::NotInitialized;
	}
	mIoService->post(f);
	return NoError;
}

void WorkThread::onThreadStart () {
	boost::asio::io_service::work work (*mIoService); // so it does not stop anymore
	mIoService->run();
}

void WorkThread::startup () {
	{
		LockGuard guard (mMutex);
		mState = Started;
	}
	mCondition.notify_all ();
}


}
