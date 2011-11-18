#pragma once

#include <string>

namespace autostart {

	/// Autostart is available on the current platform
	bool isAvailable ();

	/// Autostart is installed for given (user defined, not necessary case-sensitive (win32!)) program name
	/// Returns true on success
	bool isInstalled (const std::string & name);

	/// Install autostart for given path, returns true on success
	/// Returns true on success
	bool install (const std::string & name, const std::string & path, const std::string & parameters);

	/// Uninstall autostart for given (user defined) program name
	/// Returns true on success
	bool uninstall (const std::string & name);
};
