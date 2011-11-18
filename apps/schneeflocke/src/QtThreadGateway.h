#pragma once
#include <QObject>
#include <boost/function.hpp>

/**
 * Helper base class which executes functions
 * in the Qt GUI thread (when your GUI code derives from this class)
 */
class QtThreadGateway : public QObject {
	Q_OBJECT
public:
	typedef boost::function<void()> VoidFunction;

	QtThreadGateway (QObject * parent = 0);

	/// Executes a function in the Qt-Thread bound to Object
	void call (const VoidFunction & func);

signals:
	void forwardLoadToQt (const VoidFunction &);

private slots:
	void onNewLoad (const VoidFunction &);

protected:
	/// Class is about to quit; do not forward any load anymore
	void aboutToQuit () { mAboutToQuit = true;}
private:
	bool mAboutToQuit;
};
