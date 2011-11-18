#pragma once
#include <schnee/sftypes.h>

/*
 * Init/DeInit functions for libschnee
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

/// A RAII guard for the initializers / deinitializers, you may use it instead of init / deInit
struct SchneeApp {
public:
	/// Initializes the application with your command line parameters
	SchneeApp (int argc = 0, const char* argv[] = 0) { init (argc, argv); }
	/// Initializes the application with your command line parameters
	SchneeApp (int argc, char ** argv) { init (argc, argv); }
	/// Deinitialization, by RAII or at the end of your program
	~SchneeApp () { deinit (); }
};


}

}
