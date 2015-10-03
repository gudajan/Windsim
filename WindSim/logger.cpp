#include "logger.h"

QPointer<QTextEdit> Logger::log = nullptr;

void Logger::Setup(QWidget* parent)
{
	if (!log)
	{
		log = new QTextEdit(parent);
		log->setObjectName(QStringLiteral("log"));
		QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Expanding);
		sp.setHorizontalStretch(0);
		sp.setVerticalStretch(1);
		sp.setHeightForWidth(log->sizePolicy().hasHeightForWidth());
		log->setSizePolicy(sp);

		log->setReadOnly(true);
	}
}

void Logger::logging(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	if (log)
	{
		switch (type)
		{
		case QtDebugMsg:
		case QtWarningMsg:
		case QtCriticalMsg:
			log->append(msg);
			log->repaint();
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