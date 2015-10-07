#include "logger.h"

QPointer<QTextEdit> Logger::m_log = nullptr;

void Logger::Setup(QWidget* parent)
{
	if (!m_log)
	{
		m_log = new QTextEdit(parent);
		m_log->setObjectName(QStringLiteral("log"));
		QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Expanding);
		sp.setHorizontalStretch(0);
		sp.setVerticalStretch(1);
		sp.setHeightForWidth(m_log->sizePolicy().hasHeightForWidth());
		m_log->setSizePolicy(sp);

		m_log->setReadOnly(true);
	}
}

void Logger::logging(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	if (m_log)
	{
		switch (type)
		{
		case QtDebugMsg:
		case QtWarningMsg:
		case QtCriticalMsg:
			m_log->append(msg);
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

void Logger::logit(const QString& msg)
{
	m_log->append(msg);
	m_log->repaint();
}