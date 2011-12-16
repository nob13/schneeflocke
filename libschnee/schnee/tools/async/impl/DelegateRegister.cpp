#include "DelegateRegister.h"
#include "../DelegateBase.h"
#include <schnee/net/impl/IOService.h>
#include <stdio.h>

namespace sf {

void DelegateRegister::registerMe (DelegateBase * me, const char * desc) {
	mMutex.lock ();
	KeyType key = mNextKey++;
	Info & info = mInfos[key];
	info.desc  = desc;
	info.base  = me;
	mInfos[key] = info;
	mMutex.unlock ();
	me->mDelegateKey = key;
}

void DelegateRegister::unregisterMe (DelegateBase * base) {
	KeyType key = base->mDelegateKey;
	mMutex.lock ();
	ThreadId tid = currentThreadId ();
	InfoMap::iterator i = mInfos.find (key);
	if (i == mInfos.end()) {
		assert (false && "Not registered");
	} else {
		Info & info (i->second);
		while (info.locked) {
			mCondition.wait (mMutex);
		}
		assert (i->second.base == base);
		mInfos.erase (i);
	}
	mMutex.unlock();
}

bool DelegateRegister::isRegistered (KeyType key) {
	mMutex.lock ();
	bool result;
	InfoMap::iterator i = mInfos.find (key);
	result = (i != mInfos.end());
	mMutex.unlock ();
	return result;
}

bool DelegateRegister::lock (KeyType key) {
	mMutex.lock ();

	bool found;
	ThreadId tid = currentThreadId ();
	while (!tryGetLock_locked (key, tid, found)){
		if (!found) {
			mMutex.unlock ();
			return false;
		}
		mCondition.wait (mMutex);
	}

	mMutex.unlock();
	return true;
	// mCondition.notify_all (); // must not notify upon locking
}

void DelegateRegister::unlock (KeyType key) {
	{
		sf::LockGuard guard (mMutex);
		InfoMap::iterator i = mInfos.find (key);
		if (i == mInfos.end()) {
			assert (false && "Unlocking of an not existing object");
			return;
		}
		Info & info (i->second);
#ifndef NDEBUG
		ThreadId tid = currentThreadId ();
		if (info.locked == 0){
			assert (false && "Object was not locked");
			return;
		}
		if (info.locker != tid) {
			assert (false && "Object was locked by wrong thread");
			return;
		}
#endif
		info.locked--;
		info.calls++;
	}
	mCondition.notify_all();
}

void DelegateRegister::pushCrossCall () {
	mMutex.lock ();
	mCrossCalls++;
	mMutex.unlock ();
	// mCondition.notify_all(); // must not notify upon locking
}

void DelegateRegister::popCrossCall () {
	mMutex.lock ();
	mCrossCalls--;
	mMutex.unlock();
	mCondition.notify_all();
}

void DelegateRegister::finish() {
	LockGuard guard (mMutex);
	while (mCrossCalls > 0) {
		mCondition.wait(mMutex);
	}
	mFinishing = true;

	if (!mInfos.empty()){
		printf ("Not all objects are unregistered yet!");
		printf ("The following still exist:\n");
		for (InfoMap::const_iterator i = mInfos.begin(); i != mInfos.end(); i++) {
			printf ("  %ld: %s\n", i->first, i->second.desc ? i->second.desc : "(without name)");
		}
		assert (false && "Not all objects are unregistered yet");
		exit (1);
	}
}

bool DelegateRegister::tryGetLock_locked (KeyType key, const ThreadId & tid, bool & found) {
	InfoMap::iterator i = mInfos.find (key);
	if (i == mInfos.end()) {
		found = false;
		return false;
	}
	found = true;
	Info & info (i->second);
	if (info.locked == 0){
		info.locker = tid;
		info.locked = 1;
		return true;
	}
	// locked is > 0
	assert (info.locked > 0);
	if (info.locker == tid) {
		info.locked++;
		return true;
	}
	return false;
}

DelegateRegister::DelegateRegister () {
	mNextKey = 1;
	mCrossCalls = 0;
	mFinishing = false;
}

DelegateRegister::~DelegateRegister () {
	assert (mFinishing && "Call finish before destroying");
}


}
