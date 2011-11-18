#include "Controller.h"
#include <QSettings>
#include <QCoreApplication>
#include "types.h"
#include "gui/GUI.h"
#include <schnee/net/Tools.h>
#include "autostart.h"
#include "modeladapters/UserList.h"
#include "modeladapters/TransferList.h"
#include "modeladapters/ShareToUserList.h"
#include "modeladapters/ConnectionList.h"
#include "modeladapters/ShareList.h"

Controller::Controller (){
	SF_REGISTER_ME;
	mModel = new Model ();
	mUserList = new UserList (this);
	mIncomingTransferList = new TransferList (false);
	mOutgoingTransferList = new TransferList (true);
	mShareToUserList = new ShareToUserList;
	mConnectionList = new ConnectionList (this);
	mShareList = new ShareList (this);
	mGUI   = new GUI (this);
	mState   = OFFLINE;
}

Controller::~Controller () {
	SF_UNREGISTER_ME;
	delete mGUI;
	delete mUserList;
	delete mIncomingTransferList;
	delete mConnectionList;
	delete mShareList;
	delete mModel;
}

void Controller::init () {
	QSettings settings;
	Model::Settings s;
	settings.beginGroup("im");
	s.userId   = sfString (settings.value("userId","@sflx.net").toString());
	s.resource = sfString (settings.value("resource", qtString (sf::net::hostName())).toString());
	s.password 				= sfString (settings.value("password").toString());
	s.destinationDirectory =  sfString (settings.value("destinationDirectory", QDir::homePath()).toString());
	s.echoServerIp          = sfString (settings.value("echoServerIp", "82.211.19.149").toString());
	if (s.echoServerIp == "62.48.92.13"){
		// sflx.net moved!
		// workaround
		s.echoServerIp = "82.211.19.149";
	}
	s.echoServerPort        = settings.value("echoServerPort", "1234").toInt ();
	s.useSlxmpp             = settings.value("useSlxmpp", false).toBool();
	s.useBosh               = settings.value("useBosh", false).toBool();
	s.autoConnect           = settings.value("autoConnect", false).toBool();
	settings.endGroup ();
	mModel->setSettings(s);


	mModel->init ();
	mModel->beacon()->connections().conDetailsChanged() = dMemFun (this, &Controller::onConDetailsChanged);
	mModel->listTracker()->trackingUpdate()  = dMemFun (this, &Controller::onTrackingUpdate);
	mModel->listTracker()->lostTracking()    = dMemFun (this, &Controller::onLostTracking);
	mModel->fileGetting()->gotListing()      = dMemFun (this, &Controller::onUpdatedListing);
	mModel->fileGetting()->updatedTransfer() = dMemFun (this, &Controller::onUpdatedIncomingTransfer);
	mModel->fileSharing()->updatedTransfer() = dMemFun (this, &Controller::onUpdatedOutgoingTransfer);

	sf::InterplexBeacon * beacon = mModel->beacon();

	beacon->presences().peersChanged().add (sf::dMemFun (this, &Controller::onPeersChanged));
	beacon->presences().onlineStateChanged().add (sf::dMemFun (this, &Controller::onOnlineStateChanged));
	beacon->presences().subscribeRequest() = sf::dMemFun (this, &Controller::onUserSubscribeRequest);
	beacon->presences().serverStreamErrorReceived().add (sf::dMemFun (this, &Controller::onServerStreamErrorReceived));

	// Set own features and activating protocol filter
	{
		const sf::String schneeProtocol = "http://sflx.net/protocols/schnee";
		std::vector<sf::String> ownFeatures;
		ownFeatures.push_back (schneeProtocol);
		sf::Error e = beacon->presences().setOwnFeature(sf::String ("schneeflocke ") + sf::schnee::version(), ownFeatures);
		if (e) {
			sf::Log (LogError) << LOGID << "Could not set own feature" << std::endl;
		} else {
			mUserList->setFeatureFilter(schneeProtocol);
		}
	}

	mGUI->init ();
	mGUI->call (sf::bind (&UserList::rebuildList, mUserList));
	loadShares ();

	if (s.autoConnect){
		xcall (abind (dMemFun (this, &Controller::changeConnection), DS_CONNECT));
	}
}

void Controller::quit () {
	// we go down when QApplication goes down.
	QApplication::quit();
}

void Controller::updateSettings(const Model::Settings & s) {
	mModel->setSettings (s);
	QSettings settings;
	settings.beginGroup("im");
	settings.setValue("userId", qtString (s.userId));
	settings.setValue("password", qtString (s.password));
	settings.setValue("destinationDirectory", qtString (s.destinationDirectory));
	settings.setValue("echoServerIp", qtString (s.echoServerIp));
	settings.setValue("echoServerPort", QVariant (s.echoServerPort));
	settings.setValue ("useSlxmpp", s.useSlxmpp);
	settings.setValue ("useBosh", s.useBosh);
	settings.setValue ("autoConnect", s.autoConnect);
	settings.endGroup ();
}

void Controller::share (const QString & shareName, const QString & fileName, bool forAll, const sf::UserSet & whom){
	sf::FileSharing * sharing = mModel->fileSharing ();
	sf::Error e = sharing->share (sfString (shareName), sfString (fileName), forAll, whom);
	if (e) {
		mGUI->onError (err::CouldNotAddShare, QObject::tr ("Could not add share"), QObject::tr ("Could not add share: %1").arg (toString (e)));
	}
	mShareList->update();
	saveShares();
}

void Controller::editShare (const QString & shareName, const QString & fileName, bool forAll, const sf::UserSet & whom) {
	sf::FileSharing * sharing = mModel->fileSharing ();
	sf::Error e = sharing->editShare (sfString (shareName), sfString (fileName), forAll, whom);
	if (e) {
		mGUI->onError (err::CouldNotEditShare, QObject::tr ("Could not add share"), QObject::tr ("Could not edit share %1, reason: %2")
		.arg (shareName).arg(toString(e)));
	}
	mShareList->update();
	saveShares();
}

void Controller::unshare (const QString & shareName) {
	sf::FileSharing * sharing = mModel->fileSharing();
	sf::Error e = sharing->unshare(sfString(shareName));
	if (e) {
		mGUI->onError(err::CouldNotRemoveShare, QObject::tr ("Could not remove share"),
				QObject::tr ("Could not remove share %1, reason:%2").arg(shareName).arg(toString (e)));
	}
	mShareList->update();
	saveShares();
}

void Controller::registerAccount () {
	mModel->registerAccount (sf::dMemFun (this, &Controller::onRegisterAccount));
}

bool Controller::isConnectedTo (const sf::HostId & hostId) {
	return mModel->beacon()->connections().channelLevel(hostId) >= 10;
}

sf::Error Controller::trackHostShared (const sf::HostId & hostId) {
	if (!isConnectedTo (hostId)){
		return mModel->beacon()->connections().liftToAtLeast (10, hostId, sf::abind (sf::dMemFun (this, &Controller::onConnectHostForTrackingResult), hostId));
	}
	// forward as if we woudl have it connected
	onConnectHostForTrackingResult (sf::NoError, hostId);
	return sf::NoError;
}

void Controller::listDirectory (const sf::Uri & uri) {
	sf::FileGetting * getting = mModel->fileGetting();
	sf::Error e = getting->listDirectory(uri);
	if (e) {
		mGUI->onError (err::CouldNotListDirectory, QObject::tr ("Could not list directory"), QObject::tr ("Could not list directory %1 of %2, reason: %3")
		.arg(qtString (uri.host())).arg(qtString (uri.path().toString())).arg (toString (e)));
	}
}

void Controller::cancelIncomingTransfer (sf::AsyncOpId id) {
	mModel->fileGetting()->cancelTransfer(id);
}

void Controller::clearIncomingTransfers () {
	mModel->fileGetting()->removeFinishedTransfers();
}

void Controller::cancelOutgoingTransfer (sf::AsyncOpId id) {
	mModel->fileSharing()->cancelTransfer(id);
}

void Controller::clearOutgoingTransfers () {
	mModel->fileSharing()->removeFinishedTransfers();
}

void Controller::requestFile (const sf::Uri & uri) {
	sf::FileGetting * getting = mModel->fileGetting();
	sf::Error e = getting->request(uri);
	if (e) {
		mGUI->onError(err::CouldNotRequestFile, QObject::tr ("Could not request file"), QObject::tr ("Could not request file %1 of %2, reason: %3")
		.arg (qtString(uri.path().toString())).arg(qtString (uri.host())).arg(toString(e)));
	}
}

void Controller::requestDirectory (const sf::Uri & uri) {
	sf::FileGetting * getting = mModel->fileGetting();
	sf::Error e = getting->requestDirectory(uri);
	if (e) {
		mGUI->onError (err::CouldNotRequestDirectory, QObject::tr ("Could not request directory"), QObject::tr ("Could not request director %1 of %2, reason: %3")
		.arg (qtString(uri.host())).arg (qtString(uri.path().toString())).arg(toString(e)));
	}
}

void Controller::addUser (const sf::UserId & userId) {
	sf::Error e = mModel->beacon()->presences().subscribeContact (userId);
	if (e) {
		mGUI->onError(err::CouldNotAddUser, QObject::tr ("Could not add user"), QObject::tr ("Could not add user %1, reason: %2").
				arg (qtString (userId)).arg (toString (e)));
	}
}

void Controller::subscriptionRequestReply (const sf::UserId & userId, bool allow, bool alsoAdd) {
	sf::Error e = mModel->beacon()->presences().subscriptionRequestReply(userId,allow,alsoAdd);
	if (e) {
		mGUI->onError(err::CouldNotSubscribeReplyUser, QObject::tr ("Subscription error"), QObject::tr("Could not allow other user %1 to subscribe, reason: %2")
		.arg(qtString(userId)).arg (toString(e)));
	}
}

void Controller::removeUser (const sf::UserId & userId) {
	sf::Error e = mModel->beacon()->presences().removeContact (userId);
	if (e) {
		mGUI->onError(err::CouldNotRemoveUser, QObject::tr("User Removal"), QObject::tr ("Could not remove use %1, reason: %2").arg(qtString(userId)).arg(toString(e)));
	}
}

void Controller::changeConnection (DesiredState state) {
	{
		sf::LockGuard guard (mMutex);
		if (state == DS_CONNECT) {
			if (mState == CONNECTED)
				return; // already connected
			sf::Error e = mModel->connect(dMemFun (this, &Controller::onConnect));
			mState = CONNECTING;
			if (e) mGUI->onError (err::CouldNotConnectService, QObject::tr ("Could not connect to Service"),
					QObject::tr ("Could not connect to service, reason %1. Please check your connection settings.").arg(toString(e)));
		}
		if (state == DS_DISCONNECT) {
			/*sf::Error err =*/ mModel->disconnect();
			mState = OFFLINE;
		}
	}
	mGUI->call (sf::bind (&GUI::onStateChange, mGUI));
}

bool Controller::autostartAvailable () const {
	return autostart::isAvailable();
}

bool Controller::autostartInstalled () const {
	return autostart::isInstalled("schneeflocke");
}

bool Controller::setAutostartInstall (bool v) {
	if (v) {
		return autostart::install("schneeflocke", sfString (QCoreApplication::applicationFilePath()), "--autostart");
	} else {
		return autostart::uninstall ("schneeflocke");
	}
}

void Controller::onError        (err::Code code, const QString & title, const QString & what) {
	mGUI->call (sf::bind (&GUI::onError, mGUI, code, title, what));
}

sf::UserSet toUserSet (const QVariant & variant) {
	QStringList intermediate = variant.toStringList();
	sf::UserSet result;
	for (int i = 0; i < intermediate.size(); i++){
		result.insert(sfString(intermediate[i]));
	}
	return result;
}

void Controller::loadShares () {
	QSettings settings;
	settings.beginGroup("share");
	int count = settings.beginReadArray("shares");
	for (int i = 0; i < count; i++) {
		settings.setArrayIndex(i);
		// do not read everything...
		sf::String shareName = sfString (settings.value("shareName").toString());
		sf::String fileName  = sfString (settings.value("fileName").toString());
		bool forAll = settings.value("forAll").toBool();
		sf::UserSet whom = toUserSet (settings.value("whom"));
		sf::Error e = mModel->fileSharing()->editShare(shareName, fileName, forAll, whom);
		if (e) {
			onError (err::CouldNotRestoreShare,
					QObject::tr ("Could not restore share"),
					QObject::tr ("Could not restore share %1, reason: %2")
					.arg (qtString(shareName)).arg (toString(e)));
		}
	}
	settings.endArray();
	settings.endGroup();
	mShareList->update();
}

QVariant toQVariant (const sf::UserSet & userlist) {
	QStringList intermediate;
	for (sf::UserSet::const_iterator i = userlist.begin(); i != userlist.end(); i++) {
		intermediate.append(qtString(*i));
	}
	return QVariant (intermediate);
}

void Controller::saveShares () {
	sf::FileSharing::FileShareInfoVec shares = mModel->fileSharing()->shared();
	QSettings settings;
	settings.beginGroup("share");
	settings.beginWriteArray("shares", shares.size());
	for (size_t i = 0; i < shares.size(); i++) {
		settings.setArrayIndex((int)i);
		const sf::FileSharing::FileShareInfo & info = shares[i];
		settings.setValue("shareName", qtString(info.shareName));
		settings.setValue("path", qtString (info.path.toString()));
		settings.setValue("fileName", qtString (info.fileName));
		settings.setValue("forAll", info.forAll);
		settings.setValue("whom", toQVariant(info.whom));
	}
	settings.endArray();
	settings.endGroup();
}

void Controller::onConnect (sf::Error connectionError) {
	sf::LockGuard guard (mMutex);
	if (connectionError) {
		mState = ERROR;
		onError (err::CouldNotConnectService, QObject::tr ("Could not connect to Service"),
				QObject::tr ("Could not connect to service, reason <b>%1</b>. Please check your connection settings.").arg(toString(connectionError)));
		mGUI->call (sf::bind (&GUI::onStateChange, mGUI));
	}
}

void Controller::onPeersChanged () {
	// Bad performance: mGUI fetches User list, ShareToUserList too
	mGUI->call (sf::bind (&UserList::onPeersChanged, mUserList));
	mGUI->call (sf::bind (&ShareToUserList::update, mShareToUserList,mModel->beacon()->presences().users()));
}

void Controller::onOnlineStateChanged (sf::OnlineState os) {
	sf::LockGuard guard (mMutex);
	sf::Log (LogProfile) << LOGID << "Controller::onOnlineStateChanged: " << toString (os) << std::endl;
	State before = mState;
	switch (os){
	case sf::OS_ONLINE:
		mState = CONNECTED;
	break;
	case sf::OS_OFFLINE:
		mState = OFFLINE;
	break;
	default:
		// ignore
	break;
	}
	// Note: this can lead to a loop if onStateChange changes again current connection status
	if (mState != before){
		mGUI->call (sf::bind (&GUI::onStateChange, mGUI));
		if (mState != CONNECTING){
			mGUI->call (sf::bind (&UserList::rebuildList, mUserList));
		}
	}
}

void Controller::onServerStreamErrorReceived (const sf::String & text) {
	onError (err::ServerSentError, QObject::tr ("Server stream error"), QObject::tr ("Received a server stream error:<br/> <b>%1</b>").arg(qtString(text)));
}

void Controller::onUserSubscribeRequest (const sf::UserId & userId) {
	mGUI->call (sf::bind (&GUI::onUserSubscribeRequest, mGUI, userId));
}

void Controller::onConDetailsChanged () {
	mGUI->call (sf::bind (&ConnectionList::update, mConnectionList));
}

void Controller::onConnectHostForTrackingResult (sf::Error result, const sf::HostId & hostId) {
	if (result) {
		mGUI->call (sf::bind (&UserList::onLostTracking, mUserList, hostId));
		onError (err::CouldNotConnectRemoteHost, QObject::tr ("Could not connect remote host"), QObject::tr ("Could not connect remote host %1").arg (qtString(hostId)));
		return;
	}
	sf::SharedListTracker * tracker = mModel->listTracker();
	sf::Error e = tracker->trackShared (hostId);
	if (e) {
		mGUI->onError (err::CouldNotTrackShared, QObject::tr ("Could not track"), QObject::tr ("Could not track files of %1, reason: %2").arg(qtString(hostId)).arg(toString (e)));
	}
}

void Controller::onRegisterAccount (sf::Error e) {
	switch (e) {
	case sf::error::TooMuch:
		onError (err::RegistrationError, QObject::tr ("Registration Error"), QObject::tr ("There is already a registration in process"));
		break;
	case sf::error::NotSupported:
		onError (err::RegistrationError, QObject::tr ("Registration Error"), QObject::tr ("Registration is not supported on that server"));
		break;
	case sf::NoError:
		mGUI->call (sf::bind (&GUI::onSuccess, mGUI, QObject::tr ("Registration"), QObject::tr ("Registration was successfull!")));
		break;
	default:
		onError (err::RegistrationError, QObject::tr ("Registration Error"), QObject::tr ("Registration failed: <b>%1</b>").arg(toString(e)));
		break;
	}
}


void Controller::onLostTracking (const sf::HostId & host, const sf::Error cause) {
	mGUI->call (sf::bind (&UserList::onLostTracking, mUserList, host));
	mGUI->call (sf::bind (&GUI::onLostTracking, mGUI, host, toString (cause)));
}

void Controller::onTrackingUpdate (const sf::HostId & host, const sf::SharedList & list) {
	mGUI->call (sf::bind (&UserList::onTrackingUpdate, mUserList, host, list));
}

void Controller::onUpdatedIncomingTransfer (sf::AsyncOpId id, sf::TransferInfo::TransferUpdateType type, const sf::TransferInfo & t) {
	mGUI->call (sf::bind (&TransferList::onUpdatedTransfer, mIncomingTransferList, id, type, t));
}

void Controller::onUpdatedListing  (const sf::Uri & uri, sf::Error error, const sf::DirectoryListing & listing) {
	mGUI->call (sf::bind(&UserList::onUpdatedListing, mUserList, uri, error, listing));
}

void Controller::onUpdatedOutgoingTransfer (sf::AsyncOpId id, sf::TransferInfo::TransferUpdateType type, const sf::TransferInfo & t) {
	mGUI->call (sf::bind(&TransferList::onUpdatedTransfer, mOutgoingTransferList, id, type, t));
}
