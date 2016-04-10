#include "staticLogger.h"

#include <QLayout>
#include <QMenu>

MessageLog::MessageLog(QWidget* parent)
	: QPlainTextEdit(parent)
	, clearAction(new QAction(this))
{
	clearAction->setText(tr("Clear"));

	connect(clearAction, SIGNAL(triggered()), this, SLOT(clear()));
}

void MessageLog::contextMenuEvent(QContextMenuEvent* event)
{
	QMenu* menu = createStandardContextMenu(event->pos());

	menu->addAction(clearAction);
	menu->exec(event->globalPos());
}


QPointer<MessageLog> StaticLogger::m_log = nullptr;

void StaticLogger::Setup(QWidget* parent, QLayout* layout)
{
	if (!m_log)
	{
		m_log = new MessageLog(parent);
		m_log->setObjectName(QStringLiteral("log"));
		QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Expanding);
		sp.setHorizontalStretch(0);
		sp.setVerticalStretch(0);
		sp.setHeightForWidth(m_log->sizePolicy().hasHeightForWidth());
		m_log->setSizePolicy(sp);

		m_log->setReadOnly(true);
		QFont monospace("monospace");
		monospace.setStyleHint(QFont::StyleHint::Monospace);
		monospace.setWeight(QFont::Light);
		m_log->setFont(monospace);

		layout->addWidget(m_log);
	}
}

void StaticLogger::logging(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	if (m_log)
	{
		switch (type)
		{
		case QtDebugMsg:
		case QtWarningMsg:
		case QtCriticalMsg:
			m_log->appendPlainText(msg);
			m_log->repaint();
			break;
		case QtFatalMsg:
			abort();
		}
	}
	else
	{
		QByteArray localMsg = msg.toLocal8Bit();
		switch (type)
		{
		case QtDebugMsg:
			fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
			break;
		case QtWarningMsg:
			fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
			break;
		case QtCriticalMsg:
			fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
			break;
		case QtFatalMsg:
			fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
			abort();
		}
	}
}

void StaticLogger::logit(const QString& msg)
{
	m_log->appendPlainText(msg);
	m_log->repaint();
}