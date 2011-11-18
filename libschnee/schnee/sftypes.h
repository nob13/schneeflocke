#pragma once

/** @file
 * Common types, typedefs and small tools used everywhere in libschnee.
 */

#ifdef WIN32
#include "winsupport.h"
#endif


#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/thread/condition.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>
#include <set>
#include "tools/ByteArray.h"
#include "tools/Uri.h"
#include "tools/ToString.h"
#include "Error.h"

namespace sf {
	/// shorter name for Smart Pointers
	using boost::shared_ptr;
	using boost::weak_ptr;
	
	/// String which is used all over libschnee (plain C++-String)
	typedef std::string String;
	
	/// A smart byte array
	typedef shared_ptr<ByteArray> ByteArrayPtr;
	
	/// Create method for ByteArrayPtr
	template <class C> static ByteArrayPtr createByteArrayPtr (const C & data)
										{return ByteArrayPtr (new ByteArray (data)); }
	inline ByteArrayPtr createByteArrayPtr () { return ByteArrayPtr (new ByteArray());}
	
	/// Non recursive Mutex
	typedef boost::mutex Mutex;
	
	/// Recursive mutex (can be locked multiple times by the same thread)
	typedef boost::recursive_mutex RecursiveMutex;
	
	/// Smart Mutex Lock (RAII); we do not use lock_guard because this gives us support for Conditions
	typedef boost::unique_lock <Mutex> LockGuard;
	
	/// Recursive Lock guard (RAII)
	typedef boost::unique_lock <RecursiveMutex> RecursiveLockGuard;
	
	/// A condition we can wait for
	typedef boost::condition Condition;
	
	/// A key value for delegate / object tracking
	typedef int64_t DelegateKey;

	/// A key value for Asynchronous Operations (like network calls)
	typedef int64_t AsyncOpId;

	/// A User Identification (e.g. Jabber Bare Id)
	typedef String UserId;
	
	/// A host identification (e.g. Jabber Full Id)
	typedef String HostId; 

	/// A Set of Hosts
	typedef std::set<HostId> HostSet;

	/// A Set of Users
	typedef std::set<UserId> UserSet;

	/// A online state for a peer (and ourself)
	enum OnlineState { OS_OFFLINE = 0, OS_ONLINE, OS_UNKNOWN };
	const char* toString (OnlineState s);
	
	// for simple exchange
	using boost::function;
	using boost::bind;
	
	/// A simple delegate carrying no data
	typedef function<void ()> VoidDelegate;
	/// A simple callback carrying no data
	typedef function<void ()> VoidCallback;
	/// A simple callback carrying result of an operation
	typedef function<void (Error)> ResultCallback;
	
	/// Converses a integer to a hex value in String format
	template <class T> String toHex (const T & t){
		std::ostringstream stream;
		stream << "0x" << std::hex << t << std::dec;
		return stream.str();
	}
	
	/// Easy to use time value (casted by boost threads from  boost::posix_time::ptime).
	typedef boost::system_time Time;
	
	/// Returns current time
	inline Time currentTime (){
		return boost::get_system_time();
	}

	/// Returns the system time in ms milliseconds, often used for timeouts
	inline Time futureInMs (int ms){
		return boost::get_system_time() + boost::posix_time::milliseconds(ms);
	}
	
	/// Returns a time so negative that it is before everything
	inline Time negInfTime () {
		return Time (boost::date_time::neg_infin);
	}
	
	/// Returns a time so positive that is is after everything
	inline Time posInfTime () {
		return Time (boost::date_time::pos_infin);
	}

	/// Returns a often used timeout value. if ms <= 0 then the timeout is in infinitive future; otherwise it is futureInMs
	inline Time regTimeOutMs (int ms){
		return ms < 0 ? posInfTime () : futureInMs (ms);
	}
}


