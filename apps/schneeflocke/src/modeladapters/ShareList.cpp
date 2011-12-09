#include "ShareList.h"
#include "../types.h"

ShareList::ShareList (const Controller * controller) {
	mController = controller;
}

void ShareList::update () {
	FileShareInfovec vec;
	{
		SF_SCHNEE_LOCK;
		vec = mController->umodel()->fileSharing()->shared();
	}
	QStringList list;
	for (FileShareInfovec::const_iterator i = vec.begin(); i != vec.end(); i++) {
		list.append(::qtString(i->shareName));
	}
	setStringList (list);
	// will let the views change everything
	emit (layoutChanged());
	emit (dataChanged (index(0,0), index(list.size(), 1)));
}

