#pragma once
#include <schnee/tools/Singleton.h>
#include <schnee/sftypes.h>
#include <boost/type_traits/remove_reference.hpp>

#ifdef WIN32
// Also tested in Linux, but seems bloaty
#include <boost/thread.hpp>
typedef boost::thread::id ThreadId;
inline ThreadId currentThreadId () { return boost::this_thread::get_id(); }
#else
typedef pthread_t ThreadId;
inline ThreadId currentThreadId () { return pthread_self (); }
#endif

/**
 * @file
 * The automatic delegate register system.
 * Important for users are the base class DelegateBase
 * the macros SF_REGISTER_ME and SF_UNREGISTER_ME
 * and the functions sf::xcall and sf::dMemFun.
 */

namespace sf {
class DelegateBase;

// DelegateRegister is not needed for users, just DelegateBase
///@cond DEV


/**
 * A global register for all objects which want to have thread safe delegates.
 * Used through the macros SF_REGISTER_ME and SF_UNREGISTER_ME. Usually you
 * should not come into touch with this class directly, but with the functions
 * dMemFun, which generates a thread safe callback which will call only if the
 * registered object of the member function still exist. For that functionality
 * you have to derive from DelegateBase.
 *
 *
 */
class DelegateRegister : public sf::Singleton<DelegateRegister> {
public:
	friend class sf::Singleton<DelegateRegister>;
	typedef sf::DelegateKey KeyType;

	/// Installs object into the repository
	void registerMe (DelegateBase * me, const char * desc = 0);

	/// Removes object from the repository
	void unregisterMe (DelegateBase * base);

	/// Checks whether object is registered
	bool isRegistered (KeyType key);

	/// Locks a object for calling
	bool lock (KeyType key);

	/// Shortcut to lock()
	static bool slock (KeyType key) { return instance().lock(key); }

	/// Unlocks a object from calling
	void unlock (KeyType key);

	/// Shortcut to unlock()
	static void sunlock (KeyType key) { instance().unlock (key); }

	/// Locks a cross call (from someone else) - used in xcall (..., DelegateBase*)
	bool crossLock (KeyType key);
	/// Unlocks a cross call (from someone else) - used in xcall (..., DelegateBase*)
	void crossUnlock (KeyType key);

	/// Pushes a cross call to the service (so that we can wait for them all at the end)
	void pushCrossCall ();

	/// Pops a cross call from the service
	void popCrossCall ();

	/// Finish the Register (waiting for all open cross calls to be done)
	void finish ();

private:
	friend class DelegateBase;

	/// Tries to get the lock of some Info element
	bool tryGetLock_locked (KeyType key, const ThreadId & tid,  bool & found);

	DelegateRegister();
	~DelegateRegister ();

	struct Info {
		Info () : locked (0), crossLock (false), calls (0), desc (0), base (0) {}

		int 		   		locked;	// lock level
		ThreadId   			locker;	// thread who locked
		bool 				crossLock;	// locked by xcall
		int64_t 	   		calls;
		const char *   		desc;
		DelegateBase * 		base;
	};
	typedef std::map<KeyType, Info> InfoMap;

	InfoMap mInfos;
	KeyType mNextKey;
	sf::Mutex mMutex;
	sf::Condition mCondition;

	int mCrossCalls;				///< How many cross thread calls are pending
	bool mFinishing;				///< Before closing the object.
};

///@endcond DEV

}
