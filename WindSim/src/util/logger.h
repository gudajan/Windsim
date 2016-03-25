#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>

class Logger : public QObject
{
	Q_OBJECT
public:
	// Write message to message log
	void logit(const QString& msg);
signals:
	// Signal to message log
	void log(const QString& msg);
};
#endif