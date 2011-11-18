#include "DeadlineTimer.h"

#include <boost/asio.hpp> // For the Timer
#include <schnee/net/impl/IOService.h>

namespace sf {

struct DeadlineTimerControl {
	sf::Mutex mutex;
	sf::VoidDelegate callback;
	boost::asio::deadline_timer * timer;
	bool calling;
	bool canceled; // for debugging

	DeadlineTimerControl(){
		timer = 0;
		calling = false;
		canceled = false;
	}

	~DeadlineTimerControl (){
		delete timer;
	}
};

typedef sf::shared_ptr<DeadlineTimerControl> ControlPtr;

static void timerHandler (sf::shared_ptr<DeadlineTimerControl> control, boost::system::error_code ec){
	if (ec == boost::asio::error::operation_aborted) return;
	sf::LockGuard guard (control->mutex);
	control->calling = true;
	if (control->callback) control->callback ();
	control->calling = false;
}

DeadlineTimer::DeadlineTimer () {
}

DeadlineTimer::~DeadlineTimer () {
	cancel ();
}

void DeadlineTimer::start (const sf::Time & timeOut) {
	cancel();
	mControl = ControlPtr  (new DeadlineTimerControl);
	sf::LockGuard guard (mControl->mutex);
	mControl->callback = mTimeout;
	mControl->timer = new boost::asio::deadline_timer (IOService::service(), timeOut);
	mControl->timer->async_wait (sf::bind (timerHandler, mControl, _1));
}

void DeadlineTimer::cancel () {
	if (mControl){
		if (!beingTimer()){
			mControl->mutex.lock ();
			mControl->timer->cancel ();
			mControl->callback = VoidDelegate ();
			mControl->canceled = true;
			mControl->mutex.unlock ();
		} else {
			mControl->callback = VoidDelegate ();
			mControl->canceled = true;
		}
	}
	mControl = ControlPtr ();
}

bool DeadlineTimer::beingTimer () const {
	return mControl && mControl->calling && IOService::isCurrentThreadService(IOService::service());
}

}
