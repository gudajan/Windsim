#include "windsim.h"
#include "logger.h"

WindSim::WindSim(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	Logger::Setup(ui.splitter);

}

WindSim::~WindSim()
{

}

void WindSim::cleanUp()
{
	ui.dx11Viewer->cleanUp();
}
