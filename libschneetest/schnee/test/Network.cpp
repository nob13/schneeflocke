#include "Network.h"
#include <schnee/tools/Log.h>
#include <set>
#include <assert.h>

namespace sf {
namespace test {

bool Network::addConnection(Connection c){
	LockGuard guard (mMutex);
	if (c.a.empty() || c.b.empty()){
		sf::Log (LogError) << LOGID << "Connection may not have empty names" << std::endl;
		return false;
	}
	c.normalize ();
	
	if (c.delay < 0){
		sf::Log (LogError) << LOGID << "Delay may not be < 0" << std::endl;
		return false;
	}
	if (c.bwA < 0 || c.bwB < 0){
		sf::Log (LogError) << LOGID << "Bandwidth may not be < 0" << std::endl;
		return false;
	}
	
	Host & a = findCreateHost_locked (c.a);
	Host & b = findCreateHost_locked (c.b);
	
	for (std::vector<DiConId>::const_iterator i = a.connections.begin(); i != a.connections.end(); i++){
		bool reverse;
		const Connection & con = connectionUnchecked_locked (*i, reverse);
		if ( (!reverse && (con.b == c.b)) || (reverse && (con.a == c.b))){
			sf::Log (LogError) << LOGID << "Detected an duplicate connection (" << c.a << " -- " << c.b << "), won't add a second one" << std::endl;
			/*
			 * Hosts created before may survice, but this shall not be a problem since they are not connected
			 */
			return false;
		}
	}
	ConId conId = nextConnectionId_locked ();
	mConnections[conId] = c;
	a.connections.push_back (conId);
	DiConId reverse = - conId - 1;
	// a is smaller than b; so b gets the negative connection value
	b.connections.push_back (reverse);
	mRouteCache.clear();
	return true;
}

void formatBandwidth (float b, std::ostream & stream){
	if (b == 0) return; // no bandwidth marker
	if (b > 1000000000.0f) { stream << (b / 1000000000.0f) << "GBit"; return;}
	if (b > 1000000.0f) { stream << (b / 1000000.0f) << "MBit"; return; }
	if (b > 1000.0f) { stream << (b / 1000.0f) << "Kbit"; return; }
	stream << b << "bit";
}

void formatDelay (float p, std::ostream & stream){
	if (p == 0) return; // no ping marker
	if (p < 0.001f) { stream << (p * 1000000.0f) << "µs"; return; }
	if (p < 1.000f) { stream << (p * 1000.0f) << "ms"; return; }
	stream << p << "s";
}

void Network::exportGraphViz (std::ostream & stream, bool withAdditional, bool close) const {
	// TODO: Add subgraph support (so that clusters will be shown better!)
	stream << "graph Network { " << std::endl;
	stream << "    graph [overlap=scale];" << std::endl;
	for (ConnectionMap::const_iterator i = mConnections.begin(); i != mConnections.end(); i++){
		const Connection & c = i->second;
		stream << "    " << c.a << " -- " << c.b;
		stream << " [";
		if (withAdditional && (c.delay != 0 || c.bwA != 0 || c.bwB != 0)){
			stream << "fontsize=8 /*, decorative=true*/, ";
			if (c.delay!=0) { 
				stream << "label=\"";
				formatDelay (c.delay, stream);
				if (c.bwA > 0 && c.bwA == c.bwB) {
					// if bandwidth is equal put them into the label
					stream << ",";
					formatBandwidth (c.bwA, stream);
				}
				stream << "\", ";
			}
			if (c.bwA != c.bwB){
				// put the bandwidth onto the edges if they are different
				if (c.bwA > 0){
					stream << "headlabel=\"";
					formatBandwidth (c.bwA, stream);
					stream << "\", ";
				}
				if (c.bwB > 0){
					stream << "taillabel=\"";
					formatBandwidth (c.bwB, stream);
					stream << "\", ";
				}
			}
		}
		const Host & ha = *host (c.a);
		const Host & hb = *host (c.b);
		if (ha.isRouter() && hb.isRouter()){
			stream << "len=2.0 ";
		}
		stream << "]";
		stream << ";" << std::endl;
	}
	stream << "    /* Additional node information */" << std::endl;
	for (HostMap::const_iterator i = mHosts.begin(); i != mHosts.end(); i++){
		const Host & h = i->second;
		if (h.isRouter()){
			stream << "    " << h.name << " [shape=box]; /* router */" << std::endl;
		}
	}
	if (close) stream << "}" << std::endl;
}

sf::String Network::routeString (const Route & r) const {
	LockGuard guard (mMutex);
	if (r.empty()) { return "[]"; }
	std::ostringstream stream;
	stream << "[";
	for (Route::const_iterator i = r.begin(); i != r.end(); i++){
		bool reverse;
		const Connection & c = connectionUnchecked_locked (*i, reverse);
		stream << (reverse ? c.b : c.a) << ",";
	}
	bool reverse;
	const Connection & c = connectionUnchecked_locked (r.back(), reverse);
	stream << (reverse ? c.a : c.b);
	stream << "]";
	return stream.str();
}

bool Network::findRoute (const sf::String & start, const sf::String & end, Route & route) const {
	LockGuard guard (mMutex);
	HostMap::const_iterator ai = mHosts.find (start);
	HostMap::const_iterator bi = mHosts.find (end);
	if (ai == mHosts.end() || bi == mHosts.end()) {
		sf::Log (LogError) << LOGID << "Non valid start or endpoint (gave " << start << " and " << end << ")" << std::endl;
		return false;
	}
	if (ai == bi){
		sf::Log (LogWarning) << LOGID << "Start end endpoint are equal" << std::endl;
		return false;
	}
	route.clear();
	return dijkstraRouteFinder_locked (ai->second, bi->second, route);
}

const Host * Network::host (const sf::String & name) const {
	HostMap::const_iterator i = mHosts.find (name);
	if (i == mHosts.end()) return 0;
	return & (i->second);
}

const Connection * Network::connection (DiConId id, bool * reverse) const {
	LockGuard guard (mMutex);
	return connection_locked (id, reverse);
}

float Network::routeDelay (const Route & r) const {
	LockGuard guard (mMutex);
	float sum = 0.0f;
	for (Route::const_iterator i = r.begin(); i != r.end(); i++){
		const Connection * c = connection_locked (*i);
		if (!c) {
			sf::Log (LogError) << LOGID << "Invalid route" << std::endl;
			return -1.0f;
		}
		sum+=c->delay;
	}
	return sum;
}

const Connection * Network::connection_locked (DiConId id, bool * reverse) const {
	bool r = id < 0;
	if (r) id = - id - 1;
	if (reverse) *reverse = r;
	ConnectionMap::const_iterator i = mConnections.find (id);
	if (i == mConnections.end()) return 0;
	return & (i->second);
}


// Route finding algorithm (Dijkstra)
struct Node {
	Node (const Host * _h = 0, float _distance = 0, size_t _before = 0, DiConId _con = 0) : h (_h), distance (_distance), before (_before), con(_con) {}
	const Host * h;
	float distance;
	size_t before;  // id of last node in 'rest' vector
	DiConId con;	// connection which to this node 
	bool operator< (const Node & other) const {
		return (distance < other.distance) || ((distance == other.distance) && (h->name < other.h->name));
	}
};

bool Network::dijkstraRouteFinder_locked (const Host & a, const Host & b, Route & route) const {
	const Host * start  = &a;
	const Host * target = &b;

	{
		// We have some caching, because route finding took > 40% in the degeneration test.
		RouteCache::const_iterator i = mRouteCache.find (HostPair(start, target));
		if (i != mRouteCache.end()) { route = i->second; return true; }
	}

	std::set<Node> waiting;
	std::set<const Host*> marked;
	std::vector<Node> rest;
	waiting.insert (Node (start, 0, 0));
	marked.insert (start);
	while (!waiting.empty()){
		
		Node first = *waiting.begin ();
		// sf::Log (LogInfo) << LOGID << "Checking " << first.h->name << " (waiting = " << sf::toString (waiting) << ")"<< std::endl;
		if (first.h == target){
			// Found shortest way, reconstructing it
			for (Node * current = &first; current != &rest[0]; current = &rest[current->before]){
				route.push_front (current->con);
			}
			mRouteCache[HostPair(start,target)] = route;
			return true;
		}
		waiting.erase (waiting.begin());
		rest.push_back (first);
		const Host * h = first.h;
		marked.insert (h);
		size_t before = rest.size() - 1;
		for (std::vector<DiConId>::const_iterator i = h->connections.begin(); i != h->connections.end(); i++){
			bool reverse;
			const Connection & c = connectionUnchecked_locked (*i, reverse);
			const Host & nh = reverse ? mHosts.find (c.a)->second : mHosts.find(c.b)->second;
			if (marked.find (&nh) != marked.end()) continue;
			Node n (&nh, first.distance + c.delay, before, *i);
			// sf::Log (LogInfo) << LOGID << "Inserting " << n.h->name << std::endl;
			waiting.insert (n);
		}
	}
	// Found no way
	return false;
}


Host & Network::findCreateHost_locked (const sf::String & name){
#ifndef NDEBUG
	assert (mMutex.try_lock() == false);
#endif
	HostMap::iterator i = mHosts.find (name);
	if (i == mHosts.end()){
		Host h;
		h.name = name;
		mHosts[name] = h;
		return mHosts[name];
	}
	return i->second;
}

}
}

