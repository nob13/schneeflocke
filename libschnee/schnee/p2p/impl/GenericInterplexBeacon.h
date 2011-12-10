#pragma once

#include <schnee/p2p/InterplexBeacon.h>

#include <schnee/p2p/impl/PresenceProvider.h>
#include <schnee/tools/async/DelegateBase.h>
#include <deque>
#include "GenericConnectionManagement.h"

namespace sf {

/**
 * GenericInterplexBeacon is an InterplexBeacon implementation
 * which works together with ChannelProvider and PresenceProvider
 *
 * This Interplex doesn't build up connections automatically you have
 * to use liftConnection etc.
 */
class GenericInterplexBeacon : public InterplexBeacon, public DelegateBase {
public:
	typedef shared_ptr<PresenceProvider> PresenceProviderPtr;
	typedef shared_ptr<ChannelProvider>  ChannelProviderPtr;

	GenericInterplexBeacon ();
	virtual ~GenericInterplexBeacon ();

	///@name Init
	///@{

	/// Connect to a Presenceprovider
	void setPresenceProvider (PresenceProviderPtr presenceProvider);

	///@}
	
	// Implementation of InterplexBeacon
	virtual Error connect (const String & connectionString, const String & password = "", const ResultCallback & callback = ResultCallback());
	virtual void disconnect ();

	virtual PresenceManagement & presences ();
	virtual const PresenceManagement & presences () const;
	virtual CommunicationMultiplex & components ();
	virtual GenericConnectionManagement & connections (); // overwriting return type with derived
	virtual const GenericConnectionManagement & connections() const;

private:

	/// Callback for Onlien State Changes
	void onOnlineStateChanged (OnlineState os);
	/// Callback for Peers list changed
	void onPeersChanged ();
	
	PresenceProviderPtr mPresenceProvider; ///< Current presence provider
	
	PresenceManagement::HostInfoMap mHosts;
	CommunicationMultiplex      mCommunicationMultiplex;
	GenericConnectionManagement mConnectionManagement;

	mutable sf::Mutex mMutex;
};


}
