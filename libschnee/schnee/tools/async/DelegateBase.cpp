#include "DelegateBase.h"
#include "impl/DelegateRegister.h"
#include <schnee/net/impl/IOService.h>
#include <schnee/schnee.h>

namespace sf {

void DelegateBase::registerMe (const char * name) {
	assert (schnee::mutex().try_lock() == false && "libschnee must be locked!");
	DelegateRegister::instance().registerMe(this, name);
}
void DelegateBase::unregisterMe () {
	assert (schnee::mutex().try_lock() == false && "libschnee must be locked!");
	DelegateRegister::instance().unregisterMe(this);
}

DelegateBase::~DelegateBase () {
	if (DelegateRegister::instance().isRegistered (mDelegateKey)){
		assert (!"You forgot to unregister me");
		SF_UNREGISTER_ME
	}
}

DelegateRegisterLock::DelegateRegisterLock (DelegateRegister::KeyType k) {
	mKey = k;
	mSuc  = DelegateRegister::slock (mKey);
}

DelegateRegisterLock::~DelegateRegisterLock (){
	if (mSuc) DelegateRegister::sunlock (mKey);
}

/*
 * This strange construction is necessary that
 * libschnee is always locked through an xcall.
 *
 * This is important if one parameter of an xcall
 * (somehere hidden in a boost::bind / abind
 * construction) is a shared_ptr, which gets
 * destructed upon releasing the xcall.
 *
 * libschnee has to be locked even then.
 *
 * I found no way to hook into Boost ASIO
 * to get it always locked.
 */

/// Holds the call to be xcalled
/// and makes sure its only called once.
struct CallHolder {
	CallHolder(const boost::function <void()> & f) : func (f) {
		hasCalled = false;
	}
	void call () {
		assert (!hasCalled && func);
		func ();
		func.clear();
		hasCalled = true;
	}
	void noCall (){
		func.clear();
	}
	boost::function<void ()> func;
	bool hasCalled;
};

/// Executes one xcall
struct ExecuteXCall {
	ExecuteXCall  (const ExecuteXCall & other) : mHolder (other.mHolder) {
	}
	ExecuteXCall (const function<void()> & func) : mHolder (new CallHolder (func)){
	}
	void operator ()() {
		SF_SCHNEE_LOCK;
		mHolder->call();
		DelegateRegister::instance().popCrossCall ();
	}
	shared_ptr<CallHolder> mHolder;
};

/// Executes one xcallTimed
struct ExecuteXCallTimed : public ExecuteXCall{
	ExecuteXCallTimed (const ExecuteXCallTimed & other) : ExecuteXCall (other), mHandle (other.mHandle) {
	}
	ExecuteXCallTimed (const function<void()> & func, const boost::shared_ptr<TimedCallHandleOwner> & handle) : ExecuteXCall (func), mHandle(handle) {
	}
	void operator() (const boost::system::error_code & ec){
		SF_SCHNEE_LOCK;
		if (!ec) {
			mHolder->call();
		} else {
			mHolder->noCall();
		}
		DelegateRegister::instance().popCrossCall ();
	}

	boost::shared_ptr<TimedCallHandleOwner> mHandle;
};

/*
/// Executes an xcall
void executeXCall (sf::function<void ()>& call){
	SF_SCHNEE_LOCK
	call ();
	call.clear(); // remove it inside the SF_SCHNEE_LOCK
	DelegateRegister::instance().popCrossCall ();
}
*/

void xcall (const sf::function<void ()> & call){
	DelegateRegister::instance().pushCrossCall ();
	ExecuteXCall pack (call);
	sf::IOService::service().post (pack);
}

/*
/// Executes the timed xcall
void executeXCallTimed (const boost::system::error_code& ec, boost::shared_ptr<TimedCallHandleOwner > handle, sf::function<void ()> & call){
	SF_SCHNEE_LOCK
	// shared_ptr will kill timer handle
	if (!ec)
		call ();
	call.clear();
	DelegateRegister::instance().popCrossCall ();
}
*/

TimedCallHandleOwner::TimedCallHandleOwner (void * instance) {
	mInstance = instance;
}
TimedCallHandleOwner::~TimedCallHandleOwner () {
	boost::asio::deadline_timer * realInstance = static_cast<boost::asio::deadline_timer *> (mInstance);
	delete realInstance;
}


TimedCallHandle xcallTimed (const sf::function<void ()> & call, const sf::Time& time) {
	DelegateRegister::instance().pushCrossCall ();
	boost::asio::deadline_timer * timer = new boost::asio::deadline_timer (sf::IOService::service(), time);
	shared_ptr<TimedCallHandleOwner> owner (new TimedCallHandleOwner (timer));
	// timer->async_wait (abind (&executeXCallTimed, owner, call));
	ExecuteXCallTimed pack (call, owner);
	timer->async_wait (pack);
	return TimedCallHandle(owner);
}

Error cancelTimer (TimedCallHandle & handle) {
	boost::shared_ptr<TimedCallHandleOwner> x = handle.lock();
	if (!x) return error::NotInitialized;
	handle = TimedCallHandle ();
	boost::asio::deadline_timer * realInstance = static_cast<boost::asio::deadline_timer *> (x->_instance());
	boost::system::error_code ec;
	realInstance->cancel (ec);
	if (ec) return error::Other;
	return NoError;
}

}
