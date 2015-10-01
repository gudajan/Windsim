#include "windsim.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	WindSim w;
	w.show();
	return a.exec();
}
