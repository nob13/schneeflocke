#include <schnee/sftypes.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/tools/async/MemFun.h>

namespace sf {

/**
 * Helper class to easily wait for asynchronous signals.
 *
 */
class ResultCallbackHelper : public DelegateBase {
public:
	ResultCallbackHelper () {
		SF_REGISTER_ME;
		reset ();
	}

	~ResultCallbackHelper () {
		SF_UNREGISTER_ME;
	}

	/// Gets a callback for regular ErrorCallbacks
	ResultCallback  onResultFunc () {
		reset ();
		return (dMemFun (this, &ResultCallbackHelper::onResult));
	}

	/// Gets a callback for regular VoidCallbacks
	VoidCallback onReadyFunc () {
		LockGuard guard (mMutex);
		reset ();
		return (dMemFun (this, &ResultCallbackHelper::onReady));
	}

	bool ready () const {
		LockGuard guard (mMutex);
		return mReady;
	}

	Error result () const {
		LockGuard guard (mMutex);
		return mResult;
	}

	void reset () {
		LockGuard guard (mMutex);
		mReady  = false;
		mResult = NoError;
	}

	bool waitUntilReady (int timeOutMs) {
		LockGuard guard (mMutex);
		Time t (futureInMs (timeOutMs));
		while (!mReady){
			if (!mCondition.timed_wait(mMutex, t))
				return false;
		}
		return true;
	}

	/// @depreciated.
	bool waitReadyAndNoError (int timeOutMs) {
		bool suc = waitUntilReady(timeOutMs);
		if (!suc) return false;
		LockGuard guard (mMutex);
		return !mResult;
	}

	/// Like waitReadyAndNoError but easier and with specifc error code
	Error wait (int timeOutMs = 30000){
		bool suc = waitUntilReady (timeOutMs);
		if (!suc) return error::TimeOut;
		return mResult;
	}
private:
	/// Connect this function (for ResultCallback)
	void onResult (Error result) {
		mReady  = true;
		mResult = result;
	}

	/// Connect this function (for VoidCallback)
	void onReady () {
		mReady  = true;
		mResult = NoError;
	}

	bool _isReady () { return mReady; } /// memFun is not yet const function compatible

	mutable Mutex mMutex;
	Condition mCondition;
	bool  mReady;
	Error mResult;
};


}
