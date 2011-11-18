#pragma once
#include <QStandardItemModel>
#include "../../types.h"
#include <flocke/sharedlists/SharedList.h>
#include <flocke/filesharing/FileGetting.h>
#include <schnee/p2p/InterplexBeacon.h>

namespace userlist {
class HostItem;

extern const int UserItemType;  	// == 1001

// Item used for user presentation
class UserItem : public QStandardItem {
public:
	typedef sf::PresenceManagement::UserInfo UserInfo;
	typedef sf::PresenceManagement::HostInfoMap HostInfoMap;
	typedef sf::PresenceManagement::HostInfo HostInfo;
	
	UserItem (const UserInfo & info);
	
	// overriding standard item
	virtual int type () const { return UserItemType; }

	/// Set new data
	void set (const UserInfo & info);

	/// Returns user info
	const UserInfo & info () const { return mUserInfo; }
	
	/// Returns a host item for a specific host or 0 if not found
	HostItem * hostItem (const sf::HostId & hostItem);
	
	
	/// Updates all registered hosts
	void onUpdateHosts (const HostInfoMap & hosts);
	
private:
	/// Updates visual presentation (note, you have to call it after all changes!)
	void updatePresentation ();

	typedef std::map<sf::UserId, userlist::HostItem *> HostItemMap;
	HostItemMap mHosts;
	UserInfo mUserInfo;
};

}
