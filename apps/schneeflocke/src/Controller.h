#pragma once
#include "Model.h"
#include "AppError.h"
#include <QSettings>
#include <schnee/tools/async/DelegateBase.h>
#include <schnee/schnee.h>

class GUI;
class UserList;
class TransferList;
class ShareToUserList;
class ConnectionList;
class ShareList;

/** App logic controller
 *  Mostly responsible for translating delegate callbacks
 *  Into Qt-compatible calls to the GUI.
 *
 *  The GUI itself has access to the Model (but only const) through Controller
 *
 *  The controller also handles some Qt-Level Models (UserList, TransferList)
 *  They are not inside Model to not taint it with Qt-Code (and the models
 *  need passing data into the Qt-Thread). They are not inside GUI as the GUI
 *  just interprets data from them.
 */
class Controller : public sf::DelegateBase {
public:
	Controller ();
	~Controller ();
	/// read-only access to the model
	/// Note the model must be locked
	const Model * umodel () const;

	/// Initializes model and view
	void init ();

	enum State {
		CONNECTED, CONNECTING, OFFLINE, ERROR
	};
	
	// API for GUI
	void quit ();
	void updateSettings (const Model::Settings & settings);
	void share (const QString & shareName, const QString & fileName, bool forAll, const sf::UserSet & whom);
	void editShare (const QString & shareName, const QString & fileName, bool forAll, const sf::UserSet & whom);
	void unshare (const QString & shareName);

	void registerAccount ();

	bool isConnectedTo (const sf::HostId & hostId);
	sf::Error trackHostShared (const sf::HostId & hostId);
	void listDirectory (const sf::Uri & uri);
	void requestFile (const sf::Uri & uri);
	void requestDirectory (const sf::Uri & uri);
	void cancelIncomingTransfer (sf::AsyncOpId id);
	void clearIncomingTransfers ();
	void cancelOutgoingTransfer (sf::AsyncOpId id);
	void clearOutgoingTransfers ();
	void addUser (const sf::UserId & userId);
	void subscriptionRequestReply (const sf::UserId & userId, bool allow, bool alsoAdd);
	void removeUser (const sf::UserId & userId);
	State state ();

	enum DesiredState { DS_CONNECT, DS_DISCONNECT };

	// Try to change connection
	void changeConnection (DesiredState connect);

	// autostart (true on success)
	bool autostartAvailable () const;
	bool autostartInstalled () const;
	bool setAutostartInstall (bool v);

	// Model Adapters
	UserList * userList () { return mUserList; }
	TransferList * incomingTransferList () { return mIncomingTransferList; }
	TransferList * outgoingTransferList () { return mOutgoingTransferList; }
	ShareToUserList * shareToUserList() { return mShareToUserList; }
	ConnectionList * connections () { return mConnectionList; }
	ShareList * shareList () { return mShareList; }
	
private:
	void onChangeConnection (DesiredState connect);

	void onError        (err::Code code, const QString & title, const QString & what);
	
	/// Load shared elements from INI file
	void loadShares ();
	/// Saves shared elements from INI file
	void saveShares ();


	// Connect callback
	void onConnect (sf::Error err);
	// Presence delegates
	void onPeersChanged ();
	void onOnlineStateChanged (sf::OnlineState os);
	void onServerStreamErrorReceived (const sf::String & text);
	void onUserSubscribeRequest (const sf::UserId & userId);
	void onConDetailsChanged ();

	void onConnectHostForTrackingResult (sf::Error result, const sf::HostId & hostId);
	void onRegisterAccount (sf::Error e);

	// Callbacks for SharedListTracker
	void onLostTracking (const sf::HostId & user, const sf::Error cause);
	void onTrackingUpdate (const sf::HostId & user, const sf::SharedList & list);
	// Callbacks for FileGetting
	void onUpdatedIncomingTransfer (sf::AsyncOpId id, sf::TransferInfo::TransferUpdateType type, const sf::TransferInfo & t);
	void onUpdatedListing  (const sf::Uri & uri, sf::Error error, const sf::DirectoryListing & listing);
	// Callbacks for FileSharing
	void onUpdatedOutgoingTransfer (sf::AsyncOpId id, sf::TransferInfo::TransferUpdateType type, const sf::TransferInfo & t);

	Model * mModel;
	GUI   * mGUI;
	UserList * mUserList;
	TransferList * mIncomingTransferList;
	TransferList * mOutgoingTransferList;
	ShareToUserList * mShareToUserList;
	ConnectionList * mConnectionList;
	ShareList * mShareList;
	State      mState;
	QSettings * mSettings;
};
