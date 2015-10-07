#include "dx11widget.h"
#include "logger.h"
#include "dx11renderer.h"

#include <QResizeEvent>

DX11Widget::DX11Widget(QWidget* parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),

	m_renderThread()
{
	// Create DirectX Renderer on our widget
	DX11Renderer* renderer = new DX11Renderer(winId(), width(), height());

	renderer->onInit();

	// Rendering should be executed in another thread, so 3D rendering does not block the GUI
	renderer->moveToThread(&m_renderThread);

	connect(&m_renderThread, &QThread::finished,renderer, &QObject::deleteLater);
	connect(&m_renderThread, &QThread::started, renderer, &DX11Renderer::execute);
	connect(this, &DX11Widget::resize, renderer, &DX11Renderer::onResize);
	connect(renderer, &DX11Renderer::logit, this, &DX11Widget::logit);
	connect(this, &DX11Widget::stopRendering, renderer, &DX11Renderer::stop);


	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	setFocus();
}

DX11Widget::~DX11Widget()
{
	emit stopRendering();
	m_renderThread.quit();
	m_renderThread.wait();
}

void DX11Widget::paintEvent(QPaintEvent* event)
{
	if (!m_renderThread.isRunning())
		m_renderThread.start();
}

void DX11Widget::resizeEvent(QResizeEvent* event)
{
	//m_renderer.onResize(event->size().width(), event->size().height());
	emit resize(event->size().width(), event->size().height());
}

void DX11Widget::logit(const QString& str)
{
	Logger::logit(str);
}
