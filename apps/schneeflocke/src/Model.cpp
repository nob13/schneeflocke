#include "Model.h"
#include <boost/foreach.hpp>

#include <schnee/im/xmpp/XMPPRegistration.h>
#include <schnee/settings.h>

Model::Model () {
	mBeacon      = 0;
	mSharingClient = 0;
	mSharingServer = 0;
	mSharedListServer = 0;
	mSharedListTracker = 0;
	mFileSharing = 0;
	mFileGetting = 0;
	mRegistration = 0;
	SF_REGISTER_ME;
}

Model::~Model () {
	SF_UNREGISTER_ME;
	delete mFileSharing;
	delete mFileGetting;
	delete mSharedListServer;
	delete mSharedListTracker;
	delete mBeacon; // will delete mSharing{server/client}
	delete mRegistration;
}

sf::Error Model::init () {
	mBeacon  = sf::InterplexBeacon::createIMInterplexBeacon();
	mSharingServer = sf::DataSharingServer::create ();
	mSharingClient = sf::DataSharingClient::create ();
	if (!mBeacon || !mSharingClient || !mSharingServer)
		return sf::error::NotInitialized;

	mBeacon->components().addComponent(mSharingClient);
	mBeacon->components().addComponent(mSharingServer);
	mSharingServer->checkPermissions() = dMemFun (this, &Model::onCheckPermissions);

	mSharedListServer = new sf::SharedListServer (mSharingServer);
	mSharedListTracker = new sf::SharedListTracker (mSharingClient);
	
	mFileSharing = new sf::FileSharing (mSharingServer, mSharedListServer);
	mFileGetting = new sf::FileGetting (mSharingClient);
	mFileGetting->setDestinationDirectory(mSettings.destinationDirectory);

	sf::Error e = mSharedListServer->init();
	if (e) return e;
	e = mSharedListTracker->init();
	if (e) return e;
	e = mFileSharing->init ();
	if (e) return e;
	e = mFileGetting->init();
	if (e) return e;
	return sf::NoError;
}

void Model::setSettings (const Settings & settings) {
	if (mFileGetting)
		mFileGetting->setDestinationDirectory(settings.destinationDirectory);
	mSettings = settings;
	sf::schnee::setEchoServer (mSettings.echoServerIp);
	sf::schnee::setEchoServerPort(mSettings.echoServerPort);
	sf::schnee::setForceBoshXmpp(mSettings.useBosh);
}

// enforces limitations on XMPP  resource name
static sf::String fixResourceName (const sf::String & s) {
	// max 63 characters, no empties, no '/'
	sf::String result;
	for (size_t i = 0; i < 64 && i < s.length(); i++) {
		char c = s[i];
		if (c != '/' && c != '.' && c != '_' && c != ' '&& c >= 32 && c < 128) result += c;
	}
	if (result.empty()) result = "SF";
	return result;
}

sf::Error Model::connect (const sf::ResultCallback & resultCallback) {
	sf::LockGuard guard (mMutex);
	sf::String connectionString;
	if (mSettings.useSlxmpp){
		connectionString = "slxmpp://" + mSettings.userId;
	} else
		connectionString = "xmpp://" + mSettings.userId + "/" + fixResourceName (mSettings.resource);
	sf::Error err = mBeacon->connect (connectionString, mSettings.password,  resultCallback);
	return err;
}

// defined in types.cpp
// TODO: Better place.
void splitUserServer (const sf::UserId & userId, sf::String * user, sf::String * server);


void Model::registerAccount (const sf::ResultCallback & callback) {
	sf::LockGuard guard (mMutex);
	if (mRegistration) {
		if (callback) xcall (sf::abind (callback, sf::error::TooMuch));
		return;
	}
	if (mSettings.useSlxmpp){
		if (callback) xcall (sf::abind (callback, sf::error::NotSupported));
		return;
	}
	mRegistration = new sf::XMPPRegistration ();
	sf::String username, servername;
	sf::XMPPRegistration::Credentials cred;
	sf::String server;
	splitUserServer (mSettings.userId, &cred.username, &server);
	cred.password = mSettings.password;
	// email is hopefully not needed...
	sf::ResultCallback acb = sf::abind (sf::dMemFun (this, &Model::onRegisterResult), callback);
	// TODO: Get rid of the return values of functions who call back anyway! #125
	sf::Error e = mRegistration->start(server, cred, 30000, acb);
	if (e) sf::xcall (sf::abind(acb, e));
}

sf::Error Model::disconnect () {
	sf::LockGuard guard (mMutex);
	mBeacon->disconnect ();
	return sf::NoError;
}

void Model::onRegisterResult (sf::Error e, const sf::ResultCallback & originalCallback) {
	{
		sf::LockGuard guard (mMutex);
		delete mRegistration;
		mRegistration = 0;
	}
	if (originalCallback) originalCallback (e);
}

bool Model::onCheckPermissions (const sf::HostId & host, const sf::Path & path){
	if (!mBeacon || !mSharedListServer || !mFileSharing) {
		return false; // ?
	} else {
		if (mSharedListServer->path() == path) return true;
		sf::UserId user = mBeacon->presences().hostInfo(host).userId;

		sf::String shareName = path.head ();
		sf::Error e = mFileSharing->checkPermissions(user, shareName);
		if (!e) return true;
		if (e == sf::error::NotFound) {
			sf::Log (LogInfo) << LOGID << "Was asked for permissions for not found element " << path << " (host=" << host << "), replying no" << std::endl;
		}
		return false;
	}
}


