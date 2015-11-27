#ifndef LOGGER_H
#define LOGGER_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPointer>

class QLayout;

class Logger
{
public:
	static void Setup(QWidget* parent, QLayout* layout);
	static void logging(QtMsgType type, const QMessageLogContext &context, const QString &msg);
	static void logit(const QString& msg);

private:
	static QPointer<QPlainTextEdit> m_log;

};

#endif