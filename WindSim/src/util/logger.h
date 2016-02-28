#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>

class Logger : public QObject
{
	Q_OBJECT
public:
	void logit(const QString& msg);
signals:
	void log(const QString& msg);
};
#endif