#pragma once
#include <QStandardItemModel>
#include "../../types.h"
#include <flocke/sharedlists/SharedList.h>
#include <flocke/filesharing/FileGetting.h>
class UserList;
namespace userlist {
extern const int ShareItemType;
class HostItem;

// Represents a file or directory share
class ShareItem : public QStandardItem {
public:
	typedef sf::DirectoryListing::Entry Entry;
	
	ShareItem (HostItem * hostItem, const sf::Uri & uri, const Entry & entry);
	virtual ~ShareItem ();
	
	const sf::Uri & uri () const { return mUri; }
	const Entry & entry () const { return mEntry; }
	
	bool populated() const { return mPopulated;}
	void setPopulated(bool v) { mPopulated = v; }
	
	bool isDirectory() const { return mEntry.type == sf::DirectoryListing::Directory; }
	
	void set (const sf::Uri & uri, const Entry & entry);
	int type() const { return ShareItemType; }
	
	void updateListing  (sf::Error error, const sf::DirectoryListing & listing);

	bool operator< (const QStandardItem & other) const;

	/// Send by the hostitem to invalidate mHost
	void invalidateHost () { mHost = 0; }
protected:
	void updatePresentation ();
	HostItem * mHost;
	bool mPopulated;
	sf::Uri mUri;
	Entry mEntry;
	
	typedef std::map<sf::String, ShareItem*> ChildrenMap;
	ChildrenMap mChildren;
};


}
