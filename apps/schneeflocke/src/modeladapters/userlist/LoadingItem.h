#pragma once
#include <QStandardItemModel>

namespace userlist {

/// Identifies that Items are loading/connecting etc....
class LoadingItem : public QStandardItem {
public:
	LoadingItem (const QString & msg = QObject::tr ("Loading..."));
};


}
