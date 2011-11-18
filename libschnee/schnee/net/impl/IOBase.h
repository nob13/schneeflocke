#pragma once

#include <schnee/sftypes.h>
#include <boost/asio.hpp>


///@cond DEV

namespace sf {

/// Base class for IO operations. Providing ioService connection, and State/Change mutex simplification
/// Note: when you create a async process you have to increment mPendingOperations
/// Inside the handler (and inside a mutex lock) you have to decrement it again
///
/// All operations shall lock the internal state; operations who don't have the suffix _locked
/// Some operations need to be started from IOService context because otherwise windows couldn't cancel it


/// Deletion: Delete all IOBase objects via requestDelete()
class IOBase {
public:
	IOBase (boost::asio::io_service & service);

	/// Request the deletion of the object (calls deleteItSelf from IOService-Thread)
	void requestDelete ();

public:
	
	void setError (Error error, const String & msg){
		{
			LockGuard lock (mStateMutex);
			setError_locked (error, msg);
		}
		mStateChange.notify_all ();
	}
	

	Error error () const {
		LockGuard lock (mStateMutex);
		return mError;
	}
	
	String errorMessage () const {
		LockGuard guard (mStateMutex);
		return mErrorMessage;
	}
	
protected:
	void setError_locked (Error error, const String & msg){
		mError = error;
		mErrorMessage = msg;
	}

	/// Calls a result callback with appropriate locking
	/// Note: do not call this function on user callls, only on IOService calls!
	void notifyCallback_locked (ResultCallback * cb, Error result);

	/// Notifies a delegate (does NOT clears it)
	void notifyDelegate_locked (const VoidCallback & del);

	/// Causes the IOBase to delete it self.
	/// Make sure that you overwrite it in order to cancel pending connections (and afterwards call this method!)
	/// Also disable all callbacks!
	virtual void onDeleteItSelf ();
	virtual ~IOBase ();

	boost::asio::io_service & mService;
	Error mError;
	String mErrorMessage;
	bool mToDelete;										///< Object is to delete
	
	// internal state locking
	mutable Mutex mStateMutex;							///< Some internal data changed
	Condition mStateChange;						///< Condition for waiting that something changes
	
	int mPendingOperations;
private:

	/// Is called from deleteItSelf at the end and really deletes the object (or waits until no more calls)
	void realDelete ();

	int mDeletionTries;									///< How many tries where to delete the object?
};

}

///@endcond DEV
