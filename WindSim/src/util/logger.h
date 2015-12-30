#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>

class Logger : public QObject
{
	Q_OBJECT
signals:
	void logit(const QString& msg);
};
#endif