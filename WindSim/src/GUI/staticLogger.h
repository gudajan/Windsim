#ifndef STATIC_LOGGER_H
#define STATIC_LOGGER_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPointer>

class QLayout;

class MessageLog : public QPlainTextEdit
{
public:
	MessageLog(QWidget* parent = nullptr);

protected:
	void contextMenuEvent(QContextMenuEvent *e) override;
private:
	QAction* clearAction;
};

class StaticLogger
{
public:
	static void Setup(QWidget* parent, QLayout* layout);
	static void logging(QtMsgType type, const QMessageLogContext &context, const QString &msg);
	static void logit(const QString& msg);

private:
	static QPointer<MessageLog> m_log;

};

#endif