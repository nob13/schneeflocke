#include "TransferList.h"
#include "../types.h"
#include <schnee/tools/Log.h>

const int TransferList::ColumnCount = 8;

TransferList::TransferList (bool outgoing) {
	mOutgoing = outgoing;
}

TransferList::~TransferList () {

}

TransferList::TransferInfo TransferList::infoAt (const QModelIndex & index) const {
	int r = index.row ();
	if (r < 0 || (size_t)r >= mOpIds.size()) {
		sf::Log (LogWarning) << LOGID << "Strange row " << r << std::endl;
		return TransferInfo ();
	}
	return mTransfers.find(mOpIds[r])->second;
}

sf::AsyncOpId TransferList::idAt (const QModelIndex & index) const {
	int r = index.row ();
	if (r < 0 || (size_t)r >= mOpIds.size()) {
		sf::Log (LogWarning) << LOGID << "Strange row " << r << std::endl;
		return 0;
	}
	return mOpIds[r];
}

void TransferList::onUpdatedTransfer (AsyncOpId id, TransferUpdateType type, const TransferInfo & t) {
	switch (type){
	case sf::TransferInfo::Added:
		assert (mTransfers.count(id) == 0);
		mTransfers[id] = t;
		rebuild ();
		break;
	case sf::TransferInfo::Removed:
		assert (mTransfers.count(id) > 0);
		mTransfers.erase(id);
		rebuild ();
		break;
	case sf::TransferInfo::Changed:{
		mTransfers[id] = t;
		assert (mRows.count(id) > 0);
		int row = mRows[id];
		emit (dataChanged (index (row, 0), index (row, ColumnCount)));
		break;
	}
	default:
		assert (false);
		break;
	}
}

QVariant TransferList::headerData ( int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole) return QVariant();
	if (orientation == Qt::Horizontal){
		if (section == 0) return QVariant (tr("URI"));
		if (section == 1) return QVariant (tr("Filename"));
		if (section == 2) {
			if (mOutgoing)
				return QVariant (tr ("Receiver"));
			else
				return QVariant (tr ("Source"));
		}
		if (section == 3) return QVariant (tr("Size"));
		if (section == 4) return QVariant (tr("Progress"));
		if (section == 5) return QVariant (tr("Speed"));
		if (section == 6) return QVariant (tr("Time"));
		if (section == 7) return QVariant (tr("State"));
		return QVariant ();
	}
	return QVariant ();
}

int TransferList::rowCount (const QModelIndex & parent) const {
	if (parent.isValid()) return 0;
	return (int) mTransfers.size ();
}

int TransferList::columnCount (const QModelIndex & parent) const {
	if (parent.isValid()) return 0;
	return ColumnCount;
}

static QString formatSize (float f) {
	if (f <= 0.1) return "0b";
	if (f < 1000.0f) return QString::number (f) + "B";
	if (f < 2000000.0f) return QString::number (f / 1024.0f) + "KiB";
	return QString::number (f / (1024.0f * 1024.0f)) + "MiB";
}

/// Formats a byte / second value
static QString formatSpeed (float f){
	return formatSize (f) + "/s";
}

/// Formats a time interval of s seconds
static QString formatInterval (int s) {
	if (s <= 0) return QObject::tr ("unknown");
	if (s <= 90) return QString::number (s) + "s";
	if (s <= 4000) {
		int min = (s / 60);
		int sec = s - min * 60;
		return QString::number(min) + "min " + QString::number(sec) + "s";
	}
	int hours   = s / 3600;
	int minuts  = (s % 3600) / 60;
	// int seconds = (s % 3600) % 60;
	return QString::number (hours) + "h " + QString::number(minuts) + "min ";
}

QVariant TransferList::data (const QModelIndex & index, int role) const {
	if (role != Qt::DisplayRole) return QVariant();
	int r = index.row ();
	if (r < 0 || (size_t)r >= mOpIds.size()) {
		sf::Log (LogWarning) << LOGID << "Strange row " << r << std::endl;
		return QVariant ();
	}
	AsyncOpId id = mOpIds[r];
	assert (mTransfers.count(id) > 0);
	const TransferInfo & t = mTransfers.find(id)->second;
	int c = index.column ();
	switch (c) {
	case 0:
		return QVariant (qtString (sf::toString (t.uri)));
		break;
	case 1:
		return QVariant (qtString (t.filename));
		break;
	case 2:
		if (mOutgoing)
			return QVariant (qtString (t.receiver));
		else
			return QVariant (qtString (t.source));
	break;
	case 3:
		return QVariant (formatSize (t.size));
		break;
	case 4:
		return QVariant (((float)t.transferred / (float)t.size)); // [0 .. 1]
		break;
	case 5:{
		return QVariant (formatSpeed (t.speed));
		break;
	}
	case 6:{
		int seconds = (int) (t.size / t.speed);
		return QVariant (formatInterval (seconds));
		break;
	}
	case 7:
		if (t.state == sf::TransferInfo::ERROR){
			return QVariant ("Error: " + QString (toString (t.error)));
		}
		return QVariant (tr(toString (t.state)));
		break;
	}
	return QVariant ();
}

void TransferList::rebuild () {
	mOpIds.clear();
	mRows.clear ();
	int i = 0;
	mOpIds.resize (mTransfers.size());
	for (TransferMap::const_iterator j = mTransfers.begin(); j != mTransfers.end(); j++){
		AsyncOpId id = j->first;
		mOpIds[i] = id;
		mRows[id] = i;
		i++;
	}
	// will let the views change everything
	emit (layoutChanged());
	emit (dataChanged (index(0,0), index(mTransfers.size(), columnCount(QModelIndex()))));
}
