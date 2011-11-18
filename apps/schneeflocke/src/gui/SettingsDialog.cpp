#include "SettingsDialog.h"

SettingsDialog::SettingsDialog (Controller * controller) {
	mController = controller;
	mSettingsWindowUi.setupUi(this);
	mFileDestinationChooseDialog = 0;
}

void SettingsDialog::init() {
	/// Settings Window
	connect (mSettingsWindowUi.buttonBox, SIGNAL (accepted()), this, SLOT (onSettingsAccepted()));
	const Model::Settings & settings = mController->model()->settings();
	sf::String user, server;
	splitUserServer (settings.userId, &user, &server);
	mSettingsWindowUi.usernameEdit->setText (qtString (user));
	mSettingsWindowUi.serverEdit->setEditText(qtString (server));
	mSettingsWindowUi.passwordEdit->setText (qtString (settings.password));
	mSettingsWindowUi.useBoshCheckBox->setCheckState (settings.useBosh ? Qt::Checked : Qt::Unchecked);
	mSettingsWindowUi.fileDestinationEdit->setText (qtString (settings.destinationDirectory));
	// mSettingsWindowUi.slxmppCheckbox->setCheckState (settings.useSLXMPP ? Qt::Checked : Qt::Unchecked);
	mSettingsWindowUi.echoServerIpEdit->setText (qtString (settings.echoServerIp));
	mSettingsWindowUi.echoServerPortEdit->setValue (settings.echoServerPort);
	mSettingsWindowUi.autoConnectCheckBox->setCheckState (settings.autoConnect ? Qt::Checked : Qt::Unchecked);
	::setDefaultServer(server); // TODO Should be done in controller

	if (mController->autostartAvailable()){
		mSettingsWindowUi.autostartCheckBox->setCheckState (mController->autostartInstalled() ? Qt::Checked : Qt::Unchecked);
	} else {
		mSettingsWindowUi.autostartCheckBox->setEnabled (false);
		mSettingsWindowUi.autostartCheckBox->setHidden(true);
	}

	mFileDestinationChooseDialog = new QFileDialog (this, tr ("Choose Directory"));
	mFileDestinationChooseDialog->setAcceptMode (QFileDialog::AcceptOpen);
	mFileDestinationChooseDialog->setModal (true);
	mFileDestinationChooseDialog->setFileMode (QFileDialog::Directory);
	mFileDestinationChooseDialog->setOption (QFileDialog::ShowDirsOnly, true);
	connect (mFileDestinationChooseDialog, SIGNAL (fileSelected (const QString&)), this, SLOT (onFileDestinationSelected(const QString&)));
	connect (mSettingsWindowUi.fileDestinationChooseButton, SIGNAL (clicked()), mFileDestinationChooseDialog, SLOT (show()));
}

void SettingsDialog::onSettingsAccepted () {
	Model::Settings settings = mController->model()->settings();
	sf::String server = sfString (mSettingsWindowUi.serverEdit->currentText());
	settings.userId = sfString (mSettingsWindowUi.usernameEdit->text()) + "@" + server;
	settings.password = sfString (mSettingsWindowUi.passwordEdit->text());
	settings.destinationDirectory = sfString (mSettingsWindowUi.fileDestinationEdit->text());
	// No SLXMPP Button anymore
	// settings.useSLXMPP = (mSettingsWindowUi.slxmppCheckbox->checkState() == Qt::Checked ? true : false);
	settings.useBosh = (mSettingsWindowUi.useBoshCheckBox->checkState() == Qt::Checked ? true : false);
	settings.echoServerIp   = sfString (mSettingsWindowUi.echoServerIpEdit->text());
	settings.echoServerPort = mSettingsWindowUi.echoServerPortEdit->value();
	settings.autoConnect    = (mSettingsWindowUi.autoConnectCheckBox->checkState()  == Qt::Checked ? true : false);
	mController->updateSettings(settings);
	::setDefaultServer(server);

	bool autostart = mSettingsWindowUi.autostartCheckBox->checkState () == Qt::Checked;
	mController->setAutostartInstall (autostart);

	// Register functionality is disabled on sflx.net
	/*
	if (mSettingsWindowUi.createAccountCheckBox->isChecked()){
		mController->registerAccount();
		// disable it again
		mSettingsWindowUi.createAccountCheckBox->setCheckState(Qt::Unchecked);
	}
	*/
	close ();
}

void SettingsDialog::onFileDestinationSelected (const QString & dir) {
	mSettingsWindowUi.fileDestinationEdit->setText (dir);
}



