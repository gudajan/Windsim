#include "textOverlay.h"

#include <QPainter>
#include <QRect>
#include <QMoveEvent>

// Relative to object position
const static int g_textLineHeight = 13;
const static int g_spacing = 5;
const static QPoint g_fpsBackPos(g_spacing, g_spacing);
const static QSize g_fpsBackSize(70, g_textLineHeight);
const static QPoint g_textBackPos(g_spacing, 2 * g_spacing + g_fpsBackSize.height());
const static QSize g_textBackSize(190, g_textLineHeight * 5); // 5 lines at max

TextOverlay::TextOverlay(QWidget* parent)
	: QDialog(parent, Qt::FramelessWindowHint | Qt::WindowTransparentForInput)
	, m_text()
	, m_textBack(g_textBackPos, g_textBackSize)
	, m_fps("FPS: ")
	, m_fpsBack(g_fpsBackPos, g_fpsBackSize)
	, m_showText(false)
{
	setAttribute(Qt::WA_TranslucentBackground);

	resize(g_spacing + std::max(g_fpsBackSize.width(), g_textBackSize.width()), 2 * g_spacing + g_fpsBackSize.height() + g_textBackSize.height());

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(moveToParent()));

	m_timer.start(16);
}

void TextOverlay::showText(bool showText)
{
	m_showText = showText;
}

void TextOverlay::moveToParent()
{
	QPoint pos = parentWidget()->mapToGlobal(QPoint(0,0));
	move(pos);
}

void TextOverlay::paintEvent(QPaintEvent* event)
{
	QPainter p(this);
	//p.fillRect(m_fpsBack, QColor(0, 0, 0, 50));
	p.drawText(m_fpsBack, m_fps);
	if (m_showText)
	{
		//p.fillRect(m_textBack, QColor(0, 0, 0, 50));
		p.drawText(m_textBack, m_text);
	}
}