#ifndef LOGGER_H
#define LOGGER_H

#include <QWidget>
#include <QTextEdit>
#include <QPointer>

class Logger
{
public:
	static void Setup(QWidget* parent);
	static void logging(QtMsgType type, const QMessageLogContext &context, const QString &msg);
	static void logit(const QString& msg);

private:
	static QPointer<QTextEdit> m_log;

};

#endif