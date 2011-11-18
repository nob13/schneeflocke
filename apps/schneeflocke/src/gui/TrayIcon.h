#pragma once
#include <QSystemTrayIcon>
#include <QIcon>
#include "../Controller.h"
#include <QMainWindow>

// Handles the TrayIcon of Schneeflocke App
class TrayIcon : public QObject {
Q_OBJECT
public:
	TrayIcon (Controller * controller);
	~TrayIcon ();
	void init (QMainWindow * mainWindow);

	void setState (Controller::State);
	void show () { mTrayIcon.show(); }
	void hide () { mTrayIcon.hide(); }
private slots:
	void onQuit ();
	void onActivated (QSystemTrayIcon::ActivationReason reason);
	void onShowWindow ();
private:
	QIcon mIconForTrayOffline;
	QIcon mIconForTrayOnline;
	QMenu *  mMenu;
	QSystemTrayIcon mTrayIcon;

	Controller * mController;
	QMainWindow * mMainWindow;
};
