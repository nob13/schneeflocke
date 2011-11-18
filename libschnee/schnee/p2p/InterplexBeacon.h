#pragma once

#include <schnee/tools/Singleton.h>
#include <schnee/tools/Serialization.h>
#include <schnee/p2p/Datagram.h>
#include <schnee/p2p/PresenceManagement.h>

#include "CommunicationMultiplex.h"
#include "ConnectionManagement.h"

namespace sf {

/**
 * The facade of the whole Bootstrapping process.
 *
 * - It holds the different communication components (with CommunicationMultiplex in components)
 * - Have connections to the other peers (you can send them data via the send () method, but this
 *   is usually done by the communication components).
 *
 * - TODO: It would be better to make InterplexBeacon a PresenceProvider as it already has 70% of the methods
 *
 * The name was a cool thing in StarTrek 8.
 */
class InterplexBeacon {
public:
	typedef function<void (OnlineState os)> OnlineStateChangedDelegate;


	InterplexBeacon ();
	virtual ~InterplexBeacon ();

	///@name State
	///@{

	/// Connect to the Network
	/// This function calls back if you want to be notified if connection was successfull (NoError) or failed
	/// Callback will be called always, even if connect() returns an error (but from a different thread)
	virtual Error connect (const String & connectionString, const String & password = "", const ResultCallback & callback = ResultCallback()) = 0;

	/// Waits until IMInterplexBeacon is connected
	virtual Error waitForConnected () = 0;

	/// Disconnects from IM Network
	virtual void disconnect () = 0;

	///@}
	
	///@name Presences
	///@{
	/// Gives you access to an associated the presence provider
	virtual PresenceManagement & presences () = 0;
	virtual const PresenceManagement & presences () const = 0;
	///@}

	///@name Communication Components
	///@{
	
	/// Gives you access to the communication multiplex of the beacon
	/// Here you can add/remove communication components
	virtual CommunicationMultiplex & components () = 0;
	
	///@}

	///@name Connections to other peers
	///@{

	/// Gives you access to the connection mangement
	virtual ConnectionManagement & connections() = 0;
	virtual const ConnectionManagement & connections() const = 0;
	
	///@}
	
	///@name Creators
	///@{

	/// Creates an Interplex Beacon which uses Instant Messaging protocols for generating the channels
	static InterplexBeacon * createIMInterplexBeacon ();

	///@}
};

}
