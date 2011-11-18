#pragma once
#include "../Controller.h"

#include "ui_sharefiledialog.h"
#include <QDialog>
#include <QFileDialog>

/// Dialog for sharing files and editing this shares
/// Just call show to let it do its work!
class ShareFileDialog : public QDialog {
Q_OBJECT
public:
	ShareFileDialog (Controller * controller);

	/// Call during GUI::init()
	void init ();
private slots:
	/// Directory selected
	void onDirectorySelected (const QString & dir);

	/// User accepted sharing of a file
	void onShareFileAccept ();
	/// User removes a share
	void onShareRemove ();
	/// User clears a share
	void onNewShare ();
	/// USer entered a share
	void onUserSelectedShare (const QModelIndex & index);

private:
	void saveShare();
	void clear ();
	bool isModified () const;

	Ui::ShareFileDialog mShareFileDialogUi;
	Controller * mController;

	QFileDialog * mOpenDirectoryDialog;
	QString mCurrentShareName; //< selected share name
};
