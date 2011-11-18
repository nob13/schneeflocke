#include "AuthUserDialog.h"
#include "../types.h"
#include <assert.h>

AuthUserDialog::AuthUserDialog (const sf::UserId & user, QWidget * parent) : QMessageBox (parent){
	setIcon (QMessageBox::Question);
	setWindowTitle ("User Authentication");
	setText (tr ("User %1 would like to add you to his contact list, do you accept?").arg (qtString (user)));
	setStandardButtons (QMessageBox::Yes | QMessageBox::No | QMessageBox::Ignore);
	setEscapeButton (QMessageBox::Ignore);
	setDefaultButton (QMessageBox::Yes);
	setAttribute (Qt::WA_DeleteOnClose);

	setWindowModality (Qt::NonModal);
	mUserId = user;
}

AuthUserDialog::Result AuthUserDialog::result () {
	int r = QMessageBox::result(); // according to http://bugreports.qt.nokia.com/browse/QTBUG-15738 it returns the acutal button
	if (r == QMessageBox::Yes) return Accept;
	if (r == QMessageBox::No) return Reject;
	if (r == QMessageBox::Ignore) return Ignore;
	assert (!"Strange return value?");
	return Reject;
}

