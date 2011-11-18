#pragma once
#include <schnee/sftypes.h>
#include <schnee/test/test.h>

/// A small help tool lets you wait for 'finish' signals
/// Used for synchronizing calls
class SyncCounter {
public:
	SyncCounter () {
		mCurrent = 0;
		mAwaited = 0;
	}
	
	void mark () {
		sf::LockGuard guard (mMutex);
		mAwaited = mCurrent + 1;
	}
	void wait () {
		sf::LockGuard guard (mMutex);
		while(mCurrent < mAwaited) {
			mCondition.wait(mMutex);
		}
	}
	void finish () {
		sf::LockGuard guard (mMutex);
		{
			mCurrent++;
			if (mCurrent > mAwaited) {
				fprintf (stderr, "SyncCounter::finish : Not awaited Counter!\n");
			}
			mCondition.notify_all();
		}
	}
private:
	int       mCurrent;
	int       mAwaited;
	sf::Mutex     mMutex;
	sf::Condition mCondition;
};
