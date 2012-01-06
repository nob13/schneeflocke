#include "NetworkDispatcher.h"

#include <schnee/tools/Log.h>

namespace sf {
namespace test {

typedef std::map<HostId, NetworkDispatcher*> DispatcherMap;

static DispatcherMap gDispatchers;

NetworkDispatcher::NetworkDispatcher (Network & network, LocalChannelUsageCollectorPtr usageCollector) : mNetwork (network) {
	SF_REGISTER_ME;
	mOnline = false;
	mUsageCollector = usageCollector;
}

NetworkDispatcher::~NetworkDispatcher () {
	SF_UNREGISTER_ME;
	disconnect();
}

Error NetworkDispatcher::setConnectionString (const String & connectionString, const String & password) {
	if (mOnline) {
		Log (LogWarning) << LOGID << "Setting connection string while being logged in leads to logout" << std::endl;
		disconnect ();
	}
	size_t pos = connectionString.find ("://");
	if (pos == connectionString.npos) mHostId = connectionString;
	else mHostId = connectionString.substr (pos + 3, connectionString.npos);
	return NoError;
}

sf::Error NetworkDispatcher::connect(const ResultDelegate & callback) {
	if (mOnline) disconnect ();
	const Host * h = mNetwork.host (mHostId);
	if (!h) {
		Log (LogError) << LOGID << "Did not found host " << mHostId << std::endl;
		return error::ConnectionError;
	}
	if (gDispatchers.find (mHostId) != gDispatchers.end()) {
		return error::ExistsAlready;
	}
	gDispatchers[mHostId] = this;
	mOnline = true;
	for (DispatcherMap::const_iterator i = gDispatchers.begin(); i != gDispatchers.end(); i++){
		xcall (dMemFun(i->second, &NetworkDispatcher::onPeersChanged));
	}
	if (callback) {
		xcall (abind(callback, sf::NoError));
	}
	return NoError;
}

sf::Error NetworkDispatcher::waitForConnected() {
	if (mOnline) return NoError;
	return error::InvalidArgument;
}

void NetworkDispatcher::disconnect() {
	gDispatchers.erase (mHostId);
	for (DispatcherMap::const_iterator i = gDispatchers.begin(); i != gDispatchers.end(); i++){
		xcall (dMemFun(i->second, &NetworkDispatcher::onPeersChanged));
	}
}

HostId NetworkDispatcher::hostId () const {
	return mHostId;
}

OnlineState NetworkDispatcher::onlineState () const {
	if (mOnline) return OS_ONLINE; else return OS_OFFLINE;
}

OnlineState NetworkDispatcher::onlineState (const sf::HostId & user) const {
	return gDispatchers.count(user) > 0 ? OS_ONLINE : OS_OFFLINE;
}


NetworkDispatcher::UserInfoMap NetworkDispatcher::users () const {
	HostInfoMap h = hosts();
	UserInfoMap result;
	for (HostInfoMap::const_iterator i = h.begin(); i != h.end(); i++) {
		UserInfo & info = result[i->first];
		info.userId = i->second.hostId; // are the same here
	}
	return result;
}

NetworkDispatcher::HostInfoMap NetworkDispatcher::hosts () const {
	HostInfoMap result;
	for (DispatcherMap::const_iterator i = gDispatchers.begin(); i != gDispatchers.end(); i++){
		const HostId & id = i->first;
		result[id].hostId = id;
		result[id].userId = id; // are the same here
	}
	return result;
}

NetworkDispatcher::HostInfoMap NetworkDispatcher::hosts(const UserId & user) const {
	HostInfoMap result;
	// user id is the same like host id here
	if (gDispatchers.count(user) > 0){
		HostInfo info;
		info.userId = user;
		info.hostId = user;
		result[info.hostId] = info;
	}
	return result;
}

NetworkDispatcher::HostInfo NetworkDispatcher::hostInfo (const HostId & host) const {
	DispatcherMap::const_iterator i = gDispatchers.find (host); // host and user are the same here
	if (i == gDispatchers.end()) return HostInfo ();
	HostInfo result;
	result.userId = host;
	result.hostId = host;
	return result;
}

sf::Error NetworkDispatcher::createChannel (const HostId & target, const ResultCallback & callback, int timeOutMs) {
	if (!mOnline) return error::ConnectionError;
	if (!mChannelCreated) return error::NotInitialized;

	const Host * src = mNetwork.host(mHostId);
	const Host * dst = mNetwork.host(target);
	Route route;
	bool suc = false;
	float delay = 0;
	bool neighbor = false;
	if (src && dst){
		suc = mNetwork.findRoute (mHostId, target, route);
		if (suc) {
			delay = mNetwork.routeDelay (route);
		}
	}
	if (!src || !dst) return error::CouldNotConnectHost;
	if (!suc) return error::CouldNotConnectHost;
	
	if (route.size() < 3) neighbor = true; // maximum one router

	NetworkDispatcher * dispatcher;
	{
		DispatcherMap::const_iterator i = gDispatchers.find (target);
		if (i == gDispatchers.end()) dispatcher = 0;
		else dispatcher = i->second;
	}
	if (!dispatcher) {
		return error::CouldNotConnectHost;
	}
	LocalChannel * lca = new LocalChannel (mHostId, mUsageCollector);
	LocalChannel * lcb = new LocalChannel (target, mUsageCollector);
	LocalChannel::bindChannels (*lca, *lcb); // here we should also insert delay/bw etc.
	if (delay > 0){
		lca->setDelay (delay);
		lcb->setDelay (delay);
	}
	if (neighbor){
		lca->setIsToNeighbor(true);
		lcb->setIsToNeighbor(true);
	}
	lca->setHops (route.size());
	lcb->setHops (route.size());
	lca->setAuthenticated(mNetwork.authenticated());
	lcb->setAuthenticated(mNetwork.authenticated());
	Log (LogInfo) << LOGID << "Created channel " << mHostId << " to " << target << " with delay " << delay << std::endl;
	dispatcher->pushChannel (mHostId, ChannelPtr (lcb));
	xcall (abind (mChannelCreated, target, ChannelPtr(lca), true));
	if (callback) xcall (abind (callback, NoError));
	return NoError;
}

void NetworkDispatcher::pushChannel (const HostId & source, ChannelPtr channel) {
	if (!mChannelCreated) {
		Log (LogError) << LOGID << "No Delegate" << std::endl;
		return;
	}
	xcall (abind (mChannelCreated, source, channel, false));
}

void NetworkDispatcher::onPeersChanged () {
	if (mPeersChanged) mPeersChanged ();
}


}
}
