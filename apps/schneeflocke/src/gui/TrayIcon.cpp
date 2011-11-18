#include "TrayIcon.h"
#include <QMenu>
#include <QAction>
#include <QApplication>

TrayIcon::TrayIcon (Controller * controller) {
	mMainWindow = 0;
	mController = controller;
	mIconForTrayOffline = QIcon (":/schneeflocke_offline.png");
	mIconForTrayOnline  = QIcon (":/schneeflocke.png");
	mTrayIcon.setIcon(mIconForTrayOffline);
}

TrayIcon::~TrayIcon () {
	delete mMenu;
}

void TrayIcon::init (QMainWindow * mainWindow) {
	mMainWindow = mainWindow;

	QAction * quitAction = new QAction (tr ("Quit"), this);
	QAction * showMainWindow = new QAction (tr ("Show Window"), this);
	connect (quitAction, SIGNAL (triggered()), this, SLOT (onQuit()));
	connect (showMainWindow, SIGNAL (triggered()), this, SLOT (onShowWindow()));
	mMenu = new QMenu ();
	mMenu->addAction(showMainWindow);
	mMenu->addSeparator();
	mMenu->addAction (quitAction);
	mTrayIcon.setContextMenu(mMenu);
	connect (&mTrayIcon, SIGNAL (activated(QSystemTrayIcon::ActivationReason)), this, SLOT (onActivated (QSystemTrayIcon::ActivationReason)));
}

void TrayIcon::setState (Controller::State state) {
	if (state == Controller::CONNECTED){
		mTrayIcon.setIcon (mIconForTrayOnline);
	} else {
		mTrayIcon.setIcon (mIconForTrayOffline);
	}
}

void TrayIcon::onQuit () {
	mController->quit();
}

void TrayIcon::onActivated (QSystemTrayIcon::ActivationReason reason) {
	if (reason == QSystemTrayIcon::Context)
		return; // already handled in menu
	if (mMainWindow){
		if (mMainWindow->isVisible() && QApplication::activeWindow()){
			mMainWindow->hide();
		} else {
			onShowWindow();
		}
	}
}

void TrayIcon::onShowWindow () {
	if (mMainWindow) {
		if (mMainWindow->isVisible()){
			QApplication::setActiveWindow(mMainWindow);
			mMainWindow->activateWindow();
		} else
			mMainWindow->show();
	}
}


