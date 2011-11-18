#pragma once
#include <QAbstractTableModel>
#include <flocke/filesharing/FileGetting.h>

#include <map>

/**
 * A table model for working transfers
 *
 * Columns:
 * Uri, Size, Progress, Speed, Lasting Time, State
 */
class TransferList : public QAbstractTableModel {
public:
	static const int ColumnCount;

	typedef sf::FileGetting::TransferInfo TransferInfo;
	typedef TransferInfo::TransferUpdateType TransferUpdateType;
	typedef sf::AsyncOpId AsyncOpId;

	TransferList (bool outgoing);
	virtual ~TransferList ();

	// Gets info at a specific index
	TransferInfo infoAt (const QModelIndex & index) const;
	// Gets the transfer id of the specific index
	sf::AsyncOpId idAt (const QModelIndex & index) const;

	// Updated Transfer
	void onUpdatedTransfer (AsyncOpId id, TransferUpdateType type, const TransferInfo & t);

	// Implementation of QAbstractModel
	QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
	int rowCount (const QModelIndex & parent) const;
	int columnCount (const QModelIndex & parent) const;
	QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const;


private:
	/// Tell all views to completely rebuild the list
	void rebuild ();

	typedef std::map<AsyncOpId, TransferInfo> TransferMap;
	TransferMap mTransfers;		///< Current Transfer Map
	typedef std::vector<AsyncOpId> OpIdVec;
	typedef std::map<AsyncOpId, int> RowMap;
	OpIdVec mOpIds;				///< Maps Qt rows to Async Op Ids
	RowMap mRows;				///< Maps async op ids to Qt rows
	bool mOutgoing;				///< Outgoing or ingoing transfers
};
