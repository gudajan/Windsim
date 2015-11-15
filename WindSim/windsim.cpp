#include "windsim.h"
#include "logger.h"
#include "settings.h"
#include "settingsDialog.h"
#include "commands.h"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonObject>
#include <QStandardItemModel>

WindSim::WindSim(QWidget *parent)
	: QMainWindow(parent),
	m_iniFilePath(QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath("settings.ini"))),
	m_settingsDialog(nullptr),
	m_project(),
	m_undoStack(parent)
{
	ui.setupUi(this);

	// Create Undo/Redo actions
	ui.actionUndo = m_undoStack.createUndoAction(this, tr("&Undo"));
	ui.actionUndo->setShortcuts(QKeySequence::Undo);
	ui.menuEdit->insertAction(ui.menuCreate->menuAction(), ui.actionUndo);
	ui.actionRedo = m_undoStack.createRedoAction(this, tr("&Redo"));
	ui.actionRedo->setShortcuts(QKeySequence::Redo);
	ui.menuEdit->insertAction(ui.menuCreate->menuAction(), ui.actionRedo);
	ui.menuEdit->insertSeparator(ui.menuCreate->menuAction());

	ui.objectView->setSelectionMode(QListView::ExtendedSelection); // Select multiple items (with shift and ctrl)

	Logger::Setup(ui.gbLog, ui.verticalLayout_5);

	ui.objectView->setModel(&m_project.getModel());

	// Project-actions
	connect(ui.actionNew, SIGNAL(triggered()), this, SLOT(actionNewTriggered()));
	connect(ui.actionOpen, SIGNAL(triggered()), this, SLOT(actionOpenTriggered()));
	connect(ui.actionClose, SIGNAL(triggered()), this, SLOT(actionCloseTriggered()));
	connect(ui.actionSave, SIGNAL(triggered()), this, SLOT(actionSaveTriggered()));
	connect(ui.actionSaveAs, SIGNAL(triggered()), this, SLOT(actionSaveAsTriggered()));

	// Create-actions
	connect(ui.actionCreateMesh, SIGNAL(triggered()), this, SLOT(actionCreateMeshTriggered()));

	// Settings-action
	connect(ui.actionSettings, SIGNAL(triggered()), this, SLOT(showSettingsDialog()));

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
	createActionEnable(true);

	Logger::logit("INFO: Initialized new project.");

	// Create default sky object
	actionCreateSkyTriggered("Sky");

	m_undoStack.clear();

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

	if (!m_project.open(filename))
	{
		Logger::logit("ERROR: Failed to open the project file '" + filename + "'.");
		return false;
	}

	// Add 3D objects
	const QStandardItemModel& model = m_project.getModel();
	int rc = model.rowCount();
	for (int i = 0; i < rc; ++i)
	{
		QJsonObject json = model.item(i)->data().toJsonObject();
		ui.dx11Viewer->addObject3D(json);
	}

	setWindowTitle("WindSim - " + filename);

	// Disable, enable actions
	projectActionsEnable(false, false, true, true, true);
	createActionEnable(true);

	m_undoStack.clear();
	Logger::logit("INFO: Opened project from '" + filename + "'.");

	return true;
}

void WindSim::actionCloseTriggered()
{
	if (!maybeSave()) return;

	m_project.close();
	ui.dx11Viewer->removeAllObject3D();

	setWindowTitle("WindSim");

	// Disable, enable actions
	projectActionsEnable(true, true, false, false, false);
	createActionEnable(false);

	m_undoStack.clear();
	Logger::logit("INFO: Closed project.");
}

bool WindSim::actionSaveTriggered()
{
	if (m_project.hasFilename())
	{
		if (!m_project.save())
		{
			Logger::logit("ERROR: Failed to save project to '" + m_project.getFilename() +"'.");
			return false;
		}
		m_undoStack.setClean();
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

	if (!m_project.saveAs(filename))
	{
		Logger::logit("ERROR: Failed to save project to '" + filename + "'.");
		return false;
	}

	m_undoStack.setClean();
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
		{ "obj-file", filename }
	};

	QUndoCommand* addCmd = new AddObjectCmd(json, &m_project, ui.dx11Viewer);
	m_undoStack.push(addCmd);

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

	QUndoCommand* addCmd = new AddObjectCmd(json, &m_project, ui.dx11Viewer);
	m_undoStack.push(addCmd);

	Logger::logit("INFO: Created new sky '" + name + "'.");

	return true;
}

void WindSim::showSettingsDialog()
{
	if (!m_settingsDialog)
	{
		m_settingsDialog = new SettingsDialog(this);
		connect(m_settingsDialog, SIGNAL(settingsChanged()), this, SLOT(applySettings()));
		connect(this, SIGNAL(settingsChanged()), m_settingsDialog, SLOT(updateSettings()));
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
	if (!m_undoStack.isClean())
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
		if (nameAvailable(name))
		{
			break;
		}
		else
		{
			QMessageBox::warning(this, tr("Invalid name"), tr(qPrintable("The name '" + name + "' is already taken. Please reenter an available name.")), QMessageBox::Ok);
		}
	}
	return name;
}

bool WindSim::nameAvailable(const QString& name)
{
	// TODO: iterate model and check for name
	return true;
}

void WindSim::projectActionsEnable(bool newAct, bool open, bool close, bool save, bool saveAs)
{
	ui.actionNew->setEnabled(newAct);
	ui.actionOpen->setEnabled(open);
	ui.actionClose->setEnabled(close);
	ui.actionSave->setEnabled(save);
	ui.actionSaveAs->setEnabled(saveAs);
}

void WindSim::createActionEnable(bool mesh)
{
	ui.actionCreateMesh->setEnabled(mesh);
}
