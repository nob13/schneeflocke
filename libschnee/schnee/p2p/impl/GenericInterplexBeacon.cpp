#include "GenericInterplexBeacon.h"
#include <schnee/tools/Log.h>
namespace sf {

GenericInterplexBeacon::GenericInterplexBeacon () {
	SF_REGISTER_ME;
	mCommunicationMultiplex.setCommunicationDelegate(&mConnectionManagement);
	mConnectionManagement.init (&mCommunicationMultiplex, &mAuthentication);
}

GenericInterplexBeacon::~GenericInterplexBeacon (){
	SF_UNREGISTER_ME;
	mCommunicationMultiplex.setCommunicationDelegate(0);
	mConnectionManagement.uninit();
}

void GenericInterplexBeacon::setPresenceProvider (PresenceProviderPtr presenceProvider){
	assert (presenceProvider);
	if (!presenceProvider) return;
	mPresenceProvider = presenceProvider;
	mPresenceProvider->onlineStateChanged().add (dMemFun (this, &GenericInterplexBeacon::onOnlineStateChanged));
	mPresenceProvider->peersChanged().add (dMemFun (this, &GenericInterplexBeacon::onPeersChanged));
}

Error GenericInterplexBeacon::setConnectionString (const String & connectionString, const String & password) {
	if (!mPresenceProvider) return error::NotInitialized;
	mConnectionManagement.clear(); // hack
	Error e = mPresenceProvider->setConnectionString(connectionString, password);
	if (e) return e;
	// HostId is valid now
	mConnectionManagement.setHostId (mPresenceProvider->hostId());
	mAuthentication.setIdentity(mPresenceProvider->hostId());
	return NoError;
}

Error GenericInterplexBeacon::connect (const ResultCallback & callback) {
	return mPresenceProvider->connect (callback);
}

void GenericInterplexBeacon::disconnect () {
	if (mPresenceProvider){
		mPresenceProvider->disconnect ();
	}
	mHosts.clear();
	mConnectionManagement.clear();
}

PresenceManagement & GenericInterplexBeacon::presences () {
	assert (mPresenceProvider);
	return *mPresenceProvider;
}

const PresenceManagement & GenericInterplexBeacon::presences () const {
	assert (mPresenceProvider);
	return *mPresenceProvider;
}

CommunicationMultiplex & GenericInterplexBeacon::components () {
	return mCommunicationMultiplex;
}

GenericConnectionManagement & GenericInterplexBeacon::connections () {
	return mConnectionManagement;
}

const GenericConnectionManagement & GenericInterplexBeacon::connections() const {
	return mConnectionManagement;
}

void GenericInterplexBeacon::onOnlineStateChanged (OnlineState os) {
	HostId id;
	if (os == OS_ONLINE){
		id = mPresenceProvider->hostId ();
		mConnectionManagement.setHostId (id);
		mConnectionManagement.startPing();
	} else {
		mConnectionManagement.stopPing ();
	}
}

void GenericInterplexBeacon::onPeersChanged () {
	/*
	 * Checking who went offline/online
	 * Offliners : kill connections to them (TODO: is this really necessary?)
	 * Onliners:   request for additional information
	 */
	PresenceManagement::HostInfoMap hosts = mPresenceProvider->hosts();
	HostSet wentOffline;
	for (PresenceManagement::HostInfoMap::const_iterator i = mHosts.begin(); i != mHosts.end(); i++) {
		if (hosts.count(i->first) == 0){
			wentOffline.insert (i->first);
			mConnectionManagement.clear(i->first);
		}
	}
	for (PresenceManagement::HostInfoMap::const_iterator i = hosts.begin(); i != hosts.end(); i++) {
		if (mHosts.count(i->first) == 0){
			// went online...
			mPresenceProvider->updateFeatures(i->first);
		}
	}
	mHosts.swap(hosts);
	for (HostSet::const_iterator i = wentOffline.begin(); i != wentOffline.end(); i++) {
		mCommunicationMultiplex.distChannelChange(*i);
	}
}

}
