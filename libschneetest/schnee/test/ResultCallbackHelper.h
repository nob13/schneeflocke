#include <schnee/sftypes.h>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/test/initHelpers.h>
#include <schnee/tools/async/MemFun.h>

namespace sf {
namespace test {

/**
 * Helper class to easily wait for asynchronous signals
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
		return test::waitUntilTrueMs (memFun (this, &ResultCallbackHelper::_isReady), timeOutMs);
	}

	bool waitReadyAndNoError (int timeOutMs) {
		return waitUntilReady (timeOutMs) && !mResult;
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

	bool  mReady;
	Error mResult;
};


}
}
