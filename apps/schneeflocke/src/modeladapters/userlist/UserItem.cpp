#include "UserItem.h"
#include "HostItem.h"
namespace userlist {

const int UserItemType      = 1001;

UserItem::UserItem (const UserInfo & info) {
	set (info);
}

void UserItem::set (const UserInfo & info) {
	setEditable (false);
	mUserInfo = info;
	updatePresentation ();
}

HostItem * UserItem::hostItem (const sf::HostId & hostItem) {
	HostItemMap::iterator i = mHosts.find (hostItem);
	if (i == mHosts.end()) return 0;
	return i->second;
}

void UserItem::onUpdateHosts (const HostInfoMap & hosts) {
	bool changed = false; // host list changed (added or removed)
	for (HostInfoMap::const_iterator i = hosts.begin(); i != hosts.end(); i++) {
		sf::HostId id (i->first);
		HostItemMap::iterator j = mHosts.find (id);
		if (j == mHosts.end()) {
			// Generate a new Host
			HostItem * item = new HostItem (i->second);
			mHosts[id] = item;
			appendRow (item);
			changed = true;
		} else {
			// Update a host
			HostItem * item = j->second;
			item->set (i->second);
		}
	}
	{
		HostItemMap::iterator i = mHosts.begin();
		while (i != mHosts.end()){
			if (hosts.count(i->first) == 0) { 
				// remove host
				removeRow (i->second->row()); // will delete all descendants by Qt
				mHosts.erase(i++);
				changed = true;
			} else {
				i++;
			}
		}
	}
	if (changed)
		updatePresentation ();
}

void UserItem::updatePresentation () {
	setText (::formatUserName(mUserInfo.userId, mUserInfo.name));

	if (mUserInfo.error) {
		setIcon (QIcon(":/error.png"));
	} else if (mUserInfo.waitForSubscription){
		setIcon (QIcon(":/pending.png"));
	} else if (!mHosts.empty()) {
		/* there are shown hosts, so mark as online!*/
		setIcon (QIcon(":/online.png"));
	} else
		setIcon (QIcon(":/offline.png"));
}

}
