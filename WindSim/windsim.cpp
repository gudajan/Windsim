#include "windsim.h"
#include "logger.h"
#include "settings.h"

#include <QKeyEvent>
#include <QCoreApplication>
#include <QDir>

WindSim::WindSim(QWidget *parent)
	: QMainWindow(parent),
	m_iniFilePath(QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini")))
{
	ui.setupUi(this);

	Logger::Setup(ui.splitter);

	reloadIni();

}

WindSim::~WindSim()
{

}

void WindSim::cleanUp()
{
	ui.dx11Viewer->cleanUp();
}

void WindSim::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F5)
	{
		reloadIni();
	}
	return QMainWindow::keyPressEvent(event);
}

void WindSim::reloadIni()
{
	try
	{
		loadIni(m_iniFilePath.toStdString());
		Logger::logit("INFO: Reloaded settings from ini-file '" + m_iniFilePath + "'.");
	}
	catch (const std::runtime_error& re)
	{
		Logger::logit("INFO: Could not open ini-file '" + m_iniFilePath + "'. Settings not reloaded.");
	}
}
