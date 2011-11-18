#pragma once
#include <schnee/sftypes.h>

#include <QMessageBox>

/// Dialog used for removing a user from the contact list
class RemoveUserDialog : public QMessageBox {
public:
	RemoveUserDialog (const sf::UserId & userId, QWidget * parent);
	virtual ~RemoveUserDialog ();

	const sf::UserId & userId () const { return mUserId; }

	enum Result { No, Yes };
	Result result ();
private:
	sf::UserId mUserId;
};
