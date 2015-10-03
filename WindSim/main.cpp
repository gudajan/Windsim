#include "windsim.h"
#include "logger.h"
#include <QtWidgets/QApplication>


int main(int argc, char *argv[])
{
	// Install own logging method
	qInstallMessageHandler(Logger::logging);

	QApplication a(argc, argv);
	WindSim w;
	w.show();
	return a.exec();
}
