#pragma once
#include <schnee/sftypes.h>
#include <vector>
#include <map>
#include <iostream>

namespace sf {
namespace test {

typedef int32_t ConId;		///< Id for connections (valid range 0..2^31)
typedef int32_t DiConId;	///< Directed Connection Id (negative: connection is from A to B, -1 * DiConId - 1)

/// One single connection
struct Connection {
	Connection () : delay (0), bwA (0), bwB (0) {}
	Connection (const sf::String & _a, const sf::String & _b, float _delay = 0, float _bwA = 0, float _bwB = 0) 
	: a(_a), 
	  b(_b), 
	  delay(_delay), 
	  bwA (_bwA), 
	  bwB (_bwB) {}
	
	sf::String a;				///< First host, "" is invalid
	sf::String b;				///< Second host, "" is invalid
	float delay;					///< Ping of this connection (in s)
	/// Bandwidth of this connection (a --> b) (in bit/s), 0 is unknown (use something very small for unidirectional links)
	float bwA;		
	/// Bandwidth of this connection (b --> a) (in bit/s), 0 is unknown (use something very small for unidirectional links)
	float bwB;
	
	/// Reorder values, so that a < b
	void normalize (){
		if (a > b){
			std::swap (a,b);
			std::swap (bwA, bwB);
		}
		/*// Default values may have to be used during path calculation
		if (ping == 0.0001) ping = 0.0001;
		if (bwA == 0) bwA = 10000000000.0f;
		if (bwB == 0) bwB = 10000000000.0f;
		*/
	}
};

/// One single host
struct Host {
	/// Name of the host
	sf::String name;
	/// The connections the host has
	std::vector<DiConId> connections;
	/// If a host is a router (more than one connection, used in Dot-Export)
	bool isRouter () const { return connections.size() > 1; }
};

class Network;

/// One route from one host to another, remember the representation of Hop (with negative numbers!)
typedef std::deque<DiConId> Route;

/** Network representing class.
 *  Note: 
 *  - Connections are bidirectional in the moment
 *  - We need an notify mechanism
 *  - Network locks itself only on write operations (if you want to read it locked, lock it for yourself)
 */
class Network {
public:
	Network () : mNextConnectionId (0) {}
	
	/// Adds one connection and adds the hosts automatically if they are non-existant
	/// returns true on success (may fail if connection already exists)
	bool addConnection (Connection c);
	
	/// Export a graphviz representation of the network
	/// @param stream - will write data to that stream
	/// @param additional - put in information about bandwidth etc
	/// @param close  - if true, then the output structure will be closed (use false in order to add own information)
	void exportGraphViz (std::ostream & stream, bool withAdditional = true, bool close = true) const;
	
	/// Prints out a route (debug function)
	sf::String routeString (const Route & r) const;
	
	/// Find a route between to hosts, tries to find the route with less hops as possible
	/// @param start start host from which we search
	/// @param end   end host from which we search 
	/// @param route if one was found (will be cleared automatically if there are any values)
	/// @return true if a route was found
	bool findRoute (const sf::String & start, const sf::String & end, Route & route) const;
	
	/// Returns information about a host
	/// @return the host or 0 if host was not found
	const Host * host (const sf::String & name) const;
	
	/// Returns information about a connection
	/// @param DiConId the connection id
	/// @param reverse, put a pointer in it if you want to know wether connection is reversed
	/// @return the connection (if found) or 0 if not found.
	const Connection * connection (DiConId, bool * reverse = 0) const;

	/// Gives delay of a route (summized delay of each connection)
	/// Returns < 0 in case of error
	float routeDelay (const Route & r) const;
private:
	/// Returns information about a connection (locked version)
	/// @param DiConId the connection id
	/// @param reverse, put a pointer in it if you want to know wether connection is reversed
	/// @return the connection (if found) or 0 if not found.
	const Connection * connection_locked (DiConId, bool * reverse = 0) const;


	/// Finds a route between to host using Dikstra
	bool dijkstraRouteFinder_locked (const Host & a, const Host & b, Route & route) const;

	/// Finds a host and creates if it does not exists and name was given. Either name or id have to be set, otherwise it returns 0
	/// Cannot create hosts with pure id
	Host & findCreateHost_locked (const sf::String & name);
	/// Returns a new connection id
	ConId nextConnectionId_locked () { return ++mNextConnectionId; }

	/// Returns the connection for a specific DiConId, reverse is true if the connection is used reversed
	/// This variant of connection () does not check the result, use it internally
	const Connection & connectionUnchecked_locked (DiConId id, bool & reverse) const {
		reverse = id < 0;
		if (id < 0) id = -id - 1;
		return mConnections.find (id)->second;
	}
	
	typedef std::map<ConId, Connection> ConnectionMap;	///< Mapping id to connections (so we can delete them)
	ConnectionMap mConnections;						///< All our connections
	typedef std::map<sf::String, Host> HostMap;		///< Type of our hostmap (name to Host)
	HostMap mHosts;									///< A list of our hosts
	ConId mNextConnectionId;							///< Our next connection id
	mutable Mutex mMutex;

	typedef std::pair<const Host*, const Host*> HostPair;
	typedef std::map<HostPair, Route> RouteCache;
	mutable RouteCache mRouteCache;
};


}
}
