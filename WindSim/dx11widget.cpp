#include "dx11widget.h"
#include "logger.h"
#include "dx11renderer.h"

#include <QResizeEvent>

DX11Widget::DX11Widget(QWidget* parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),
	m_renderThread(),
	m_renderer(nullptr)
{
	// Create DirectX Renderer on our widget
	DX11Renderer* m_renderer= new DX11Renderer(winId(), width(), height());

	if (!m_renderer->init())
		throw std::runtime_error("Failed to initialize renderer!");

	// Rendering should be executed in another thread, so 3D rendering does not block the GUI
	m_renderer->moveToThread(&m_renderThread);

	connect(&m_renderThread, &QThread::finished, m_renderer, &QObject::deleteLater);
	connect(&m_renderThread, &QThread::started, m_renderer, &DX11Renderer::execute);
	connect(this, &DX11Widget::resize, m_renderer, &DX11Renderer::onResize);
	connect(m_renderer, &DX11Renderer::logit, this, &DX11Widget::logit);
	connect(this, &DX11Widget::stopRendering, m_renderer, &DX11Renderer::stop);


	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	setFocus();

	m_renderThread.start();
}

DX11Widget::~DX11Widget()
{
	emit stopRendering();
	m_renderThread.quit();
	m_renderThread.wait();
}

void DX11Widget::paintEvent(QPaintEvent* event)
{
		
}

void DX11Widget::resizeEvent(QResizeEvent* event)
{
	emit resize(event->size().width(), event->size().height());
}

void DX11Widget::logit(const QString& str)
{
	Logger::logit(str);
}
