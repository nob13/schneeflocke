#include "UDTMainLoop.h"

#include <schnee/tools/Log.h>
#include <boost/thread.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

namespace sf {

/*
 * Implementation Notice
 * We are using UDT::selectEx instead of UDT::epoll because there seems to
 * be no possibility to tell UDT to just watch out for readevents and not write
 * events on some sockets (maybe this is possible with two epoll instances).
 *
 * With always getting writeable-Events we burned a lot of CPU cycles.
 * If the program is gonna having more connections (in the moment around 1 - 50)
 * we will have to switch back, but for the moment its fast enough.
 */

void UDTMainLoop::add    (UDTEventReceiver * s) {
	LockGuard guard (mMutex);
	UDTSOCKET impl = s->impl();
	assert (mSockets.count (impl) == 0);
	mSockets[impl] = s;
	
	if (!mRunning)
		start_locked ();
}

void UDTMainLoop::addWrite (UDTEventReceiver * s) {
	LockGuard guard (mMutex);
	UDTSOCKET impl = s->impl();
	assert (mSockets.count(impl) == 1);

	mWriteableWaiting.insert(impl);
}


void UDTMainLoop::remove (UDTEventReceiver * s) {
	LockGuard guard (mMutex);
	UDTSOCKET impl = s->impl();
	assert (mSockets.count (impl) > 0);
	mWriteableWaiting.erase(impl);
	mSockets.erase (impl);
	if (mSockets.empty()){
		stop_locked ();
	}
}

bool UDTMainLoop::isRunning () {
	LockGuard guard (mMutex);
	return mRunning;
}

void UDTMainLoop::start_locked () {
	mToQuit  = false;
	mRunning = true;
	boost::thread t (memFun (this, &UDTMainLoop::threadEntry));
}

void UDTMainLoop::stop_locked () {
	if (!mRunning) return;
	mToQuit = true;
	while (mRunning) {
		mCondition.wait(mMutex);
	}
}

void UDTMainLoop::threadEntry () {
	mMutex.lock ();
	while (!mToQuit) {

		typedef std::vector<UDTSOCKET> SocketVec;
		SocketVec readableCheck; // list could be created somewhere else
		for (SocketMap::const_iterator i = mSockets.begin(); i != mSockets.end(); i++) {
			readableCheck.push_back(i->first);
		}
		SocketVec writeableCheck;
		for (SocketSet::const_iterator i = mWriteableWaiting.begin(); i != mWriteableWaiting.end(); i++) {
			writeableCheck.push_back(*i);
		}
		SocketVec readable;
		SocketVec writeable;
		mMutex.unlock (); // according to docu this shall be thread safe

		// watch out for writeable.
		bool fastTrack = false;
		if (writeableCheck.size() > 0){
			UDT::selectEx (writeableCheck, 0, &writeable, 0, 0);
			if (writeable.size() > 0) fastTrack = true;
		}
		int timeOutMs = fastTrack ? 0 : 10; // if something writeable is true do not wait for readables
		UDT::selectEx (readableCheck, &readable, 0, 0, timeOutMs);

		mMutex.lock();
		
		/*
		 * TODO Clean up this DelegateRegisterLock stuff
		 * It is necessary to not have the own mutex locked while calling out
		 * as the UDTSockets may delete theirself or register a write event
		 * during callback.
		 */

		// Sending signals
		for (SocketVec::const_iterator i = readable.begin(); i != readable.end(); i++) {
			SocketMap::iterator j = mSockets.find(*i);
			if (j != mSockets.end()){
				UDTEventReceiver * receiver = j->second;
				mMutex.unlock();
				{
					sf::DelegateRegisterLock lock (receiver->delegateKey());
					if (lock.suc()) receiver->onReadable();
				}
				mMutex.lock();
			}
		}
		for (SocketVec::const_iterator i = writeable.begin(); i != writeable.end(); i++) {
			SocketMap::iterator j = mSockets.find(*i);
			bool continueWriting = false;
			if (j != mSockets.end()) {
				UDTEventReceiver * receiver = j->second;
				mMutex.unlock();
				{
					sf::DelegateRegisterLock lock (receiver->delegateKey());
					if (lock.suc()) continueWriting = receiver->onWriteable();
				}
				mMutex.lock();
			}
			if (!continueWriting) {
				mWriteableWaiting.erase(*i);
			}
		}
	}
	mRunning = false;
	mMutex.unlock ();
	mCondition.notify_all();
}

UDTMainLoop::UDTMainLoop () {
	mRunning = false;
	if (UDT::startup()){
		fprintf (stderr, "Failed to startup UDT: %s\n", UDT::getlasterror().getErrorMessage());
		abort ();
	}
}

UDTMainLoop::~UDTMainLoop () {
}

}
