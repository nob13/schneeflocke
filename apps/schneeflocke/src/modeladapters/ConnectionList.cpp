#include "ConnectionList.h"

ConnectionList::ConnectionList (const Controller * controller) : QAbstractTableModel () {
	mController = controller;
	mOutdated = true;
	startTimer (1000);
}

ConnectionList::~ConnectionList () {
}

void ConnectionList::update () {
	mOutdated = true;
	emit (layoutChanged());
	emit (dataChanged (index(0,0), index(mInfos.size(), columnCount(QModelIndex()))));
}

QVariant ConnectionList::headerData ( int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole) return QVariant();
	if (orientation == Qt::Horizontal){
		if (section == 0) return QVariant (tr("Host"));
		if (section == 1) return QVariant (tr("Remote Address"));
		if (section == 2) return QVariant (tr("Local Address"));
		if (section == 3) return QVariant (tr("Type"));
		if (section == 4) return QVariant (tr("Delay"));
		if (section == 5) return QVariant (tr("Virtual"));
		if (section == 6) return QVariant (tr("Level"));
		return QVariant ();
	}
	return QVariant ();
}

int ConnectionList::rowCount (const QModelIndex & parent) const {
	checkAccuracy ();
	return (int) mInfos.size();
}

int ConnectionList::columnCount (const QModelIndex & parent) const {
	return 7;
}

static QString formatDelay (float d) {
	if (d > 1) return QString::number(d) + "s";
	if (d < 1000) return QString::number (d * 1000000) + QString::fromUtf8("µs");
	return QString::number( d * 1000) + "ms";
}

QVariant ConnectionList::data (const QModelIndex & index, int role) const {
	if (role != Qt::DisplayRole) return QVariant();
	checkAccuracy ();
	int r = index.row();
	int c = index.column();
	if (r < 0 || r >= (int) mInfos.size()) return QVariant();
	const ConnectionInfo & info = mInfos[r];
	switch (c) {
	case 0:
		return ::qtString (info.target);
	case 1:
		return ::qtString (info.cinfo.raddress);
	case 2:
		return ::qtString (info.cinfo.laddress);
	case 3:
		return ::qtString (info.stack);
	case 4:
		return info.cinfo.delay < 0 ? QVariant("unknown") : QVariant(formatDelay (info.cinfo.delay));
	case 5:
		return info.cinfo.virtual_;
	case 6:
		return info.level;
	}
	return QVariant();
}

void ConnectionList::timerEvent (QTimerEvent * event) {
	if (!mOutdated) {
		update ();
	}
}

void ConnectionList::checkAccuracy () const {
	if (mOutdated) {
		SF_SCHNEE_LOCK;
		mInfos = mController->umodel()->beacon()->connections().connections();
		mOutdated = false;
	}
}

