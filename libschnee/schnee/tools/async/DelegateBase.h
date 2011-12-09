#pragma once

#include <schnee/sftypes.h>
#include <schnee/Error.h>
#include "MemFun.h"
#include "ABind.h"
#include "Notify.h"
#include <boost/weak_ptr.hpp>
#ifdef WIN32
#define SF_FUNCTION __FUNCTION__
#else
#define SF_FUNCTION __func__
#endif

/// This must be your first Command in your class constructor if you use the DelegateBase base class, it will register your object.
#define SF_REGISTER_ME registerMe(SF_FUNCTION);
/// Similar to SF_REGISTER_ME but you can choose your own name
#define SF_REGISTER_ME_WN(X) registerMe(X);
/// This must be your first command in your class deconstructor, it will unregister your object, all delegates will now silently fail.
#define SF_UNREGISTER_ME unregisterMe();

namespace sf{
/** Base class for all who want to receive thread-safe delegates.
    If you derive from this class you should add your class to the delegate register
    by using the macros #SF_REGISTER_ME and #SF_UNREGISTER_ME

    Afterwards you can use dMemFun to connect to function objects / callbacks / delegates.
    @verbatim
    // Consider a delegate like this:
    typedef sf::delegate<void (int, const String & x)> Delegate;
    Delegate d;
    // And your object is this:
    struct Object {
		Object () { SF_REGISTER_ME; }
		~Object () { SF_UNREGISTER_ME; }
		void method (int x, const String & x);
    };
    // Now you can connect your objects method:
    Object * o = new Object ();
    d = sf::dMemFun (o, &Object::method);
    d (3, "Hello"); // will call your objects method.
    delete o;
    d (4, "Bonjour"); // this call will now silently fail - and not crash your system.

    // You can even let the system call for you
    sf::xcall (sf::abind (d, 4, "Hello"));  // will call d(4, "Hello") - but from another (IO-)thread, will come back immediately.
    // (of course the call will also fail, because the object o does not exist any more).
    // Note: sf::abind appends some arguments onto a function call and saves it in structure - like boost::bind, but a bit faster.
    // So 4 and "Hello" will be stored, which saves them from deletion until xcall executes the call from the IO thread.
    @endverbatim

 */
class DelegateBase {
public:
	DelegateBase () : mDelegateKey (0) {}

	/// Returns a unique key for all registered objects
	inline DelegateKey delegateKey () const {
#ifndef NDEBUG
		if (!mDelegateKey){
			std::cerr << "Taking delegate key from non-registered function, did you register me?" << std::endl;
		}
#endif
		return mDelegateKey;
	}

protected:
	~DelegateBase ();

	void registerMe (const char * name = 0);
	void unregisterMe ();

private:
	friend class DelegateRegister;
	DelegateKey mDelegateKey;
};

/// Executes one call in the IOService thread
/// Note: schnee lock will be set
void xcall (const sf::function<void ()> & call);


template <class T> static void holdIt (const shared_ptr<T> & x) {
	// no function
}

/// Helps on disposing shared_ptr objects, e.g. if you are called by one of this
/// Instead of just 0'ing that pointer, call safeRemofe it will rescue the possible
/// deletion to the next cycle.
template <class T> void safeRemove (shared_ptr<T> & x){
	// the xcall mechanism will hold it
	xcall (abind (&holdIt, x));
}

/// Call the callback x asynchronously, if valid (0 Parameter)
template <class T> static void notifyAsync (const T & callback) {
	if (callback) xcall (callback);
}

/// Call the callback x asynchronously, if valid (0 Parameter)
template <class T, class X0> static void notifyAsync (const T & callback, const X0 & x0) {
	if (callback) xcall (abind (callback, x0));
}

/// Call the callback x asynchronously, if valid (0 Parameter)
template <class T, class X0, class X1> static void notifyAsync (const T & callback, const X0 & x0, const X1 & x1) {
	if (callback) xcall (abind (callback, x0, x1));
}

/// Call the callback x asynchronously, if valid (0 Parameter)
template <class T, class X0, class X1, class X2> static void notifyAsync (const T & callback, const X0 & x0, const X1 & x1, const X2 &x2) {
	if (callback) xcall (abind (callback, x0, x1, x2));
}

/// Call the callback x asynchronously, if valid (0 Parameter)
template <class T, class X0, class X1, class X2, class X3> static void notifyAsync (const T & callback, const X0 & x0, const X1 & x1, const X2 &x2, const X3 & x3) {
	if (callback) xcall (abind (callback, x0, x1, x2, x3));
}

struct TimedCallHandleOwner {
	TimedCallHandleOwner (void * instance);
	~TimedCallHandleOwner ();
	/// internal use only
	void * _instance () { return mInstance;}
private:
	TimedCallHandleOwner (const TimedCallHandleOwner &);
	TimedCallHandleOwner & operator= (const TimedCallHandleOwner);
	void * mInstance;
};

typedef boost::weak_ptr<TimedCallHandleOwner> TimedCallHandle;

/// Executes one call in the IOService thread, with some delay
/// Returns you some handle, so that you can destroy the timer again
TimedCallHandle xcallTimed (const sf::function<void ()> & call, const sf::Time& time);

/// Cancel an xcallTimed handler
/// May fail if timer was already called
Error cancelTimer (TimedCallHandle & handle);

// The following code implements dMemFun

///@cond DEV

struct DelegateRegisterLock {
	DelegateRegisterLock (DelegateKey k);

	inline bool suc () const { return mSuc;}
	~DelegateRegisterLock ();

private:
	DelegateKey mKey;
	bool mSuc;
};

template <class R, class Class> struct DMemFun0 {

	typedef R (Class::*MemFun) ();

	DMemFun0 (Class * _instance, MemFun _memPtr, DelegateKey _key) : instance(_instance), memPtr(_memPtr), key (_key) {
	}

	R operator () () {
		DelegateRegisterLock lock (key);
		if (lock.suc()) return (instance->*memPtr) ();
		else return R();
	}
private:
	Class * instance;
	MemFun memPtr;
	DelegateKey key;
};

template <class R, class Class, class Arg0> struct DMemFun1 {

	typedef R (Class::*MemFun) (Arg0);
	typedef typename constref<Arg0>::type Arg0CR;

	DMemFun1 (Class * _instance, MemFun _memPtr, DelegateKey _key) : instance(_instance), memPtr(_memPtr), key (_key) {
	}

	R operator () (Arg0CR arg0) {
		DelegateRegisterLock lock (key);
		if (lock.suc()) return (instance->*memPtr)(arg0);
		return R();
	}

private:
	Class * instance;
	MemFun memPtr;
	DelegateKey key;
};

template <class R, class Class, class Arg0, class Arg1> struct DMemFun2 {

	typedef R (Class::*MemFun) (Arg0, Arg1);
	typedef typename constref<Arg0>::type Arg0CR;
	typedef typename constref<Arg1>::type Arg1CR;

	DMemFun2 (Class * _instance, MemFun _memPtr, DelegateKey _key) : instance(_instance), memPtr(_memPtr), key (_key) {
	}

	R operator () (Arg0CR arg0, Arg1CR arg1) {
		DelegateRegisterLock lock (key);
		if (lock.suc()) return (instance->*memPtr)(arg0, arg1);
		return R();
	}

private:
	Class * instance;
	MemFun memPtr;
	DelegateKey key;
};

template <class R, class Class, class Arg0, class Arg1, class Arg2> struct DMemFun3 {

	typedef R (Class::*MemFun) (Arg0, Arg1, Arg2);
	typedef typename constref<Arg0>::type Arg0CR;
	typedef typename constref<Arg1>::type Arg1CR;
	typedef typename constref<Arg2>::type Arg2CR;

	DMemFun3 (Class * _instance, MemFun _memPtr, DelegateKey _key) : instance(_instance), memPtr(_memPtr), key (_key) {
	}

	R operator () (Arg0CR arg0, Arg1CR arg1, Arg2CR arg2) {
		DelegateRegisterLock lock (key);
		if (lock.suc()) return (instance->*memPtr)(arg0, arg1, arg2);
		return R();
	}

private:
	Class * instance;
	MemFun memPtr;
	DelegateKey key;
};

template <class R, class Class, class Arg0, class Arg1, class Arg2, class Arg3> struct DMemFun4 {

	typedef R (Class::*MemFun) (Arg0, Arg1, Arg2, Arg3);

	DMemFun4 (Class * _instance, MemFun _memPtr, DelegateKey _key) : instance(_instance), memPtr(_memPtr), key (_key) {
	}

	R operator () (typename constref<Arg0>::type arg0, typename constref<Arg1>::type arg1, typename constref<Arg2>::type arg2, typename constref<Arg3>::type arg3) {
		DelegateRegisterLock lock (key);
		if (lock.suc()) return (instance->*memPtr)(arg0, arg1, arg2, arg3);
		return R();
	}

private:
	Class * instance;
	MemFun memPtr;
	DelegateKey key;
};

/*
 * dMemFun can be called like this
 * dMemFun(GenericClass, GenericClass::Function, const DelegateBase * )
 * or
 * dMemFun(GenericClass (derived from DelegateBase), GenericClass::Function)
 *
 * The second one shall be the general use case, the first one can be used
 * to bring delegate to subcomponents (which life will up to the DelegateBase destructor, guaranteed)
 */

template <class R, class Class> static DMemFun0<R,Class> dMemFun (Class * instance, R (Class::*memPtr)(), const DelegateBase * delBase){
	return DMemFun0<R, Class> (instance, memPtr, delBase->delegateKey());
}

template <class R, class Class> static DMemFun0<R,Class> dMemFun (Class * instance, R (Class::*memPtr)() ){
	const DelegateBase * delBase = instance;
	return dMemFun (instance, memPtr, delBase);
}

template <class R, class Class, class Arg0> static DMemFun1<R,Class,Arg0> dMemFun (Class * instance, R (Class::*memPtr)(Arg0), const DelegateBase * delBase ){
	return DMemFun1<R, Class, Arg0> (instance, memPtr, delBase->delegateKey());
}

template <class R, class Class, class Arg0> static DMemFun1<R,Class,Arg0> dMemFun (Class * instance, R (Class::*memPtr)(Arg0) ){
	const DelegateBase * delBase = instance;
	return dMemFun (instance, memPtr, delBase);
}

template <class R, class Class, class Arg0, class Arg1> static DMemFun2<R,Class,Arg0,Arg1> dMemFun (Class * instance, R (Class::*memPtr)(Arg0, Arg1), const DelegateBase * delBase ){
	return DMemFun2<R, Class, Arg0, Arg1> (instance, memPtr, delBase->delegateKey());
}

template <class R, class Class, class Arg0, class Arg1> static DMemFun2<R,Class,Arg0,Arg1> dMemFun (Class * instance, R (Class::*memPtr)(Arg0, Arg1) ){
	const DelegateBase * delBase = instance;
	return dMemFun (instance, memPtr, delBase);
}

// special function for derived ones (TODO: also add for the others)
template <class R, class Derived, class Class, class Arg0, class Arg1> static DMemFun2<R,Class,Arg0,Arg1> dMemFun (Derived * instance, R (Class::*memPtr)(Arg0, Arg1) ){
	DelegateBase * delBase = instance;
	Class * c = instance;
	return DMemFun2<R, Class, Arg0, Arg1> (c, memPtr, delBase->delegateKey());
}

template <class R, class Class, class Arg0, class Arg1, class Arg2> static DMemFun3<R,Class,Arg0,Arg1,Arg2> dMemFun (Class * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2), const DelegateBase * delBase ){
	return DMemFun3<R, Class, Arg0, Arg1, Arg2> (instance, memPtr, delBase->delegateKey());
}

template <class R, class Class, class Arg0, class Arg1, class Arg2> static DMemFun3<R,Class,Arg0,Arg1,Arg2> dMemFun (Class * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2) ){
	DelegateBase * delBase = instance;
	return dMemFun (instance, memPtr, delBase);
}

// special function for derived ones (TODO: also add for others)

template <class R, class Derived, class Class, class Arg0, class Arg1, class Arg2> static DMemFun3<R,Class,Arg0,Arg1,Arg2> dMemFun (Derived * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2), const DelegateBase * delBase ){
	Class * c = instance;
	return DMemFun3<R, Class, Arg0, Arg1, Arg2> (c, memPtr, delBase->delegateKey());
}

template <class R, class Derived, class Class, class Arg0, class Arg1, class Arg2> static DMemFun3<R,Class,Arg0,Arg1,Arg2> dMemFun (Derived * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2) ){
	DelegateBase * delBase = instance;
	return dMemFun (instance, memPtr, delBase);
}

template <class R, class Derived, class Class, class Arg0, class Arg1, class Arg2, class Arg3> static DMemFun4<R,Class,Arg0,Arg1,Arg2,Arg3> dMemFun (Derived * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2,Arg3), const DelegateBase * delBase ){
	Class * c = instance;
	return DMemFun4<R, Class, Arg0, Arg1, Arg2,Arg3> (c, memPtr, delBase->delegateKey());
}

template <class R, class Derived, class Class, class Arg0, class Arg1, class Arg2, class Arg3> static DMemFun4<R,Class,Arg0,Arg1,Arg2,Arg3> dMemFun (Derived * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2,Arg3) ){
	DelegateBase * delBase = instance;
	Class * c = instance;
	return dMemFun (c, memPtr, delBase);
}

///@endcond DEV
}
