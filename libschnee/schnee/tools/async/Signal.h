#pragma once
#include <schnee/sftypes.h>
#include <vector>

namespace sf {

/// A minimal signal implementation
/// libschnee is not supposed to use Signals very much
/// but in some cases its not easy to circumvent, e.g.
/// if you have seldom calling delegate function which
/// needs multiple recepients and subclassing (or
/// creating a decorator) is more complex than exchanging
/// for a signal.
/// Again: signals are neither fast nor do they create
/// readable code.
/// There is no support for return values.
template <class Signature> class Signal {
public:
	typedef sf::function<Signature> Handler;
	typedef std::vector<Handler> HandlerVector;

	/// Adds a handler to the signal
	void add (const Handler & handler) {
		mHandlers.push_back (handler);
	}

	/// Removes a handler, will be slow.
	/// Returns if handler is found
	bool remove (const Handler & handler) {
		for (typename HandlerVector::iterator i = mHandlers.begin(); i != mHandlers.end(); i++) {
			if (*i == handler) {
				mHandlers.erase(i);
				return true;
			}
		}
		return false;
	}

	/// clears all handlers
	void clear () {
		mHandlers.clear ();
	}

	/// Returns true if some handler is attached (for compatibility to delegates)
	operator bool () const {
		return !mHandlers.empty();
	}

	/// Call method without args
	void operator() () const {
		for (typename HandlerVector::const_iterator i = mHandlers.begin(); i != mHandlers.end(); i++) {
			(*i)();
		}
	}

	template <class Arg0>
	void operator() (const Arg0 & arg0) const {
		for (typename HandlerVector::const_iterator i = mHandlers.begin(); i != mHandlers.end(); i++) {
			(*i)(arg0);
		}
	}

	template <class Arg0, class Arg1>
	void operator() (const Arg0 & arg0, const Arg1 & arg1) const {
		for (typename HandlerVector::const_iterator i = mHandlers.begin(); i != mHandlers.end(); i++) {
			(*i)(arg0, arg1);
		}
	}

	template <class Arg0, class Arg1, class Arg2>
	void operator() (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2) const {
		for (typename HandlerVector::const_iterator i = mHandlers.begin(); i != mHandlers.end(); i++) {
			(*i)(arg0, arg1, arg2);
		}
	}

	template <class Arg0, class Arg1, class Arg2, class Arg3>
	void operator() (const Arg0 & arg0, const Arg1 & arg1, const Arg2 & arg2, const Arg3 & arg3) const {
		for (typename HandlerVector::const_iterator i = mHandlers.begin(); i != mHandlers.end(); i++) {
			(*i)(arg0, arg1, arg2, arg3);
		}
	}

private:
	HandlerVector mHandlers;
};


}
