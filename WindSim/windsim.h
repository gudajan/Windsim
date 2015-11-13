#ifndef WINDSIM_H
#define WINDSIM_H

#include <QMainWindow>
#include <QPointer>
#include <QTextEdit>

#include "ui_windsim.h"

#include "project.h"

class SettingsDialog;


class WindSim : public QMainWindow
{
	Q_OBJECT

public:
	WindSim(QWidget *parent = 0);
	~WindSim();

protected:
	void keyPressEvent(QKeyEvent * event);
	void closeEvent(QCloseEvent* event);

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

	void applySettings();

signals:
	void settingsChanged();

private:
	void reloadIni();
	bool maybeSave();
	QString getName(const QString& title, const QString& label, const QString& defaultName);
	bool nameAvailable(const QString& name);

	void projectActionsEnable(bool newAct, bool open, bool close, bool save, bool saveAs);
	void createActionEnable(bool mesh);

	Ui::WindSimClass ui;

	QString m_iniFilePath;
	QPointer<SettingsDialog> m_settingsDialog;
	Project m_project;


};

#endif // WINDSIM_H
