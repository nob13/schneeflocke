#pragma once

#include <boost/type_traits.hpp>
/**
 * @file
 * Abind  Automatic binding of additional parameters to functions
 */

namespace sf {
template <class Signature, class X> struct ABind1 {
	ABind1 (const Signature & sig, const X & x) : mSig (sig), mX (x) {

	}

	void operator ()() {
		mSig (mX);
	}

	template <typename Arg0> void operator () (const Arg0 & arg0) {
		mSig (arg0, mX);
	}

	template <typename Arg0, typename Arg1> void operator () (const Arg0 & arg0, const Arg1 & arg1) {
		mSig (arg0, arg1 , mX);
	}

	template <typename Arg0, typename Arg1, typename Arg2> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2) {
		mSig (arg0, arg1, arg2 , mX);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3) {
		mSig (arg0, arg1, arg2, arg3 , mX);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4) {
		mSig (arg0, arg1, arg2, arg3, arg4 , mX);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4, const Arg5 & arg5) {
		mSig (arg0, arg1, arg2, arg3, arg4, arg5 , mX);
	}
	Signature mSig;
	typename boost::remove_reference<X>::type mX;
};

template <class Signature, class X0, class X1> struct ABind2 {
	ABind2 (const Signature & sig, const X0 & x0, const X1 & x1) : mSig (sig), mX0 (x0), mX1(x1) {

	}

	void operator ()() {
		mSig (mX0, mX1);
	}

	template <typename Arg0> void operator () (const Arg0 & arg0) {
		mSig (arg0, mX0, mX1);
	}

	template <typename Arg0, typename Arg1> void operator () (const Arg0 & arg0, const Arg1 & arg1) {
		mSig (arg0, arg1 , mX0, mX1);
	}

	template <typename Arg0, typename Arg1, typename Arg2> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2) {
		mSig (arg0, arg1, arg2 , mX0, mX1);
	}
	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3) {
		mSig (arg0, arg1, arg2, arg3 , mX0, mX1);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4) {
		mSig (arg0, arg1, arg2, arg3, arg4 , mX0, mX1);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4, const Arg5 & arg5) {
		mSig (arg0, arg1, arg2, arg3, arg4, arg5 , mX0, mX1);
	}

	Signature mSig;
	typename boost::remove_reference<X0>::type mX0;
	typename boost::remove_reference<X1>::type mX1;
};

template <class Signature, class X0, class X1, class X2> struct ABind3 {
	ABind3 (const Signature & sig, const X0 & x0, const X1 & x1, const X2 & x2) : mSig (sig), mX0 (x0), mX1(x1), mX2(x2) {

	}

	void operator ()() {
		mSig (mX0, mX1, mX2);
	}

	template <typename Arg0> void operator () (const Arg0 & arg0) {
		mSig (arg0, mX0, mX1, mX2);
	}

	template <typename Arg0, typename Arg1> void operator () (const Arg0 & arg0, const Arg1 & arg1) {
		mSig (arg0, arg1, mX0, mX1, mX2);
	}

	template <typename Arg0, typename Arg1, typename Arg2> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2) {
		mSig (arg0, arg1, arg2 , mX0, mX1, mX2);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3) {
		mSig (arg0, arg1, arg2, arg3 , mX0, mX1, mX2);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4) {
		mSig (arg0, arg1, arg2, arg3, arg4 , mX0, mX1, mX2);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4, const Arg5 & arg5) {
		mSig (arg0, arg1, arg2, arg3, arg4, arg5 , mX0, mX1, mX2);
	}

	Signature mSig;
	typename boost::remove_reference<X0>::type mX0;
	typename boost::remove_reference<X1>::type mX1;
	typename boost::remove_reference<X2>::type mX2;
};

template <class Signature, class X0, class X1, class X2, class X3> struct ABind4 {
	ABind4 (const Signature & sig, const X0 & x0, const X1 & x1, const X2 & x2, const X3 & x3) : mSig (sig), mX0 (x0), mX1(x1), mX2(x2), mX3(x3) {

	}

	void operator ()() {
		mSig (mX0, mX1, mX2, mX3);
	}

	template <typename Arg0> void operator () (const Arg0 & arg0) {
		mSig (arg0, mX0, mX1, mX2, mX3);
	}

	template <typename Arg0, typename Arg1> void operator () (const Arg0 & arg0) {
		mSig (arg0, mX0, mX1, mX2, mX3);
	}

	template <typename Arg0, typename Arg1, typename Arg2> void operator () (const Arg0 & arg0, const Arg1 & arg1) {
		mSig (arg0, arg1 , mX0, mX1, mX2, mX3);
	}

	template <typename Arg0, typename Arg1, typename Arg2> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2) {
		mSig (arg0, arg1, arg2 , mX0, mX1, mX2, mX3);
	}
	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3) {
		mSig (arg0, arg1, arg2, arg3 , mX0, mX1, mX2, mX3);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4) {
		mSig (arg0, arg1, arg2, arg3, arg4 , mX0, mX1, mX2, mX3);
	}

	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5> void operator () (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3, const Arg4 & arg4, const Arg5 & arg5) {
		mSig (arg0, arg1, arg2, arg3, arg4, arg5 , mX0, mX1, mX2);
	}

	Signature mSig;
	typename boost::remove_reference<X0>::type mX0;
	typename boost::remove_reference<X1>::type mX1;
	typename boost::remove_reference<X2>::type mX2;
	typename boost::remove_reference<X3>::type mX3;
};


template <class Signature, class X> static ABind1 <Signature, X> abind (const Signature & sig, const X & x) {
	return ABind1<Signature, X>  (sig, x);
}

template <class Signature, class X0, class X1> static ABind2 <Signature, X0, X1> abind (const Signature & sig, const X0 & x0, const X1 & x1) {
	return ABind2<Signature, X0, X1>  (sig, x0, x1);
}

template <class Signature, class X0, class X1, class X2> static ABind3 <Signature, X0, X1, X2> abind (const Signature & sig, const X0 & x0, const X1 & x1, const X2 & x2) {
	return ABind3<Signature, X0, X1, X2>  (sig, x0, x1, x2);
}

template <class Signature, class X0, class X1, class X2, class X3> static ABind4 <Signature, X0, X1,X2,X3> abind (const Signature & sig, const X0 & x0, const X1 & x1, const X2 & x2, const X3 & x3) {
	return ABind4<Signature, X0, X1, X2, X3>  (sig, x0, x1, x2, x3);
}

}
