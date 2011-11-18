#include "DelegateBase.h"
#include "impl/DelegateRegister.h"
#include <schnee/net/impl/IOService.h>

namespace sf {

void DelegateBase::registerMe (const char * name) {
	DelegateRegister::instance().registerMe(this, name);
}
void DelegateBase::unregisterMe () {
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

/// Executes an xcall
void executeXCall (const sf::function<void ()>& call){
	call ();
	DelegateRegister::instance().popCrossCall ();
}

void xcall (const sf::function<void ()> & call){
	DelegateRegister::instance().pushCrossCall ();
	sf::IOService::service().post (abind (&executeXCall, call));
}

/// Executes the timed xcall
void executeXCallTimed (boost::shared_ptr<TimedCallHandleOwner > handle, const sf::function<void ()> & call, const boost::system::error_code& ec){
	// shared_ptr will kill timer handle
	if (!ec)
		call ();
	DelegateRegister::instance().popCrossCall ();
}

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
	timer->async_wait (boost::bind (executeXCallTimed, owner, call, _1));
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
