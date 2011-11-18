#pragma once

#include <schnee/sftypes.h>

namespace sf {

///@cond DEV
struct DeadlineTimerControl;

/**
 * A nice deadline timer implementation (it decorates the Boost.Asio Timer), whose lock blocks during destruction until timer is called.
 * (So you won't get a timer callback after deleting the timer)
 * 
 * Note: It is losing relevance since the existance of DelegateRegister.
 */
class DeadlineTimer {
public:
	DeadlineTimer ();

	~DeadlineTimer ();

	/// Delegate which will be called on timeout
	VoidDelegate & timeOut () { return mTimeout; }

	/// Starts the timer
	void start (const sf::Time & timeOut);

	/// Cancels the timer
	void cancel ();

	/// Returns true if current thread is the timer
	bool beingTimer () const;

private:
	VoidDelegate mTimeout;
	sf::shared_ptr<DeadlineTimerControl> mControl; ///< Control structure for the timer (must be deleted after Timer - Callback)
};


///@endcond DEV
}
