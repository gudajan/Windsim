#ifndef WINDSIM_H
#define WINDSIM_H

#include <QMainWindow>
#include <QPointer>
#include <QTextEdit>
#include <QStandardItemModel>

#include "ui_windsim.h"

#include "project.h"


class WindSim : public QMainWindow
{
	Q_OBJECT

public:
	WindSim(QWidget *parent = 0);
	~WindSim();

	static QPointer<QTextEdit> log;

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
	// void actionRemoveObjectTriggered();

private:
	void reloadIni();
	bool maybeSave();
	bool nameAvailable(const QString& name);

	void projectActionsEnable(bool newAct, bool open, bool close, bool save, bool saveAs);
	void createActionEnable(bool mesh);

	Ui::WindSimClass ui;

	QString m_iniFilePath;
	Project m_project;
	QStandardItemModel m_objectModel;



};

#endif // WINDSIM_H
