#pragma once

#include <QStringListModel>
#include <flocke/filesharing/FileSharing.h>
#include "../Controller.h"

/// List of shares
class ShareList : public QStringListModel {
public:
	typedef sf::FileSharing::FileShareInfoVec FileShareInfovec;
	ShareList (const Controller * mController);

	// overwrite (for disabling writeability)
	virtual Qt::ItemFlags flags ( const QModelIndex & index ) const {
		return QStringListModel::flags(index) & ~Qt::ItemIsEditable;
	}

	/// Notifys the list that it is outdated.
	void update ();
private:
	const Controller * mController;

};
