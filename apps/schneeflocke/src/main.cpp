#include <iostream>

#include <QApplication>
#include <QIcon>
#include <schnee/schnee.h>
#include "Controller.h"

int main (int argc, char ** argv) {
	sf::schnee::SchneeApp schneeApp (argc, argv);
	QApplication app (argc, argv);
	QApplication::setApplicationName("schneeflocke");
	QApplication::setOrganizationName("sflx.net team");
	QApplication::setWindowIcon(QIcon (":/schneeflocke.png"));


	Controller controller;
	controller.init ();
	return app.exec ();
}

