#include "LoadingItem.h"
#include <QObject>
namespace userlist {

LoadingItem::LoadingItem (const QString & msg){
	setEditable (false);
	setText (msg);
}

}
