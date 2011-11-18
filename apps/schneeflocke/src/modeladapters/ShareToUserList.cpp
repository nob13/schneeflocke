#include "ShareToUserList.h"
#include <schnee/tools/Log.h>

#include "userlist/UserItem.h" // for default server

using namespace sf;

ShareToUserList::ShareToUserList (QObject * parent) : QAbstractListModel (parent) {

}

ShareToUserList::~ShareToUserList () {

}

void ShareToUserList::update (const sf::PresenceManagement::UserInfoMap & users) {
	typedef sf::PresenceManagement::UserInfoMap UserInfoMap;
	mEntries.clear();
	for (UserInfoMap::const_iterator i = users.begin(); i != users.end(); i++) {
		Entry e;
		e.id   = i->first;
		e.displayName = ::formatUserName(i->first, i->second.name);
		mEntries.push_back (e);
	}
	{
		// Collecting checked elements not existant anymore...
		sf::UserSet::iterator i = mChecked.begin();
		while (i != mChecked.end()){
			if (users.count(*i) == 0){
				mChecked.erase(i++);
			} else i++;
		}
	}
	emit (layoutChanged());
	emit (dataChanged (index(0,0), index(mEntries.size(), 1)));
}

void ShareToUserList::setUserSet (const sf::UserSet & set) {
	mChecked = set;
	emit (dataChanged(index(0,0), index (mEntries.size(), 1)));
}

void ShareToUserList::clearUserSet () {
	mChecked.clear();
	emit (dataChanged(index(0,0), index (mEntries.size(), 1)));
}

QVariant ShareToUserList::data (const QModelIndex & index, int role) const {
	int r = index.row ();
	if (r < 0 || r > (int) mEntries.size()) {
		sf::Log (LogWarning) << LOGID << "Strange row" << std::endl;
		return QVariant ();
	}

	const Entry & e = mEntries[r];
	if (role == Qt::CheckStateRole){
		return mChecked.count(e.id) > 0 ? Qt::Checked : Qt::Unchecked;
	}
	if (role != Qt::DisplayRole) return QVariant();
	return mEntries[r].displayName;
}

int ShareToUserList::rowCount (const QModelIndex & parent) const {
	return (int) mEntries.size();
}

bool ShareToUserList::setData ( const QModelIndex & index, const QVariant & value, int role) {
	if (role == Qt::CheckStateRole) {
		int r = index.row();
		if (r < 0 || r > (int) mEntries.size()) return false;
		const Entry & e (mEntries[r]);
		bool v = value == Qt::Checked;
		if (v){
			mChecked.insert(e.id);
		} else {
			mChecked.erase(e.id);
		}
		return true;
	}
	return false;
}


