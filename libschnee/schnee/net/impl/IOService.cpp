#include "IOService.h"
#include <boost/asio.hpp>

#include <boost/bind.hpp>

namespace sf {

void IOService::start () {
	mThread = new boost::thread (boost::bind(&IOService::run, this));
	LockGuard m (mMutex);
	while (!mRunning){
		mStateChange.wait(mMutex);
	}
}

void IOService::stop () {
	LockGuard m (mMutex);
	if (!mThread) return;
	if (mRunning) mService.stop();
	mThread->join();
	delete mThread;
	mRunning = false; // thread will join automatically
}

void IOService::starting (){
	{
		LockGuard m (mMutex);
		mRunning = true;
	}
	mStateChange.notify_all();
}


void IOService::run (){
	boost::asio::io_service::work work (mService); // so io service won't stop if there is no more work anymore 
	mService.post (boost::bind(&IOService::starting,this));
	mService.run();
}

boost::thread::id IOService::threadId (const boost::asio::io_service& s){
	if (instance().mRunning && (&s == &(instance().mService))){
		return instance().mThread->get_id();
	}
	return boost::thread::id();
}

bool IOService::isCurrentThreadService (const boost::asio::io_service & service){
	boost::thread::id ct = boost::this_thread::get_id();
	boost::thread::id st = threadId (service);
	return ct == st;
}

IOService::IOService () : mThread (0), mRunning (false) {
}
IOService::~IOService (){
}

}
