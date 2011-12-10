#pragma once

#include <schnee/sftypes.h>
#include <schnee/Error.h>
#include <schnee/tools/async/Signal.h>

#include <sfserialization/autoreflect.h>
#include "../PresenceManagement.h"

namespace sf {

/**
 * PresenceProvider provies functions necessary to find information
 * about other peers (PresenceManagement) and to connect to presene networks.
 *
 * Its part of the GenericInterplexBeacon architecture.
 */
class PresenceProvider : public PresenceManagement {
public:

	///@name State
	///@{

	typedef function <void (sf::Error)> ResultDelegate;

	/// Starts connecting to a IM resource
	/// @return returns NoError if connection process began, false on error (e.g. invalid protocol / arguments)
	virtual sf::Error connect(const sf::String & connectionString, const sf::String & password = "", const ResultDelegate & callback = ResultDelegate()) = 0;

	/// Disconnect from the IM Network
	virtual void disconnect() = 0;

	///@}
};


}
