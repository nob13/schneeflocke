#include "ShareFileDialog.h"
#include "../modeladapters/ShareList.h"
#include "../modeladapters/ShareToUserList.h"
#include "../types.h"
#include <QMessageBox>

ShareFileDialog::ShareFileDialog (Controller * controller) {
	mController = controller;
	mShareFileDialogUi.setupUi (this);
	#if (QT_VERSION >= 0x40700)
	mShareFileDialogUi.shareNameEdit->setPlaceholderText(tr("Set Automatically"));
	#endif
}

void ShareFileDialog::init () {
	setAttribute (Qt::WA_QuitOnClose, false);

	mOpenDirectoryDialog = new QFileDialog (this, tr ("Choose Directory"));
	mOpenDirectoryDialog->setAcceptMode (QFileDialog::AcceptOpen);
	mOpenDirectoryDialog->setModal (true);
	mOpenDirectoryDialog->setFileMode (QFileDialog::Directory);
	mOpenDirectoryDialog->setOption (QFileDialog::ShowDirsOnly, true);
	connect (mOpenDirectoryDialog, SIGNAL (fileSelected (const QString&)), this, SLOT (onDirectorySelected(const QString&)));

	connect (mShareFileDialogUi.chooseDirectoryButton, SIGNAL (clicked()), mOpenDirectoryDialog, SLOT (show()));
	connect (this, SIGNAL (accepted()), this, SLOT (onShareFileAccept()));
	connect (mShareFileDialogUi.removeShareButton, SIGNAL (clicked()), this, SLOT (onShareRemove()));
	connect (mShareFileDialogUi.newShareButton, SIGNAL (clicked()), this, SLOT (onNewShare()));
	connect (mShareFileDialogUi.shareListView, SIGNAL  (clicked (const QModelIndex &)), this, SLOT (onUserSelectedShare (const QModelIndex&)));
	mShareFileDialogUi.specificUserListView->setModel (mController->shareToUserList());
	mShareFileDialogUi.shareListView->setModel (mController->shareList());
}

void ShareFileDialog::onDirectorySelected (const QString & dir) {
	mShareFileDialogUi.fileNameEdit->setText (dir);
	if (!mShareFileDialogUi.shareNameEdit->isModified()){
		QFileInfo info (dir);
		mShareFileDialogUi.shareNameEdit->setText (info.fileName());
	}
}

void ShareFileDialog::onShareFileAccept () {
	if (isModified()){
		saveShare ();
	}
	clear();
}

void ShareFileDialog::onShareRemove () {
	QModelIndex index =  mShareFileDialogUi.shareListView->currentIndex();
	if (!index.isValid()) return;

	QString shareName = mController->shareList()->stringList().at(index.row());
	mController->unshare(shareName);
	clear ();
}

void ShareFileDialog::onNewShare () {
	if (isModified()) {
		QMessageBox::StandardButton button = QMessageBox::question(this, tr ("Changed Shares"), tr ("Do you like to save your changes?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (button == QMessageBox::Yes) {
			saveShare();
		}
		if (button == QMessageBox::Cancel) {
			return;
		}
		if (button == QMessageBox::No) {
			// nothing...
		}
	}
	clear ();
}

void ShareFileDialog::onUserSelectedShare (const QModelIndex & index) {
	if (!index.isValid()) return;
	QString shareName = mController->shareList()->stringList().at(index.row());

	sf::FileSharing::FileShareInfo info;
	sf::Error e = mController->model()->fileSharing()->putInfo(::sfString(shareName), &info);
	if (e) return; // ?


	if (isModified()) {
		QMessageBox::StandardButton button = QMessageBox::question(this, tr ("Changed Shares"), tr ("Do you like to save your changes?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (button == QMessageBox::Yes) {
			saveShare();
		}
		if (button == QMessageBox::Cancel) {
			return;
		}
		if (button == QMessageBox::No) {
			// nothing...
		}
	}
	mCurrentShareName = shareName;

	mShareFileDialogUi.fileNameEdit->setText (qtString (info.fileName));
	mShareFileDialogUi.shareNameEdit->setText (shareName);
	mShareFileDialogUi.shareNameEdit->setModified(true);
	if (info.forAll) {
		mShareFileDialogUi.everyoneRadioButton->setChecked(true);
		mController->shareToUserList()->clearUserSet();
	} else {
		mShareFileDialogUi.specificUserRadioButton->setChecked(true);
		mController->shareToUserList()->setUserSet(info.whom);
	}
}

void ShareFileDialog::saveShare() {
	QString fileName  = mShareFileDialogUi.fileNameEdit->text ();
	QString shareName = mShareFileDialogUi.shareNameEdit->text ();
	bool forAll      = mShareFileDialogUi.everyoneRadioButton->isChecked();
	const sf::UserSet & whom = mController->shareToUserList()->checked();
	mController->editShare (shareName, fileName, forAll, whom);
	if (!mCurrentShareName.isEmpty() && mCurrentShareName != shareName){
		// someone changed the share name
		mController->unshare(mCurrentShareName);
		mCurrentShareName = shareName;
	}
}

void ShareFileDialog::clear () {
	mShareFileDialogUi.fileNameEdit->clear ();
	mShareFileDialogUi.shareNameEdit->clear ();
	mShareFileDialogUi.shareNameEdit->setModified (false);
	mShareFileDialogUi.fileNameEdit->setModified(false);
	mCurrentShareName.clear();
}

bool ShareFileDialog::isModified () const {
	sf::FileSharing::FileShareInfo info;
	QString shareName = mShareFileDialogUi.shareNameEdit->text();
	QString fileName  = mShareFileDialogUi.fileNameEdit->text();
	if (shareName.isEmpty() || fileName.isEmpty()) return false;

	sf::Error e = mController->model()->fileSharing()->putInfo(::sfString(shareName), &info);
	if (e) return true; // not existant?
	if (info.fileName != sfString (fileName)) return true;
	if (info.forAll != mShareFileDialogUi.everyoneRadioButton->isChecked()) return true;
	if (info.whom != mController->shareToUserList()->checked()) return true;
	return false;
}
