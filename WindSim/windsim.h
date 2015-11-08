#ifndef WINDSIM_H
#define WINDSIM_H

#include <QtWidgets/QMainWindow>
#include "ui_windsim.h"

#include <QPointer>
#include <QTextEdit>

class WindSim : public QMainWindow
{
	Q_OBJECT

public:
	WindSim(QWidget *parent = 0);
	~WindSim();

	static QPointer<QTextEdit> log;

public slots:
	void cleanUp();

protected:
	void keyPressEvent(QKeyEvent * event);

private:

	void reloadIni();

	Ui::WindSimClass ui;

	QString m_iniFilePath;

};

#endif // WINDSIM_H
