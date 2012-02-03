#include "QtThreadGateway.h"
#include <QMetaType>

QtThreadGateway::QtThreadGateway (QObject * parent) : QObject (parent) {
	qRegisterMetaType<VoidFunction> ("VoidFunction");
	QObject::connect (this, SIGNAL (forwardLoadToQt(const VoidFunction&)), this, SLOT (onNewLoad (const VoidFunction &)), Qt::QueuedConnection);
	mAboutToQuit = false;
}


void QtThreadGateway::call(const boost::function<void ()> & func) {
	emit (forwardLoadToQt (func));
}

void QtThreadGateway::onNewLoad (const VoidFunction & func) {
	if (!mAboutToQuit) func ();
}
