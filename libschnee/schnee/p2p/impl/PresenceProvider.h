#pragma once

#include <schnee/sftypes.h>
#include <schnee/Error.h>
#include <schnee/tools/async/Signal.h>

#include <sfserialization/autoreflect.h>
#include "../PresenceManagement.h"

namespace sf {

/**
 * PresenceProvider provies functions necessary to find information
 * about other peers (PresenceManagement) and to connect to presence networks.
 *
 * Its part of the GenericInterplexBeacon architecture.
 */
class PresenceProvider : public PresenceManagement {
public:

	///@name State
	///@{

	typedef function <void (sf::Error)> ResultDelegate;

	/// Sets connection details. hostId will be valid after this call.
	virtual Error setConnectionString (const String & connectionString, const String & password) = 0;

	/// Starts connecting to a IM resource
	/// @return returns NoError if connection process began, false on error (e.g. invalid protocol / arguments)
	virtual sf::Error connect(const ResultDelegate & callback = ResultDelegate()) = 0;

	/// Disconnect from the IM Network
	virtual void disconnect() = 0;

	///@}
};


}
