#pragma once

#include <QStandardItemModel>

#include <flocke/filesharing/FileGetting.h>

#include "../types.h"
#include "../Controller.h"

#include <map>

#include "userlist/UserItem.h"
#include "userlist/HostItem.h"
#include "userlist/ShareItem.h"
#include "userlist/LoadingItem.h"

/// Online users (and sharers) List
class UserList : public QStandardItemModel {
public:
	UserList (Controller * );
	virtual ~UserList ();

	// Rebuild Qt presentation of online users and shared files
	void rebuildList();

	/// Peers somehow changed
	void onPeersChanged ();
	/// Updated Shared List for a peer
	void onTrackingUpdate (const sf::HostId & hostId, const sf::SharedList & list);
	/// Lost Tracking to someone
	void onLostTracking (const sf::HostId & user);
	/// Updated directory listing
	void onUpdatedListing  (const sf::Uri & uri, sf::Error error, const sf::DirectoryListing & listing);

	/// Only display hosts which provide a specific feature
	void setFeatureFilter (const sf::String & filter);

	// overwrite (for disabling writeability)
	virtual Qt::ItemFlags flags ( const QModelIndex & index ) const {
		return QStandardItemModel::flags(index) & ~Qt::ItemIsEditable;
	}


	/// Returns user item at specific index(or 0 if no user item is selected)
	userlist::UserItem * userItem (const QModelIndex& index);
private:
	userlist::HostItem  * findHostItem (const sf::HostId & id);
	userlist::ShareItem * findShareItem (const sf::Uri & uri);
	
	typedef sf::PresenceManagement::UserInfoMap UserInfoMap;
	typedef sf::PresenceManagement::HostInfoMap HostInfoMap;
	typedef sf::PresenceManagement::UserInfo UserInfo;
	typedef sf::PresenceManagement::HostInfo HostInfo;
	
	friend class userlist::ShareItem;
	
	// Override; clears also own data structures
	void clear ();
	/// Returns  true for Directory Items
	bool hasChildren (const QModelIndex & index) const;
	/// Automatically populates non-populated directory items
	int rowCount (const QModelIndex & index) const;

	/// Filter hosts (if feature filter is non empty)
	void filterHosts (HostInfoMap * hosts);

	typedef std::map<sf::UserId, userlist::UserItem *> UserItemMap;

	UserItemMap       mUserItems;			///< Tracked users
	QStandardItem *   mNoUserItem;			///< User Item notifying that you have no contacts

	Controller * mController;		///< Access to the controller (TODO: We need just the model)
	sf::String mFeatureFilter;
};
