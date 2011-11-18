#pragma once

#include <schnee/sftypes.h>

namespace boost {
class thread;
namespace asio {
class io_service;
}
}

namespace sf {

/**
 * A work thread is a thread who runs all the time (blocking) and who you can give some work
 * 
 * TODO: Should be merged in less general IOService class.
 * TODO: Should be added that it automatically waits for all pending operations.
 */
class WorkThread {
public:
	WorkThread ();
	~WorkThread ();
	
	/// Starts the work thread
	void start (const String & name = "WorkThread");
	/// Stops the work thread
	/// Note: it does not wait for pending data to be processed
	void stop ();
	
	/// Add work to the thread (returns an error if already finished)
	/// @return NotInitialized when thread is not started or already finished
	Error add (const VoidDelegate & f);
	
private:
	/// Thread is starting into here
	void onThreadStart ();
	/// Will change state to Started and thus notifies start that it is ready
	void startup ();
	
	enum State { NotStarted, Started, Finished };
		
	State mState;
	boost::thread * mThread;				///< Contains all thread data
	boost::asio::io_service * mIoService;	///< Contains an assiociated boost IOServices
	Mutex mMutex;
	Condition mCondition;
	String mName;
};

}
