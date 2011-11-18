#pragma once

#include <boost/type_traits.hpp>
/**
 * @file
 * MemFun Easy to use member function pointers
 */

namespace sf {

/// Adds a const reference to the type (Visual Studio forbids adding references to references)
template <class T> struct constref {
	typedef typename boost::add_const<T>::type const_type;
	typedef typename boost::add_reference<const_type>::type type;
	// typedef boost::add_reference < boost::add_const<T>::type >::type type;
};


template <class R, class Class> struct MemFun0 {

	typedef R (Class::*MemFun) ();

	MemFun0 (Class * _instance, MemFun _memPtr) : instance(_instance), memPtr(_memPtr) {
	}

	R operator () () {
		return (instance->*memPtr)();
	}
private:
	Class * instance;
	MemFun memPtr;
};

template <class R, class Class, class Arg0> struct MemFun1 {

	typedef R (Class::*MemFun) (Arg0);
	typedef typename constref<Arg0>::type Arg0CR;

	MemFun1 (Class * _instance, MemFun _memPtr) : instance(_instance), memPtr(_memPtr) {
	}

	R operator () (Arg0CR arg0) {
		return (instance->*memPtr)(arg0);
	}

private:
	Class * instance;
	MemFun memPtr;
};

template <class R, class Class, class Arg0, class Arg1> struct MemFun2 {

	typedef R (Class::*MemFun) (Arg0, Arg1);
	typedef typename constref<Arg0>::type Arg0CR;
	typedef typename constref<Arg1>::type Arg1CR;

	MemFun2 (Class * _instance, MemFun _memPtr) : instance(_instance), memPtr(_memPtr) {
	}

	R operator () (Arg0CR arg0, Arg1CR arg1) {
		return (instance->*memPtr)(arg0, arg1);
	}

private:
	Class * instance;
	MemFun memPtr;
};

template <class R, class Class, class Arg0, class Arg1, class Arg2> struct MemFun3 {

	typedef R (Class::*MemFun) (Arg0, Arg1, Arg2);
	typedef typename constref<Arg0>::type Arg0CR;
	typedef typename constref<Arg1>::type Arg1CR;
	typedef typename constref<Arg2>::type Arg2CR;

	MemFun3 (Class * _instance, MemFun _memPtr) : instance(_instance), memPtr(_memPtr) {
	}

	R operator () (Arg0CR arg0, Arg1CR arg1, Arg2CR arg2) {
		return (instance->*memPtr)(arg0, arg1, arg2);
	}

private:
	Class * instance;
	MemFun memPtr;
};

template <class R, class Class, class Arg0, class Arg1, class Arg2, class Arg3> struct MemFun4 {

	typedef R (Class::*MemFun) (Arg0, Arg1, Arg2, Arg3);

	MemFun4 (Class * _instance, MemFun _memPtr) : instance(_instance), memPtr(_memPtr) {
	}

	R operator () (typename constref<Arg0>::type arg0, typename constref<Arg1>::type arg1, typename constref<Arg2>::type arg2, typename constref<Arg3>::type arg3) {
		return (instance->*memPtr)(arg0, arg1, arg2, arg3);
	}

private:
	Class * instance;
	MemFun memPtr;
};


template <class R, class Class> static MemFun0<R,Class> memFun (Class * instance, R (Class::*memPtr)()){
	return MemFun0<R, Class> (instance, memPtr);
}

template <class R, class Class, class Arg0> static MemFun1<R,Class,Arg0> memFun (Class * instance, R (Class::*memPtr)(Arg0)){
	return MemFun1<R, Class, Arg0> (instance, memPtr);
}

template <class R, class Class, class Arg0, class Arg1> static MemFun2<R,Class,Arg0,Arg1> memFun (Class * instance, R (Class::*memPtr)(Arg0, Arg1)){
	return MemFun2<R, Class, Arg0, Arg1> (instance, memPtr);
}

template <class R, class Class, class Arg0, class Arg1, class Arg2> static MemFun3<R,Class,Arg0,Arg1,Arg2> memFun (Class * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2)){
	return MemFun3<R, Class, Arg0, Arg1, Arg2> (instance, memPtr);
}

template <class R, class Derived, class Class, class Arg0, class Arg1, class Arg2, class Arg3> static MemFun4<R,Class,Arg0,Arg1,Arg2,Arg3> memFun (Derived * instance, R (Class::*memPtr)(Arg0, Arg1, Arg2,Arg3)){
	return MemFun4<R, Class, Arg0, Arg1, Arg2,Arg3> (instance, memPtr);
}

}
