#include <schnee/schnee.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/tools/async/MemFun.h>

namespace sf {

/**
 * Helper class to easily wait for asynchronous signals from other threads.
 * Note: In order to use: libschnee must be locked.
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
		reset ();
		return (dMemFun (this, &ResultCallbackHelper::onReady));
	}

	bool ready () const {
		return mReady;
	}

	Error result () const {
		return mResult;
	}

	void reset () {
		mReady  = false;
		mResult = NoError;
	}

	bool waitUntilReady (int timeOutMs) {
		Time t (futureInMs (timeOutMs));
		while (!mReady){
			if (!mCondition.timed_wait(schnee::mutex(), t))
				return false;
		}
		return true;
	}

	/// @depreciated.
	bool waitReadyAndNoError (int timeOutMs) {
		bool suc = waitUntilReady(timeOutMs);
		if (!suc) return false;
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
		mCondition.notify_all();
	}

	/// Connect this function (for VoidCallback)
	void onReady () {
		mReady  = true;
		mResult = NoError;
		mCondition.notify_all();
	}

	bool _isReady () { return mReady; } /// memFun is not yet const function compatible

	Condition mCondition;
	bool  mReady;
	Error mResult;
};


}
