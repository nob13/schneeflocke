#pragma once
#include <QDialog>
#include <QFileDialog>
#include "../Controller.h"
#include "../types.h"

#include "ui_settingswindow.h"

class SettingsDialog : public QDialog {
Q_OBJECT
public:
	SettingsDialog (Controller * controller);
	void init();
private slots:
	void onSettingsAccepted ();
	/// A directory for saving files was selected
	void onFileDestinationSelected (const QString & dir);

private:
	Controller * mController;
	Ui::SettingsWindow 	mSettingsWindowUi;
	QFileDialog * mFileDestinationChooseDialog;
};
