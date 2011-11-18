#pragma once
#include <schnee/schnee.h>
#include <schnee/p2p/InterplexBeacon.h>
#include <schnee/tools/async/DelegateBase.h>
#include <flocke/filesharing/FileSharing.h>
#include <flocke/filesharing/FileGetting.h>
#include <flocke/sharedlists/SharedListServer.h>
#include <flocke/sharedlists/SharedListTracker.h>

namespace sf {
class XMPPRegistration;
}

/** Encapsulates libschnee and integral program components
 * */
class Model : public sf::DelegateBase {
public:

	Model ();
	~Model ();

	/// Initializes the core
	sf::Error init ();

	struct Settings {
		Settings() : echoServerPort (1234), useSlxmpp(false), useBosh (false), autoConnect(false) {}
		sf::UserId userId;
		sf::String password;
		sf::String resource;
		sf::String destinationDirectory;
		sf::String echoServerIp;
		int echoServerPort;
		bool       useSlxmpp;
		bool       useBosh;
		bool       autoConnect;
	};

	/// Current settings
	const Settings & settings () const { return mSettings; }
	/// Set settings
	void setSettings (const Settings & settings);

	/// Start connecting process
	sf::Error connect (const sf::ResultCallback & resultCallback);

	/// Start registering process
	void registerAccount (const sf::ResultCallback & resultCallback);

	/// Start disconnecting process
	sf::Error disconnect ();

	/// Gives access to the file sharing component
	sf::FileSharing * fileSharing () { return mFileSharing; }
	const sf::FileSharing * fileSharing () const { return mFileSharing; }

	/// Gives access to the file getting component
	sf::FileGetting * fileGetting () { return mFileGetting; }
	const sf::FileGetting * fileGetting () const { return mFileGetting;}
	
	/// Gives access to the shared list tracker
	sf::SharedListTracker * listTracker () { return mSharedListTracker; }
	
	// /// Gives access to the beacon
	sf::InterplexBeacon * beacon () { return mBeacon; }
	/// Gives const access to the beacon
	const sf::InterplexBeacon * beacon () const { return mBeacon; }

private:
	void onRegisterResult (sf::Error e, const sf::ResultCallback & originalCallback);
	bool onCheckPermissions (const sf::HostId & user, const sf::Path & path);


	// Core parts
	sf::InterplexBeacon * mBeacon;
	sf::DataSharingServer * mSharingServer;
	sf::DataSharingClient * mSharingClient;

	// App Components
	sf::FileSharing * mFileSharing;
	sf::FileGetting * mFileGetting;
	sf::SharedListServer  * mSharedListServer;
	sf::SharedListTracker * mSharedListTracker;

	Settings   mSettings;
	mutable sf::Mutex  mMutex;

	sf::XMPPRegistration * mRegistration; ///< A registration process (if there is any)
};
