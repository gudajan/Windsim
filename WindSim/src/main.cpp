#include "GUI\windsim.h"

#include <QtWidgets/QApplication>
#include <QMetaType>
#include <QJsonObject>

#include <unordered_set>
#include <vector>

int main(int argc, char *argv[])
{
#if defined(DEBUG) | defined(_DEBUG)
	// Enable run-time memory check for debug builds.
	// (on program exit, memory leaks are printed to Visual Studio's Output console)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Install types for enabling usage with signals/slots
	qRegisterMetaType<ObjectType>();
	qRegisterMetaType <std::unordered_set<int>>();
	qRegisterMetaType <std::vector<QJsonObject>>();

	QApplication a(argc, argv);
	WindSim w;
	w.show();

	return a.exec();
}
