#pragma once
#include <QStandardItemModel>
#include "../../types.h"
#include <schnee/p2p/InterplexBeacon.h>
#include <flocke/sharedlists/SharedList.h>
#include "LoadingItem.h"

namespace userlist {
class ShareItem;

extern const int HostItemType;      // == 1002

// Item used for Host representation
class HostItem : public QStandardItem {
public:
	typedef sf::PresenceManagement::HostInfo HostInfo;
	HostItem (const HostInfo & info);
	virtual ~HostItem();
	
	// overriding standard item
	virtual int type () const { return HostItemType; }
	
	void set (const HostInfo & info);
	const HostInfo & info () const { return mInfo;}
	
	/// Item was yet populated
	bool populated () const { return mPopulated; }
	
	/// populated status
	void setPopulated (bool v) { mPopulated = v; }
	
	void updateSharedList (const sf::SharedList & list);

	void lostSharedList ();

	/// Tries to find a share item
	ShareItem * findShareItem (const sf::Path & path);

	/// Mark item as loading
	void setConnecting ();
	/// Clear all elements
	void clear ();
private:
	friend class ShareItem;
	void registerMe (ShareItem * item, const sf::Uri & uri) { 
		mAllShareItems[uri] = item;
	}
	void unregisterMe (ShareItem * item, const sf::Uri & uri) {
		mAllShareItems.erase(uri);
	}
	
	typedef std::map<sf::String, ShareItem*> ShareItems;
	typedef std::map<sf::Uri, ShareItem*> ShareItemsByPath; // share ordered by the head of the path
	
	/// Updates visual presentation (note, you have to call it after all changes)
	void updatePresentation ();
	HostInfo mInfo;
	bool mPopulated;
	
	ShareItems mShareItems;					///< Direct descendants
	ShareItemsByPath mAllShareItems;		///< All known share items
	bool mConnecting;
};


}
