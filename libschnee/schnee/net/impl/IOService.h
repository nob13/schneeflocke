#pragma once

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <schnee/tools/Singleton.h>
#include <schnee/sftypes.h>

/// @cond DEV

namespace sf {


/**
 * A Singleton helper class managing the Asio IOService for us altogether with some tool functions
 * 
 * This class is not designed for use outside of libschnee.
 * 
 */
class IOService : public Singleton<IOService>{
public:
	/// Returns the asio service
	boost::asio::io_service & getService () { return mService; }
	/// Returns the asio service (shortcut)
	static boost::asio::io_service & service () { return instance().getService(); } 
	
	/// Start the io_service event loop (usually done via schnee::init())
	void start ();
	
	/// Stops  the io_service event loop (usually done via schnee::deinit or program destruction)
	void stop ();
	
	/// Returns thread id of a service
	static boost::thread::id threadId (const boost::asio::io_service& s);
	
	/// Whether current thread is the given boost::asio::service
	static bool isCurrentThreadService (const boost::asio::io_service & service);

	/// Whether current thread is the IOService
	static bool isCurrentThread () { return threadId(service()) == boost::this_thread::get_id(); }
private:
	friend class Singleton<IOService>;
	IOService ();
	~IOService ();

	/// First operation of starting IOService
	void starting ();
	/// Called by the thread; runs mService.run ()
	void run ();
	boost::asio::io_service mService;
	boost::thread * mThread;
	// We have to wait until IOService is running
	Condition mStateChange;
	Mutex mMutex;
	bool mRunning;
};

}

/// @endcond DEV
