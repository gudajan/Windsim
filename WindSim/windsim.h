#ifndef WINDSIM_H
#define WINDSIM_H

#include <QtWidgets/QMainWindow>
#include "ui_windsim.h"

class WindSim : public QMainWindow
{
	Q_OBJECT

public:
	WindSim(QWidget *parent = 0);
	~WindSim();

private:
	Ui::WindSimClass ui;
};

#endif // WINDSIM_H
