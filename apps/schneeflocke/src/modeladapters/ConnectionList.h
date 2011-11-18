#pragma once
#include <QAbstractTableModel>
#include <QTimer>
#include "../types.h"
#include <schnee/p2p/ConnectionManagement.h>
#include "../Controller.h"

/// Implements QAbstractTablemodel for the list of current connections
/// to other peers
class ConnectionList : public QAbstractTableModel {
public:
	typedef sf::ConnectionManagement::ConnectionInfo ConnectionInfo;
	typedef sf::ConnectionManagement::ConnectionInfos ConnectionInfos;

	ConnectionList (const Controller * controller);
	virtual ~ConnectionList ();

	/// Connections have changed
	void update ();

	// Implementation of QAbstractModel
	QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
	int rowCount (const QModelIndex & parent) const;
	int columnCount (const QModelIndex & parent) const;
	QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const;
private:
	// overwrite
	virtual void timerEvent (QTimerEvent * event);
	/// checks accuracy and reloads
	void checkAccuracy () const;
	mutable bool mOutdated;
	const Controller * mController;
	mutable ConnectionInfos mInfos;
};
