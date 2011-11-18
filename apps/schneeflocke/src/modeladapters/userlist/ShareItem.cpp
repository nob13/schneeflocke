#include "ShareItem.h"
#include "../UserList.h"
#include <schnee/tools/Log.h>

namespace userlist {
const int ShareItemType      = 1003;


ShareItem::ShareItem (HostItem * hostItem, const sf::Uri & uri, const Entry & entry){
	mPopulated = false;

	mUri = uri;
	mEntry = entry;
	
	mHost = hostItem;
	mHost->registerMe (this, mUri);
	updatePresentation();
}

ShareItem::~ShareItem () {
	if (mHost)
		mHost->unregisterMe (this, mUri);
}

void ShareItem::set (const sf::Uri & uri, const Entry & entry) {
	if (mUri != uri) {
		if (mHost) mHost->unregisterMe (this, mUri);
		mUri = uri;
		if (mHost) mHost->registerMe (this, mUri);
	}
	if (mEntry != entry || mUri != uri) {
		mUri = uri;
		mEntry = entry;
		if (mEntry.type == sf::DirectoryListing::File){
			mPopulated = false;
		}
		updatePresentation ();
	}
}

void ShareItem::updateListing  (sf::Error error, const sf::DirectoryListing & listing) {
	while (child(0)){
		removeRow (child(0)->row());
	}
	mChildren.clear();
	
	if (error){
		// TODO: Make this nicer!
		if (error == sf::error::NoPerm){
			appendRow (new QStandardItem (QObject::tr ("No Permission")));
		} else {
			appendRow (new QStandardItem (QObject::tr ("Error: %1").arg (toString (error))));
		}
		return;
	}
	for (sf::DirectoryListing::EntryVector::const_iterator i = listing.entries.begin(); i != listing.entries.end(); i++) {
		const sf::DirectoryListing::Entry & entry (*i);
		ShareItem * element = new ShareItem (mHost, sf::Uri (mUri.host(), mUri.path() + entry.name), entry);
		mChildren[entry.name] = element;
		appendRow (element);
	}
	sortChildren(0);
}

bool ShareItem::operator< (const QStandardItem & other) const {
	if (other.type() == ShareItemType){
		const ShareItem & s (static_cast<const ShareItem&> (other));
		if (mEntry.type == sf::DirectoryListing::Directory && s.mEntry.type == sf::DirectoryListing::File) return true;
		if (mEntry.type == sf::DirectoryListing::File && s.mEntry.type == sf::DirectoryListing::Directory) return false;
		return (mEntry.name < s.mEntry.name);
	}
	return QStandardItem::operator<(other);
}


void ShareItem::updatePresentation () {
	setText (::qtString(mEntry.name));
	
	if (mEntry.type == sf::DirectoryListing::File) {
		setIcon (QIcon (":/file.png"));
	} else if (mEntry.type == sf::DirectoryListing::Directory){
		setIcon (QIcon (":/folder.png"));
	} else {
		sf::Log (LogWarning) << LOGID << "Unknown entry type: " << toString (mEntry.type) << std::endl; 
	}
}

}
