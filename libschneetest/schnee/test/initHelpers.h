#pragma once
#include <schnee/schnee.h>
#include <schnee/p2p/InterplexBeacon.h>
#include "Network.h"
#include "LocalChannel.h"

#include <schnee/p2p/DataSharingServer.h>
#include <schnee/p2p/DataSharingClient.h>
///@cond DEV

/**
 * @file Various helper functions for testcases which simplify initialization
 */

namespace sf {
namespace test {

/**
 * Wait until a specific function returns true.
 * @param maxSeconds wait up to maxSeconds
 * @return the function went to true within time
 */
bool waitUntilTrue (const sf::function<bool ()>& testFunction, int maxSeconds);

/// Wait until a specific function returns true within a milliseconds range (does test each 10ms)
bool waitUntilTrueMs (const sf::function<bool ()>& testFunction, int maxMilliSeconds);


/// Checks whether there is still pending data in the LocalChannels
/// Note: 1. this has to be called from the main thread.
///       2. It will block the worker thread, assuming only the worker thread can change it
///       3. It only works with LocalChannels
bool noPendingData (sf::test::LocalChannelUsageCollectorPtr collector);

inline bool waitForNoPendingDataMs (sf::test::LocalChannelUsageCollectorPtr collector, int maxMs){
	return waitUntilTrueMs (sf::bind (noPendingData, collector), maxMs);
}

/// Returns test names (like Alice, Bob etc.
/// If id is greater than number of names, 0 is returned
const char * testNames (int id);

/// Returns the name of a server used in a testcase
inline const char * serverName (int id = 0) { return id==0?"Server" : 0; }

/// Typical inner network delay ~3..8ms
float smallRandDelay ();

/// Typical good net connection delay (5..15ms), e.g. server
float mediumRandDelay ();

// Typical good workgroup internet delay (30..60ms)
float workGroupRandDelay ();


/// Generates a star-network where all nodes are connected to one router. The names will be the same like in testNames(i).
/// @param network - the network in which the nodes/connections will be created
/// @param nodeCount - number of nodes
/// @param withServer - also create a server inside the nodes
/// @param randomDelay - creates some pseudo random delay between 10ms and 100ms into the connections
void genStarNetwork (test::Network & network, int nodeCount, bool withServer = false, bool randomDelay = false);

/// Generates a double star network, wheree the nodes are alternating on two routes. the server will be on a third router
void genDoubleStarNetwork (test::Network & network, int nodeCount, bool withServer = false, bool randomDelay = false);

}
}

///@endcond DEV
