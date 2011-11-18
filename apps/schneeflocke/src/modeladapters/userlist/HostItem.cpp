#include "HostItem.h"
#include "ShareItem.h"
#include <schnee/tools/Log.h>

namespace userlist {
const int HostItemType      = 1002;

HostItem::HostItem (const HostInfo & info) {
	mInfo = info;
	mPopulated = false;
	mConnecting = false;
	updatePresentation();
}

HostItem::~HostItem (){
	for (ShareItemsByPath::iterator i = mAllShareItems.begin(); i != mAllShareItems.end(); i++) {
		i->second->invalidateHost();
	}
}

void HostItem::set (const HostInfo & info) {
	mInfo = info;
	if (mInfo.hostId != info.hostId || mInfo.name != info.name || mInfo.userId != info.userId){
		updatePresentation ();
	}
}

void HostItem::updateSharedList (const sf::SharedList & list) {
	if (mConnecting) {
		clear();
	}
	for (sf::SharedList::const_iterator i = list.begin(); i != list.end(); i++) {
		const sf::String & shareName = i->first;
		const sf::SharedElement & element = i->second;
		const sf::String& user = element.desc.user;

		sf::DirectoryListing::Entry entry; // constructing entry
		entry.name = shareName;
		entry.size = element.size;
		
		if (user == "file"){
			entry.type = sf::DirectoryListing::File;
		} else if (user == "dir") {
			entry.type = sf::DirectoryListing::Directory;
		} else {
			sf::Log (LogWarning) << LOGID << "Unknown share item type " << user << std::endl;
			continue;
		}
		
		ShareItems::iterator j = mShareItems.find(shareName);
		if (j == mShareItems.end()){
			ShareItem * item = new ShareItem (this, sf::Uri (mInfo.hostId, element.path), entry);
			appendRow (item);
			mShareItems[shareName]  = item;
		} else {
			// existing item
			ShareItem * item = j->second;
			item->set(sf::Uri (mInfo.hostId, element.path), entry);
		}
	}
	// removed items
	ShareItems::iterator i = mShareItems.begin();
	while (i != mShareItems.end()) {
		const sf::String & shareName (i->first);
		if (list.count(shareName) == 0){
			removeRow(i->second->row());
			mShareItems.erase(i++);
			continue;
		} else {
			i++;
		}
	}
	sortChildren(0);
}

void HostItem::lostSharedList () {
	clear ();
}

ShareItem * HostItem::findShareItem (const sf::Path & path) {
	sf::Uri uri (mInfo.hostId, path);
	ShareItemsByPath::iterator i = mAllShareItems.find(uri);
	if (i == mAllShareItems.end()) return 0;
	return i->second;
}

void HostItem::setConnecting () {
	mConnecting = true;
	appendRow (new LoadingItem ("Connecting..."));
}

void HostItem::clear () {
	mConnecting = false;
	for (ShareItems::iterator i = mShareItems.begin(); i != mShareItems.end(); i++) {
		removeRow (i->second->row());
	}
	while (rowCount() > 0) {
		removeRow (0);
	}
	mShareItems.clear();
}


void HostItem::updatePresentation () {
	if (!mInfo.name.empty()) setText (::qtString (mInfo.name));
	else setText (::qtString (mInfo.hostId));
	setEditable (false);

	QIcon icon (":/host.png");
	setIcon (icon);
}

}
