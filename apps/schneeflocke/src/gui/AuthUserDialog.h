#pragma once

#include <schnee/sftypes.h>
#include <QMessageBox>

/// A dialog displayed when someone wants to subscribe you
/// (you have to authorize him)
class AuthUserDialog : public QMessageBox {
public:
	AuthUserDialog (const sf::UserId & user, QWidget * parent);

	/// Returns formerly user id used for creation
	const sf::UserId & userId () const { return mUserId; }

	enum Result { Accept, Reject, Ignore };
	Result result ();


private:
	sf::UserId mUserId;
};
