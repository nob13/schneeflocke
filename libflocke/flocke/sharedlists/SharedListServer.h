#pragma once

#include <schnee/sftypes.h>
#include <schnee/p2p/DataSharingServer.h>
#include "SharedList.h"

namespace sf {

/**
 * Maintains a list of shared elements (as defined in SharedList.h)
 * shared inside DataSharingServer at the path "shared"
 * 
 * Will be tracked by SharedListTracker.
 */
class SharedListServer {
public:
	SharedListServer (DataSharingServer * server);
	~SharedListServer();
	
	/// Initializes the SharedListServer (publishs first version of the list)
	Error init ();
	
	/// Uninitializes the SharedListServer
	Error uninit ();
	
	/// Adds an element to the shared list
	Error add (const String & shareName, const SharedElement & element);
	
	/// Removes a shared element
	Error remove (const String & shareName);
	
	/// Clears the shared list
	Error clear ();
	
	/// Returns the current shared list
	SharedList list () const;
	
	/// Returns associated path (always the same in the moment)
	Path path () const;

private:
	/// Updates the shared list
	Error update ();
	
	bool mInitialized;
	DataSharingServer * mServer;
	SharedList mList;
};

}
