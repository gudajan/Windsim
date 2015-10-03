#include "dx11widget.h"

#include <QResizeEvent>
#include <QDebug>

DX11Widget::DX11Widget(QWidget* parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),
	m_renderer(winId(), width(), height())
{
	m_renderer.onInit();

	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setUpdatesEnabled(true);

	setFocus();
}

DX11Widget::~DX11Widget()
{
	m_renderer.onDestroy();
}

void DX11Widget::paintEvent(QPaintEvent* event)
{
	m_renderer.onUpdate();
	m_renderer.onRender();

	qDebug() << "REPAINT";
}

void DX11Widget::resizeEvent(QResizeEvent* event)
{
	m_renderer.onResize(event->size().width(), event->size().height());
}
