#pragma once

#include <schnee/sftypes.h>
/**
 * @file
 * Various networking tool functions used in libschnee.
 */

namespace sf {
namespace net {
	/// Returns the name of current maschine
	String hostName ();
	
	/// Returns a list of local IP-Adresses (also IPv6 in Linux!)
	/// if withoutLocal is set to true (default), 127.0.0.0/8 and ::1 won't be returned
	/// @return true on success
	bool localAddresses(std::vector<sf::String> * dest, bool withoutLocal);

	/// Returns a list of local IPv4 Addresses
	bool localIpv4Addresses (std::vector<sf::String> * dest, bool withoutLocal);
}
}
