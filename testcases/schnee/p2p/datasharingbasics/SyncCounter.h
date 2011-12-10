#pragma once
#include <schnee/schnee.h>
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
		mAwaited = mCurrent + 1;
	}
	void wait () {
		while(mCurrent < mAwaited) {
			mCondition.wait(sf::schnee::mutex());
		}
	}
	void finish () {
		mCurrent++;
		if (mCurrent > mAwaited) {
			fprintf (stderr, "SyncCounter::finish : Not awaited Counter!\n");
		}
		mCondition.notify_all();
	}
private:
	int       mCurrent;
	int       mAwaited;
	sf::Condition mCondition;
};
