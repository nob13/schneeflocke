#include "UserList.h"

#include <schnee/tools/Log.h>
#include <schnee/tools/FileTools.h>

using namespace userlist;

UserList::UserList (Controller * controller){
	mController = controller;
	mNoUserItem = 0;
}

UserList::~UserList () {
	clear ();
}

void UserList::rebuildList () {
	clear ();
	onPeersChanged ();
}

void UserList::onPeersChanged () {
	SF_SCHNEE_LOCK;
	const sf::InterplexBeacon * beacon = mController->umodel()->beacon();
	UserInfoMap users = beacon->presences().users();

	QStringList labels;
	labels << "Users";
	setHorizontalHeaderLabels (labels);
	
	for (UserInfoMap::const_iterator i = users.begin(); i != users.end(); i++) {
		const sf::UserId & userId (i->first);
		const UserInfo & userInfo (i->second);
		UserItemMap::iterator j = mUserItems.find (userId);
		
		UserItem * item = 0;
		if (j == mUserItems.end()){
			// new user item
			item = new UserItem (userInfo);
			mUserItems[userId] = item;
			appendRow (item);
		} else {
			// Updated item
			item = j->second;
			item->set(userInfo);
		}
		HostInfoMap hosts = beacon->presences().hosts (userId);
		filterHosts (&hosts);
		item->onUpdateHosts (hosts);
	}
	// removing now nonexistant users
	UserItemMap::iterator i = mUserItems.begin();
	while (i != mUserItems.end()) {
		if (users.count(i->first) == 0){
			UserItem * item = i->second;
			removeRow (item->row()); // qt will delete items
			mUserItems.erase(i++);
			continue;
		} else {
			i++;
		}
	}

	if (users.empty()){
		if (!mNoUserItem) {
			mNoUserItem = new QStandardItem ();
			mNoUserItem->setText(tr ("No contacts"));
			appendRow (mNoUserItem);
		}
	} else {
		if (mNoUserItem) {
			removeRow (mNoUserItem->row()); // will be deleted by Qt
			mNoUserItem = 0;
		}
	}
}

void UserList::onTrackingUpdate (const sf::HostId & hostId, const sf::SharedList & list) {
	HostItem * item = findHostItem (hostId);
	if (!item) return;
	item->updateSharedList(list);
}

void UserList::onLostTracking (const sf::UserId & hostId) {
	HostItem * item = findHostItem (hostId);
	if (!item) return;
	item->lostSharedList();
}

void UserList::onUpdatedListing  (const sf::Uri & uri, sf::Error error, const sf::DirectoryListing & listing) {
	ShareItem * item = findShareItem (uri);
	if (!item) {
		sf::Log(LogInfo) << LOGID << "Did not find item for  " << uri << std::endl;
		return;
	}
	item->updateListing(error,listing);
}

void UserList::setFeatureFilter (const sf::String & filter) {
	mFeatureFilter = filter;
}

userlist::UserItem * UserList::userItem (const QModelIndex & index) {
	if (!index.isValid()) return 0;
	QStandardItem * item = itemFromIndex (index);
	if (item->type() == userlist::UserItemType) {
		return static_cast<userlist::UserItem*> (item);
	}
	return 0;
}

HostItem * UserList::findHostItem (const sf::HostId & id) {
	HostInfo info;
	{
		SF_SCHNEE_LOCK;
		info = mController->umodel()->beacon()->presences().hostInfo (id);
	}
	if (info.userId.empty()) return 0; // does not exist (anymore?)
	
	UserItemMap::iterator i = mUserItems.find (info.userId);
	if (i == mUserItems.end()) return 0; // may also not exist anymore
	
	UserItem * item = i->second;
	return item->hostItem (id);
}

userlist::ShareItem * UserList::findShareItem (const sf::Uri & uri) {
	HostItem * item = findHostItem (uri.host());
	if (!item) return 0;
	
	return item->findShareItem (uri.path());	
}

void UserList::clear () {
	// Qt will delete all added userItems
	mUserItems.clear();
	QStandardItemModel::clear ();
	mNoUserItem = 0;
}

bool UserList::hasChildren (const QModelIndex & index) const {
	/*
	 * Displaying hasChildren for all not-populated DirectoryItems
	 */
	QStandardItem * item = itemFromIndex (index);
	if (item && item->type() == ShareItemType){
		ShareItem * shareItem = static_cast<ShareItem*> (item);
		if (shareItem->isDirectory() && !shareItem->populated()) return true;
	}
	if (item && item->type() == HostItemType){
		HostItem * hostItem = static_cast<HostItem*> (item);
		if (!hostItem->populated()) return true;
	}
	return QStandardItemModel::hasChildren (index);
}

int UserList::rowCount (const QModelIndex & index) const {
	/*
	 * Populate non-populated Directory Items
	 */
	QStandardItem * item = itemFromIndex (index);
	if (item && item->type () == ShareItemType){
		ShareItem * shareItem = static_cast<ShareItem*> (item);
		if (shareItem->isDirectory()){
			if (!shareItem->populated()) {
				shareItem->setPopulated (true); // must done before; appendRow will call rowCount()
				shareItem->appendRow (new LoadingItem());
				mController->listDirectory (shareItem->uri());
			}
		}
	}
	if (item && item->type() == HostItemType) {
		HostItem * hostItem = static_cast<HostItem*> (item);
		if (!hostItem->populated()){
			sf::HostId id = hostItem->info().hostId;
			sf::Error e = mController->trackHostShared (id);
			if (!e) {
				hostItem->setPopulated (true);
				if (!mController->isConnectedTo(id)){
					hostItem->setConnecting();
				}
			}
		}
	}
	return QStandardItemModel::rowCount (index);
}

/// checks if vector contains an element v
/// O(n), but shall be ok on small vectors.
static bool contains (const std::vector<sf::String> & vector, const sf::String & v) {
	for (std::vector<sf::String>::const_iterator i = vector.begin(); i != vector.end(); i++) {
		if (*i == v) return true;
	}
	return false;
}

void UserList::filterHosts (HostInfoMap * hosts) {
	assert (hosts);
	if (!mFeatureFilter.empty()){
		HostInfoMap::iterator i = hosts->begin();
		while (i != hosts->end()){
			if (!contains (i->second.features, mFeatureFilter)){
				hosts->erase(i++);
			} else {
				i++;
			}
		}
	}
}




