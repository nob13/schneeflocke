#pragma once
#include "ABind.h"
#include <schnee/sftypes.h>

namespace sf {

/**
 * @file
 * Special construction to insert parameters into functional functions.
 * @experimental.

 * Suppose you have a function f(g) which calls (after some calculation or some dependency) g(x)
 * and you want to use it with a function h(x,y), then you would usually call:
 *
 * f(abind(h, y)).
 *
 * But if you want to create function object k(y) --> f(abind(h,y)) then you need this ABindAround.
 */

template <class Param0, class Func, class FunctionalArg> static void execABindAround1 (Param0 arg, const Func & func, const FunctionalArg & farg) {
	// Hack: abind should be not so whiny with const...
	(const_cast<Func&>(func)) (abind(farg, arg));
}

template <class Param0, class Param1, class Func, class FunctionalArg> static void execABindAround2 (Param0 arg0, Param1 arg1, const Func & func, const FunctionalArg & farg) {
	// Hack: abind should be not so whiny with const...
	(const_cast<Func&>(func)) (abind(farg, arg0, arg1));
}

template <class Param0, class Param1, class Param2, class Func, class FunctionalArg> static void execABindAround3 (Param0 arg0, Param1 arg1, Param2 arg2, const Func & func, const FunctionalArg & farg) {
	// Hack: abind should be not so whiny with const...
	(const_cast<Func&>(func)) (abind(farg, arg0, arg1, arg2));
}

// Returns a function (Param0) which will call func (abind (farg, param0))
template <class Param0, class Func, class FunctionalArg> static function <void (Param0)> abindAround1 (const Func & func, const FunctionalArg & farg){
#ifdef _MSC_VER
	// MSVC needs some help (Error C2436)
	function<void (Param0, const Func&, const FunctionalArg&)> f = &execABindAround1<Param0, Func, FunctionalArg>;
	return abind (f, func, farg);
#else
	return abind (&execABindAround1<Param0, Func, FunctionalArg>, func, farg);
#endif
}

// Returns a function (Param0, Param1) which will call func (abind (farg, param0, param1))
template <class Param0, class Param1, class Func, class FunctionalArg> static function <void (Param0, Param1)> abindAround2 (const Func & func, const FunctionalArg & farg){
#ifdef _MSC_VER
	// MSVC needs some help (Error C2436)
	function<void (Param0, Param1, const Func&, const FunctionalArg&)> f = &execABindAround2<Param0, Param1, Func, FunctionalArg>;
	return abind (f, func, farg);
#else
	return abind (&execABindAround2<Param0, Param1, Func, FunctionalArg>, func, farg);
#endif
}

// Returns a function (Param0, Param1, Param2) which will call func (abind (farg, param0, param1, param2))
template <class Param0, class Param1, class Param2, class Func, class FunctionalArg> static function <void (Param0, Param1, Param2)> abindAround3 (const Func & func, const FunctionalArg & farg){
#ifdef _MSC_VER
	// MSVC needs some help (Error C2436)
	function<void (Param0, Param1, Param2, const Func&, const FunctionalArg&)> f = &execABindAround3<Param0, Param1, Param2, Func, FunctionalArg>;
	return abind (f, func, farg);
#else
	return abind (&execABindAround3<Param0, Param1, Param2, Func, FunctionalArg>, func, farg);
#endif
}


}
