#pragma once
#include <QWidget>
#include <QFileDialog>
#include "../Model.h"
#include "../QtThreadGateway.h"
#include "ShareFileDialog.h"
#include "SettingsDialog.h"
#include "TransferWindow.h"
#include "TrayIcon.h"

// Uis (generated)
#include "ui_mainwindow.h"
#include "ui_adduserdialog.h"
#include "ui_userinfowindow.h"
#include "ui_connectionwindow.h"

class Controller;

/// QT GUI (View-Part)
class GUI : public QtThreadGateway/*, public Model::Observer*/ {
Q_OBJECT
public:
	GUI (Controller * controller);
	~GUI ();
	/// Initializes GUI
	void init ();

	// Callbacks by Controller (must be done in QtThread)
	void onStateChange ();
	void onError (err::Code code, const QString & title, const QString & what);
	/// print some success info (MessageBox)
	void onSuccess (const QString & title, const QString & what);
	void onLostTracking (const sf::HostId & hostId, const QString & errMsg);
	void onUserSubscribeRequest (const sf::UserId & userId);
signals:
	void setCurrentOnlineState (int); // forwards to state combo box

private slots:
	void onDesiredStateChange (int index);

	/// Double click inside the Sharing view
	void onViewDoubleClicked (const QModelIndex & index);

	/// Shows right-click context menu on sharing view
	void onViewShowContextMenu (const QPoint & position);

	/// Callback for browse directory action
	void onBrowseDirectory ();

	/// Callback for download directory action
	void onDownloadDirectory ();

	/// Callback for download file action
	void onDownloadFile ();

	/// Add user dialog finished
	void onAddUserFinished (int result);

	/// A user subscribe request - dialog box is closed
	void onUserSubscribeRequestDialogFinished (int result);

	/// Remove User action
	void onRemoveUser ();
	/// Remove user dialog finished
	void onRemoveUserDialogFinished (int result);

	/// Show info about a user
	void onShowUserInfo();

	void onOpenWebsite();
	void onShowAbout ();
	void onQuit();

private:

	QMainWindow         mMainWindow;
	Ui::MainWindow 		mMainWindowUi;

	SettingsDialog mSettingsDialog;

	QWidget 			 mConnectionWindow;
	Ui::ConnectionWindow mConnectionWindowUi;

	QDialog             mAddUserDialog;
	Ui::AddUserDialog   mAddUserDialogUi;
	Ui::UserInfoWindow  mUserInfoWindowUi;


	Controller * mController;

	// Actions
	QAction * mBrowseDirectoryAction;
	QAction * mDownloadDirectoryAction;
	QAction * mDownloadFileAction;
	QAction * mRemoveUserAction;
	QAction * mShowUserInfoAction;

	ShareFileDialog mShareFileDialog;
	TransferWindow mTransferWindow;

	TrayIcon mTrayIcon;
};
