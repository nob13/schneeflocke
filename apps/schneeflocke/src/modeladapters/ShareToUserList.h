#pragma once

#include <QAbstractListModel>
#include <schnee/p2p/PresenceManagement.h>

/// User List which appears on the share to user dialog
/// Users are checkable.
class ShareToUserList : public QAbstractListModel {
public:
	ShareToUserList (QObject * parent = 0);
	virtual ~ShareToUserList ();

	/// Updates content
	void update (const sf::PresenceManagement::UserInfoMap & users);

	/// Sets an explicit user set
	void setUserSet (const sf::UserSet & users);
	/// clears the user set
	void clearUserSet ();

	/// Returns current checked users
	const sf::UserSet & checked () const { return mChecked; }

	// Implementation of AbstractListModel
	int columnCount () const { return 1; }
	QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const;
	Qt::ItemFlags flags ( const QModelIndex & index ) const {
		return QAbstractListModel::flags(index) | Qt::ItemIsUserCheckable;
	}
	QModelIndex parent ( const QModelIndex & index ) const { return QModelIndex(); };
	int rowCount (const QModelIndex & parent = QModelIndex()) const;
	bool setData ( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole );
private:
	struct Entry {
		sf::UserId id;
		QString displayName;
	};
	std::vector<Entry> mEntries;
	sf::UserSet mChecked;
};
