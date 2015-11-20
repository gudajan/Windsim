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
	bool actionCreateSkyTriggered(QString name = QString());
	// void actionRemoveObjectTriggered();

	// Dialog actions:
	void showSettingsDialog();

	void applySettings(); // Settings Dialog connects to this

signals:
	void settingsChanged();

private:
	void reloadIni();
	bool maybeSave();
	QString getName(const QString& title, const QString& label, const QString& defaultName);

	void projectActionsEnable(bool newAct, bool open, bool close, bool save, bool saveAs);
	void createActionEnable(bool mesh, bool sky);

	Ui::WindSimClass ui;

	QString m_iniFilePath;

	QPointer<SettingsDialog> m_settingsDialog;
	ObjectContainer m_container;
	Project m_project;

};

#endif // WINDSIM_H
