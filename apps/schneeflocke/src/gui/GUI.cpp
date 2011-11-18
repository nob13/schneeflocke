#include "GUI.h"
#include "../Controller.h"
#include "../types.h"
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include "AuthUserDialog.h"
#include "RemoveUserDialog.h"


#include "../modeladapters/UserList.h"
#include "../modeladapters/ShareToUserList.h"
#include "../modeladapters/ConnectionList.h"
#include "../modeladapters/ShareList.h"

#include "ProgressItemDelegate.h"
#include <qglobal.h>

GUI::GUI (Controller * controller) : mSettingsDialog (controller), mShareFileDialog (controller), mTransferWindow (controller), mTrayIcon (controller) {
	mController = controller;
}

GUI::~GUI () {
}

void GUI::init () {
	mMainWindowUi.setupUi (&mMainWindow);
	mConnectionWindowUi.setupUi (&mConnectionWindow);
	mAddUserDialogUi.setupUi (&mAddUserDialog);

	#if (QT_VERSION >= 0x40700)
	mAddUserDialogUi.userIdEdit->setPlaceholderText ("Your contacts User Id");
	#endif

	// Termination of process is only done explicit (if tray icon is available)
	mSettingsDialog.setAttribute (Qt::WA_QuitOnClose, false);
	mTransferWindow.setAttribute (Qt::WA_QuitOnClose, false);
	mConnectionWindow.setAttribute (Qt::WA_QuitOnClose, false);
	mShareFileDialog.setAttribute (Qt::WA_QuitOnClose, false);
	mAddUserDialog.setAttribute (Qt::WA_QuitOnClose, false);
	if (QSystemTrayIcon::isSystemTrayAvailable()){
		// can close the window, user can still use the tray icon
		mMainWindow.setAttribute (Qt::WA_QuitOnClose, false);
	} else {
		qWarning() << "No SystemTrayIcon available. Application will close on closing the main window";
	}


	// Actions on mainWindow
	connect(mMainWindowUi.actionShare, SIGNAL (triggered()), &mShareFileDialog, SLOT (show()));
	connect(mMainWindowUi.actionAddUser, SIGNAL (triggered()), &mAddUserDialog, SLOT (show()));
	connect(mMainWindowUi.actionQuit, SIGNAL (triggered()), this, SLOT (onQuit()));
	connect(mMainWindowUi.actionTransfers, SIGNAL (triggered()), &mTransferWindow, SLOT (show()));
	connect(mMainWindowUi.actionConnections, SIGNAL (triggered()), &mConnectionWindow, SLOT (show()));
	connect(mMainWindowUi.actionAbout, SIGNAL (triggered()), this, SLOT (onShowAbout()));
	connect(mMainWindowUi.actionOpen_Website, SIGNAL (triggered()), this, SLOT (onOpenWebsite()));
	connect(mMainWindowUi.actionSettings, SIGNAL (triggered()), &mSettingsDialog, SLOT (show()));

	// Add User Window
	connect (&mAddUserDialog, SIGNAL (finished(int)), this, SLOT (onAddUserFinished(int)));

	// Connect Connections Data Model
	mConnectionWindowUi.connectionTable->setModel (mController->connections());

	// Connect Data Model (online Users)
	mMainWindowUi.treeView->setModel (mController->userList());
	connect (mMainWindowUi.treeView, SIGNAL (doubleClicked(const QModelIndex&)), this, SLOT (onViewDoubleClicked(const QModelIndex&)));
	mMainWindowUi.treeView->setContextMenuPolicy (Qt::CustomContextMenu);
	connect (mMainWindowUi.treeView, SIGNAL (customContextMenuRequested(const QPoint &)), this, SLOT (onViewShowContextMenu(const QPoint&)));

	// Online StateBox
	QStringList stateNames; stateNames <<  "Offline" << "Online"; // only public callable states do list here
	mMainWindowUi.stateBox->addItems (stateNames);
	connect (mMainWindowUi.stateBox, SIGNAL ( currentIndexChanged(int)), this, SLOT (onDesiredStateChange(int)));

	// Actions
	mBrowseDirectoryAction   = new QAction (tr("Browse"), this);
	mDownloadDirectoryAction = new QAction (tr("Download"), this);
	mDownloadFileAction      = new QAction (tr("Download"), this);
	mRemoveUserAction        = new QAction (tr("Remove Contact"), this);
	mShowUserInfoAction          = new QAction (tr("Show User Info"), this);

	connect (mBrowseDirectoryAction,   SIGNAL (triggered()), this, SLOT (onBrowseDirectory()));
	connect (mDownloadDirectoryAction, SIGNAL (triggered()), this, SLOT (onDownloadDirectory()));
	connect (mDownloadFileAction, SIGNAL (triggered()), this, SLOT (onDownloadFile()));
	connect (mRemoveUserAction, SIGNAL (triggered()), this, SLOT (onRemoveUser()));
	connect (mShowUserInfoAction, SIGNAL (triggered()), this, SLOT (onShowUserInfo()));

	mShareFileDialog.init ();
	mSettingsDialog.init();
	mTransferWindow.init();
	mTrayIcon.init(&mMainWindow);

	mMainWindow.show();
	mTrayIcon.show();
}

void GUI::onStateChange () {
	Controller::State state = mController->state();
	if (state == Controller::ERROR || state == Controller::OFFLINE) {
		mMainWindowUi.stateBox->setCurrentIndex (0); // not connected
	}
	if (state == Controller::CONNECTING){
		mMainWindowUi.statusbar->showMessage (tr("Connecting..."));
	}
	if (state == Controller::CONNECTED){
		mMainWindowUi.statusbar->showMessage (tr("Connected"));
		mMainWindowUi.stateBox->setCurrentIndex (1); // connected
	}
	if (state == Controller::OFFLINE){
		mMainWindowUi.statusbar->showMessage (tr("Offline"));
	}
	if (state == Controller::ERROR){
		mMainWindowUi.statusbar->showMessage (tr("Error"));
	}
	mTrayIcon.setState (state);
}

void GUI::onError       (err::Code code, const QString & title, const QString & what) {
	QMessageBox::warning(&mMainWindow, title, what);
	// TODO: Irrelevant errors shall not be presented with QMessageBox
	// mMainWindowUi.statusbar->showMessage (QString ("Err: ") + what);
}

void GUI::onSuccess (const QString&  title, const QString & what) {
	QMessageBox::information(&mMainWindow, title, what);
}

void GUI::onLostTracking (const sf::HostId & hostId, const QString & errMsg) {
	mMainWindowUi.statusbar->showMessage (QString ("Lost Tracking to ") + qtString (hostId) + " due " + errMsg);
}

void GUI::onUserSubscribeRequest (const sf::UserId & userId) {
	AuthUserDialog * dialog = new AuthUserDialog (userId, &mMainWindow);
	QObject::connect (dialog, SIGNAL (finished(int)), this, SLOT (onUserSubscribeRequestDialogFinished(int)));
	dialog->show();
}

void GUI::onDesiredStateChange (int index) {
	if (index < 0) return; // no support
	if (index == 0) mController->changeConnection (Controller::DS_DISCONNECT);
	if (index == 1) mController->changeConnection (Controller::DS_CONNECT);
}

void GUI::onViewDoubleClicked (const QModelIndex & index) {
	QStandardItem * item = mController->userList()->itemFromIndex (index);
	int itemType = item->type();

	if (itemType == userlist::ShareItemType){
		userlist::ShareItem * shareItem = static_cast<userlist::ShareItem*> (item);
		if (shareItem->isDirectory()){
			return;
		}
		mController->requestFile (shareItem->uri());
		mTransferWindow.show ();
	}
	if (itemType == userlist::HostItemType) {
		userlist::HostItem * hostItem = static_cast<userlist::HostItem*> (item);
		sf::HostId hostId = hostItem->info().hostId;
		if (!mController->isConnectedTo(hostId)){
			mController->trackHostShared(hostId);
			hostItem->setPopulated(false);
			hostItem->setConnecting();
		}
	}
}

void GUI::onViewShowContextMenu (const QPoint & position) {
	QList<QAction*> actions;
	QModelIndex index = mMainWindowUi.treeView->indexAt (position);
	if (index.isValid()){
		QStandardItem * item = mController->userList()->itemFromIndex(index);
		int itemType = item->type();
		if (itemType == userlist::ShareItemType){
			userlist::ShareItem * shareItem = static_cast<userlist::ShareItem*> (item);
			if (shareItem->isDirectory()){
				actions.append (mBrowseDirectoryAction);
				actions.append (mDownloadDirectoryAction);
			} else {
				actions.append (mDownloadFileAction);
			}
		}
		if (itemType == userlist::UserItemType){
			actions.append(mShowUserInfoAction);
			actions.append(mRemoveUserAction);
		}
	}
	if (actions.count() > 0){
		QMenu::exec (actions, QCursor::pos()/*mMainWindowUi.treeView->mapToGlobal (position)*/);
	}
}

void GUI::onBrowseDirectory () {
	QModelIndex index = mMainWindowUi.treeView->currentIndex ();
	if (!index.isValid()) return;
	QStandardItem * item = mController->userList()->itemFromIndex(index);
	if (item->type() == userlist::ShareItemType){
		userlist::ShareItem * shareItem = static_cast<userlist::ShareItem*> (item);
		if (shareItem->isDirectory()){
			mMainWindowUi.treeView->expand (index);
			mController->listDirectory(shareItem->uri());
		}
	}
}

void GUI::onDownloadDirectory () {
	QModelIndex index = mMainWindowUi.treeView->currentIndex ();
	if (!index.isValid()) return;
	QStandardItem * item = mController->userList()->itemFromIndex(index);
	if (item->type() == userlist::ShareItemType){
		userlist::ShareItem * shareItem = static_cast<userlist::ShareItem*> (item);
		if (shareItem->isDirectory()){
			mController->requestDirectory (shareItem->uri());
			mTransferWindow.show ();
		}
	}
}

void GUI::onDownloadFile () {
	QModelIndex index = mMainWindowUi.treeView->currentIndex ();
	if (!index.isValid()) return;
	QStandardItem * item = mController->userList()->itemFromIndex(index);
	if (item->type() == userlist::ShareItemType){
		userlist::ShareItem * shareItem = static_cast<userlist::ShareItem*> (item);
		if (!shareItem->isDirectory()){
			mController->requestFile (shareItem->uri());
			mTransferWindow.show ();
		}
	}
}

void GUI::onAddUserFinished (int result) {
	if (result == QDialog::Accepted){
		sf::UserId userId = sfString (mAddUserDialogUi.userIdEdit->text()) + "@" + sfString (mAddUserDialogUi.serverEdit->currentText());
		mController->addUser (userId);
	}
	// Putting to default again...
	mAddUserDialogUi.userIdEdit->clear();
}

void GUI::onUserSubscribeRequestDialogFinished (int result) {
	AuthUserDialog * dialog = static_cast<AuthUserDialog*> (sender ());
	AuthUserDialog::Result r = dialog->result();
	switch (r) {
	case AuthUserDialog::Accept:
		mController->subscriptionRequestReply(dialog->userId(), true, true);
		break;
	case AuthUserDialog::Reject:
		mController->subscriptionRequestReply(dialog->userId(), false, false);
		break;
	default:
		// ignore
		break;
	}
	// do we have to delete the dialog? when we delete it, qt crashes...
}

void GUI::onRemoveUser () {
	userlist::UserItem * userItem = mController->userList()->userItem (mMainWindowUi.treeView->currentIndex());
	if (!userItem) return;
	// Showing remove dialog...
	RemoveUserDialog * dialog = new RemoveUserDialog (userItem->info().userId, &mMainWindow);
	connect (dialog, SIGNAL (finished (int)), this, SLOT (onRemoveUserDialogFinished(int)));
	dialog->show();
}

void GUI::onRemoveUserDialogFinished (int result) {
	RemoveUserDialog * dialog = static_cast<RemoveUserDialog*> (sender());
	RemoveUserDialog::Result r = dialog->result();
	switch (r) {
		case RemoveUserDialog::Yes:
			mController->removeUser (dialog->userId());
		break;
		default:
			// ignore
		break;
	}
}

void GUI::onShowUserInfo() {
	userlist::UserItem * userItem = mController->userList()->userItem (mMainWindowUi.treeView->currentIndex());
	const userlist::UserItem::UserInfo & info = userItem->info();

	if (!userItem) return;
	QWidget * w = new QWidget ();

	mUserInfoWindowUi.setupUi (w);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->setAttribute(Qt::WA_QuitOnClose, false);

	if (info.error)
		mUserInfoWindowUi.errorLabel->setText (tr ("Error: ") + qtString (info.errorText));
	if (info.waitForSubscription){
		mUserInfoWindowUi.pendingSubscriptionLabel->setText (tr ("Subscription Pending"));
	}
	sf::String user,server;
	splitUserServer (info.userId, &user, &server);
	mUserInfoWindowUi.userIdEdit->setText (qtString(user));
	mUserInfoWindowUi.serverEdit->setText (qtString (server));
	mUserInfoWindowUi.nameEdit->setText (qtString (info.name));
	connect (mUserInfoWindowUi.buttonBox, SIGNAL (accepted()), w, SLOT (close()));
	w->show ();
}

void GUI::onOpenWebsite() {
	QDesktopServices::openUrl (QUrl ("http://www.sflx.net/"));
}

void GUI::onShowAbout () {
	QMessageBox::about(&mMainWindow, tr ("About Schneeflocke"), tr("This is Schneeflocke %1.\nCopyright 2010-2011 the sflx.net Team.").arg (sf::schnee::version()));
}

void GUI::onQuit() {
	mController->quit();
}

