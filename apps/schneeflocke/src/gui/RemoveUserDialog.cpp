#include "RemoveUserDialog.h"
#include "../types.h"

RemoveUserDialog::RemoveUserDialog (const sf::UserId & userId, QWidget * parent) {
	setIcon (QMessageBox::Question);
	setWindowTitle ("Remove User");
	setText (tr ("Would you really like to remove user %1 from your contact list?").arg (qtString (userId)));
	setStandardButtons (QMessageBox::Yes | QMessageBox::No);
	setEscapeButton (QMessageBox::No);
	setDefaultButton (QMessageBox::No);
	setWindowModality (Qt::NonModal);
	setAttribute (Qt::WA_DeleteOnClose);
	setAttribute (Qt::WA_QuitOnClose, false);

	mUserId = userId;
}

RemoveUserDialog::~RemoveUserDialog () {

}

RemoveUserDialog::Result RemoveUserDialog::result () {
	int r = QMessageBox::result();
	if (r == QMessageBox::Yes) return Yes;
	if (r == QMessageBox::No) return No;
	assert (!"Strange return value");
	return No;
}
