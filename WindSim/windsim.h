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

private:
	Ui::WindSimClass ui;

};

#endif // WINDSIM_H
