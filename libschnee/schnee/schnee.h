#pragma once
#include <schnee/sftypes.h>

/*
 * init / deinit and fundamental functions for libschnee
 */

namespace sf {

/// Initalization / Deinitalization functions for libschnee.
namespace schnee {

/** Initializes libschnee
 * 
 *  If you want that it parses some everywhere used arguments (like --enableLog) 
 *  than you can pass in argc/argv. 
 * 
 * 	@return true on success
 * 
 */ 
bool init (int argc = 0, const char * argv[] = 0);
/// auto converting for lazy programmers, see the other init description 
inline bool init (int argc, char ** argv) { return init (argc, (const char**) argv); }

/// Deinitializes libschnee
void deinit ();

/// Returns true if libschnee is initialized
bool isInitialized ();

/// Returns the text version of this libschnee distribution
const char * version ();

/// Returns the main mutex for libschnee
/// Note: All calls to libschnee must go through this Mutex
/// Callbacks / Delegate from libschnee will come with this
/// Mutex locked
Mutex & mutex ();

/// A Macro for locking libschnee
/// Is used internally on all asynchronous handlers
#define SF_SCHNEE_LOCK sf::LockGuard _schneelock (sf::schnee::mutex());

/// A RAII guard for the initializers / deinitializers, you may use it instead of init / deInit
struct SchneeApp {
public:
	/// Initializes the application with your command line parameters
	SchneeApp (int argc = 0, const char* argv[] = 0) {
		LockGuard guard (mutex());
		init (argc, argv);
	}
	/// Initializes the application with your command line parameters
	SchneeApp (int argc, char ** argv) {
		init (argc, argv);
	}
	/// Deinitialization, by RAII or at the end of your program
	~SchneeApp () {
		deinit ();
	}
};


}

}
