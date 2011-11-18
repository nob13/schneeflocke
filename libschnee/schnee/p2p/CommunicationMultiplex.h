#pragma once

#include <schnee/sftypes.h>
#include <schnee/p2p/CommunicationComponent.h>

namespace sf {
/**
 * A Storage for different protocols, implementing the dispatch-Function of the CommunicationDelegate.
 * Note: The send-Function is not implemented, this will have to be done by further ancestors.
 */
class CommunicationMultiplex {
public:

	/// Creates a communication multiplex bound to a specific delegate
	CommunicationMultiplex ();
	virtual ~CommunicationMultiplex ();

	/// Sets the communication Delegate for all components
	void setCommunicationDelegate (CommunicationDelegate * delegate);
	
	/// Adds a protocol to the stack
	/// CommunicationMultiplex will delete it on it's end, if you don't remove it
	Error addComponent (CommunicationComponent * component);

	/// Removes a Protocol from the Multiplex
	/// Will deconnect it, but not delete it
	Error delComponent (CommunicationComponent * component);

	/// Returns a set of currently installed components
	std::set<const CommunicationComponent *> components () const;

	/// Inserts a Datagram into the dispatching circle (so that it will be brought to the specific component)
	Error dispatch (const HostId & sender, const String & cmd, const sf::Deserialization & ds, const ByteArray & data);

	/// Informs all CommunicationComponents, that a channel changed
	void distChannelChange (const HostId & host);
	
private:
	// Part implementation of CommunicationDelegate
	virtual sf::Error dispatch_locked (const HostId & sender, const String & cmd, const sf::Deserialization & ds, const ByteArray & data);
	
	
	typedef std::map<sf::String, CommunicationComponent*> CommunicationMap; ///< Maps Command to different CommunicationComponents
	typedef std::set<CommunicationComponent*> ComponentSet;
	CommunicationMap mCommunicationMap;
	ComponentSet mComponents;
	CommunicationDelegate * mCommunicationDelegate;
	mutable Mutex mComponentSetLock;
};

}
