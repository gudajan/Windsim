#include "windsim.h"
#include "logger.h"
#include "settings.h"
#include "settingsDialog.h"
#include "meshProperties.h"
#include "commands.h"
#include "objectItem.h"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardItemModel>

QUndoStack g_undoStack;

WindSim::WindSim(QWidget *parent)
	: QMainWindow(parent),
	m_iniFilePath(QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"))),
	m_settingsDialog(nullptr),
	m_container(parent),
	m_project()
{
	ui.setupUi(this);

	// Create Undo/Redo actions
	ui.actionUndo = g_undoStack.createUndoAction(this, tr("&Undo"));
	ui.actionUndo->setShortcuts(QKeySequence::Undo);
	ui.menuEdit->insertAction(ui.menuCreate->menuAction(), ui.actionUndo);
	ui.actionRedo = g_undoStack.createRedoAction(this, tr("&Redo"));
	ui.actionRedo->setShortcuts(QKeySequence::Redo);
	ui.menuEdit->insertAction(ui.menuCreate->menuAction(), ui.actionRedo);
	ui.menuEdit->insertSeparator(ui.menuCreate->menuAction());

	Logger::Setup(ui.gbLog, ui.verticalLayout_5);

	ui.objectView->setModel(&m_container.getModel());
	ui.objectView->setSelectionMode(QListView::ExtendedSelection); // Select multiple items (with shift and ctrl)

	m_container.setRenderer(ui.dx11Viewer->getRenderer()); // Set renderer for object container, so it can propagate changes directly to the renderer


	// Project-actions
	connect(ui.actionNew, SIGNAL(triggered()), this, SLOT(actionNewTriggered()));
	connect(ui.actionOpen, SIGNAL(triggered()), this, SLOT(actionOpenTriggered()));
	connect(ui.actionClose, SIGNAL(triggered()), this, SLOT(actionCloseTriggered()));
	connect(ui.actionSave, SIGNAL(triggered()), this, SLOT(actionSaveTriggered()));
	connect(ui.actionSaveAs, SIGNAL(triggered()), this, SLOT(actionSaveAsTriggered()));

	// Create-actions
	connect(ui.actionCreateMesh, SIGNAL(triggered()), this, SLOT(actionCreateMeshTriggered()));
	connect(ui.actionCreateSky, SIGNAL(triggered()), this, SLOT(actionCreateSkyTriggered()));

	// Dialog actions
	connect(ui.actionSettings, SIGNAL(triggered()), this, SLOT(showSettingsDialog()));
	connect(ui.objectView, SIGNAL(activated(const QModelIndex&)), &m_container, SLOT(showPropertiesDialog(const QModelIndex&)));



	reloadIni();
}

WindSim::~WindSim()
{

}

void WindSim::closeEvent(QCloseEvent* event)
{
	if (maybeSave())
	{
		ui.dx11Viewer->cleanUp();
		g_undoStack.clear();
		event->accept();
	}
	else
	{
		event->ignore();
	}
}

void WindSim::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F5)
	{
		reloadIni();
	}
	else if (event->key() == Qt::Key_F4)
	{
		ui.dx11Viewer->reloadShaders();
	}
	return QMainWindow::keyPressEvent(event);
}

bool WindSim::actionNewTriggered()
{
	if (!m_project.isEmpty())
	{
		QMessageBox::warning(this, tr("Warning"), tr("A project is already loaded. Please close the current project before creating a new one."), QMessageBox::Ok);
		return false;
	}

	if(!m_project.create()) return false;

	setWindowTitle("WindSim - Unnamed");

	// Disable, enable actions
	projectActionsEnable(false, false, true, true, true);
	createActionEnable(true, true);

	Logger::logit("INFO: Initialized new project.");

	// Create default sky object
	actionCreateSkyTriggered("Sky");

	g_undoStack.clear();

	return true;
}

bool WindSim::actionOpenTriggered()
{
	if (!m_project.isEmpty())
	{
		QMessageBox::warning(this, tr("Warning"), tr("A project is already opened. Please close the current project before opening another one."), QMessageBox::Ok);
		return false;
	}

	QString filename = QFileDialog::getOpenFileName(this, tr("Open WindSim project"), QCoreApplication::applicationDirPath(), tr("WinSim Projects (*.wsp)"));

	if (filename.isEmpty()) return false;

	if (!m_project.open(m_container, filename))
	{
		Logger::logit("ERROR: Failed to open the project file '" + filename + "'.");
		return false;
	}
	// 3D objects are automatically created via signal/slot rowsInserted()/objectsInserted()

	setWindowTitle("WindSim - " + filename);

	// Disable, enable actions
	projectActionsEnable(false, false, true, true, true);
	createActionEnable(true, true);

	g_undoStack.clear();
	Logger::logit("INFO: Opened project from '" + filename + "'.");

	return true;
}

void WindSim::actionCloseTriggered()
{
	if (!maybeSave()) return;

	m_project.close(m_container);

	setWindowTitle("WindSim");

	// Disable, enable actions
	projectActionsEnable(true, true, false, false, false);
	createActionEnable(false, false);

	g_undoStack.clear();
	Logger::logit("INFO: Closed project.");
}

bool WindSim::actionSaveTriggered()
{
	if (m_project.hasFilename())
	{
		if (!m_project.save(m_container))
		{
			Logger::logit("ERROR: Failed to save project to '" + m_project.getFilename() +"'.");
			return false;
		}
		g_undoStack.setClean();
		Logger::logit("INFO: Saved project to '" + m_project.getFilename() + "'.");
		return true;
	}
	return actionSaveAsTriggered();
}

bool WindSim::actionSaveAsTriggered()
{
	QString filename = QFileDialog::getSaveFileName(this, tr("Save WindSim project"), QCoreApplication::applicationDirPath(), tr("WinSim Projects (*.wsp)"));

	if (filename.isEmpty())
		return false;

	if (!m_project.saveAs(m_container, filename))
	{
		Logger::logit("ERROR: Failed to save project to '" + filename + "'.");
		return false;
	}

	g_undoStack.setClean();
	Logger::logit("INFO: Saved project to '" + filename + "'.");
	return true;
}

bool WindSim::actionCreateMeshTriggered()
{
	QString name = getName("New Mesh", "Mesh name:", "Mesh");
	if (name.isEmpty())
		return false;

	QString filename = QFileDialog::getOpenFileName(this, tr("Choose Mesh"), QCoreApplication::applicationDirPath(), tr("Wavefront OBJ (*.obj)"));

	if (filename.isEmpty()) return false; // Empty filename -> Cancel -> abort mesh creation

	QJsonObject json
	{
		{ "name", name },
		{ "type", QString::fromStdString(objectTypeToString(ObjectType::Mesh)) },
		{ "obj-file", filename },
	};
	m_container.addCmd(json);

	Logger::logit("INFO: Created new mesh '" + name + "' from OBJ-file '" + filename + "'.");

	return true;
}

bool WindSim::actionCreateSkyTriggered(QString name)
{
	if (name.isEmpty())
	{
		name = getName("New Sky", "Sky name:", "Sky");
		if (name.isEmpty())
			return false;
	}

	QJsonObject json
	{
		{ "name", name },
		{ "type", QString::fromStdString(objectTypeToString(ObjectType::Sky))}
	};
	m_container.addCmd(json);

	Logger::logit("INFO: Created new sky '" + name + "'.");

	return true;
}

void WindSim::showSettingsDialog()
{
	if (!m_settingsDialog)
	{
		m_settingsDialog = new SettingsDialog(this);
		m_settingsDialog->setup(this);
	}

	m_settingsDialog->show();
	m_settingsDialog->raise();
}

void WindSim::applySettings()
{
	ui.dx11Viewer->applySettings();
}

void WindSim::reloadIni()
{
	try
	{
		loadIni(m_iniFilePath.toStdString());
		applySettings();
		Logger::logit("INFO: Reloaded settings from ini-file '" + m_iniFilePath + "'.");
		emit settingsChanged();
	}
	catch (const std::runtime_error& re)
	{
		Logger::logit("INFO: Could not open ini-file '" + m_iniFilePath + "'. Settings not reloaded.");
	}
}

bool WindSim::maybeSave()
{
	if (!g_undoStack.isClean())
	{
		QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("WindSim"), tr("The project has been modified.\nDo you want to save your changes?"),
			QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		if (ret == QMessageBox::Save)
		{
			if (m_project.hasFilename())
				return actionSaveTriggered();
			else
				return actionSaveAsTriggered();
		}
		else if (ret == QMessageBox::Cancel)
		{
			return false;
		}
	}
	return true;
}

QString WindSim::getName(const QString& title, const QString& label, const QString& defaultName)
{
	// TODO: Iterate object names and get first available name (defaultName1, defaultName2 etc) as default
	bool ok = true;
	QString name;
	while (true)
	{
		name = QInputDialog::getText(this, tr(qPrintable(title)), tr(qPrintable(label)), QLineEdit::Normal, defaultName, &ok);
		if (!ok) return QString(); // If pressed cancel -> abort object creation
		break;
	}
	return name;
}

void WindSim::projectActionsEnable(bool newAct, bool open, bool close, bool save, bool saveAs)
{
	ui.actionNew->setEnabled(newAct);
	ui.actionOpen->setEnabled(open);
	ui.actionClose->setEnabled(close);
	ui.actionSave->setEnabled(save);
	ui.actionSaveAs->setEnabled(saveAs);
}

void WindSim::createActionEnable(bool mesh, bool sky)
{
	ui.actionCreateMesh->setEnabled(mesh);
	ui.actionCreateSky->setEnabled(sky);
}
