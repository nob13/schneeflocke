#pragma once
#include <schnee/sftypes.h>
#include "Datagram.h"

namespace sf {

/**
 * Interface for the communication delegate of a CommunicationComponent.
 */
class CommunicationDelegate {
public:
	virtual ~CommunicationDelegate () {}

	/// Send a Datagram to the specific receiver
	virtual sf::Error send     (const HostId & receiver, const sf::Datagram & datagram, const ResultCallback & callback = ResultCallback ()) = 0;

	/// Send a Datagram to multiple users
	virtual sf::Error send     (const HostSet & receivers, const sf::Datagram & datagram) = 0;

	/// Returns level of channel (also see ConnectionManagement)
	virtual int channelLevel (const HostId & receiver) = 0;
};

}
