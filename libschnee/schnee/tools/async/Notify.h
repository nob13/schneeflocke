#pragma once
/**
 * @file
 * Notify Easily calls a callback, if its set
 */

namespace sf {

/// Call the callback x if valid (0 Parameter)
template <class T> static void notify (const T & callback) {
	if (callback) callback();
}

/// Call the callback x if valid (1 Parameter)
template <class T, class X0> static void notify (const T & callback, const X0 & x0) {
	if (callback) callback(x0);
}

/// Call the callback x if valid (2 Parameter)
template <class T, class X0, class X1> static void notify (const T & callback, const X0 & x0, const X1 & x1) {
	if (callback) callback(x0, x1);
}

/// Call the callback x if valid (3 Parameter)
template <class T, class X0, class X1, class X2> static void notify (const T & callback, const X0 & x0, const X1 & x1, const X2 &x2) {
	if (callback) callback(x0, x1, x2);
}

/// Call the callback x if valid (4 Parameter)
template <class T, class X0, class X1, class X2, class X3> static void notify (const T & callback, const X0 & x0, const X1 & x1, const X2 &x2, const X3 & x3) {
	if (callback) callback(x0, x1, x2, x3);
}


}
