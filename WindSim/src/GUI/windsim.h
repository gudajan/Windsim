#ifndef WINDSIM_H
#define WINDSIM_H

#include <QMainWindow>
#include <QPointer>
#include <QTextEdit>
#include <QUndoStack>

#include "ui_windsim.h"

#include "project.h"
#include "objectContainer.h"

class SettingsDialog;

#include <QUndoView>

class WindSim : public QMainWindow
{
	Q_OBJECT

public:
	WindSim(QWidget *parent = 0);
	~WindSim();

protected:
	void keyPressEvent(QKeyEvent * event) override;
	void closeEvent(QCloseEvent* event) override;

private slots:
	// Project actions:
	bool actionNewTriggered();
	bool actionOpenTriggered();
	void actionCloseTriggered();
	bool actionSaveTriggered();
	bool actionSaveAsTriggered();

	// Create actions:
	bool actionCreateMeshTriggered();
	bool actionCreateVoxelGridTriggered();
	bool actionCreateSkyTriggered(QString name = QString());
	bool actionCreateAxesTriggered(QString name = QString());
	// void actionRemoveObjectTriggered();

	// Propagate changes from the 3D view to the object view (set -> QItemSelection); sets selection via QItemSelectionModel
	void on3DSelectionChanged(std::unordered_set<int> ids);
	// Propagate changes from the GUI object view to 3D (QItemSelection -> set); emits selectionChanged to 3D view
	void onGUISelectionChanged();

	// Dialog actions:
	void showSettingsDialog();

	void applySettings(); // Settings Dialog connects to this

	void onUpdateFPS(int fps);

signals:
	void settingsChanged();
	void selectionChanged(std::unordered_set<int> ids); // Propagate changes from the GUI to the 3D View
	void startRendering();
	void pauseRendering();

private:
	void reloadIni();
	bool maybeSave();
	QString getName(const QString& title, const QString& label, const QString& defaultName);

	void projectActionsEnable(bool newAct, bool open, bool close, bool save, bool saveAs);
	void createActionEnable(bool mesh, bool grid);

	Ui::WindSimClass ui;

	QString m_iniFilePath;

	QPointer<SettingsDialog> m_settingsDialog;
	ObjectContainer m_container;
	Project m_project;

	//DEBUG
	std::shared_ptr<QUndoView> undoView;

};

#endif // WINDSIM_H
