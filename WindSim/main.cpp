#include "windsim.h"
#include "logger.h"
#include "common.h"

#include <QtWidgets/QApplication>
#include <QMetaType>

int main(int argc, char *argv[])
{
#if defined(DEBUG) | defined(_DEBUG)
	// Enable run-time memory check for debug builds.
	// (on program exit, memory leaks are printed to Visual Studio's Output console)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Install own logging method
	qInstallMessageHandler(Logger::logging);
	// Install ObjectType enum for enabling usage with signals/slots
	qRegisterMetaType<ObjectType>();

	QApplication a(argc, argv);
	WindSim w;
	w.show();

	return a.exec();
}
