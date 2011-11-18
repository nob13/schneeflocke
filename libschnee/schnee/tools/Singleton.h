#pragma once

/// @cond DEV

#include <assert.h>

namespace sf {

/** Small Singleton base class (not for multithreaded creation, more for subsystems which have an explicit point of init)
 *
 *  This class is not for external use.
 */
template <class T> 
class Singleton {
public:
	Singleton () {
	}
	
	// explicit initialization
	static void initInstance () {
		if (!instanceHolder()) instanceHolder() = new T;
	}
	
	/// explicit deinitialization
	static void destroyInstance (){
		delete instanceHolder(); instanceHolder() = 0;
	}
	
	/// Returns instance
	static T & instance () { 
		T * i = instanceHolder ();
		assert (i); // someone forgot to create an instance! 
		return *i;; 
	}
	
	/// Checks wether there is an instance
	static bool hasInstance () { return instanceHolder () != 0; }
	
private:
	static T * & instanceHolder () {
		static T * instance = 0;
		return instance;
	}
};

}

/// @endcond DEV
