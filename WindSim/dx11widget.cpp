#include "dx11widget.h"
#include "logger.h"
#include "dx11renderer.h"

#include <QResizeEvent>
#include <QApplication>

DX11Widget::DX11Widget(QWidget* parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),
	m_renderThread(),
	m_renderer(nullptr)
{
	// Create DirectX Renderer on our widget
	m_renderer= new DX11Renderer(winId(), width(), height());

	if (!m_renderer->init())
		throw std::runtime_error("Failed to initialize renderer!");

	// Rendering should be executed in another thread, so 3D rendering does not block the GUI
	m_renderer->moveToThread(&m_renderThread);

	// Start/Stop rendering
	connect(&m_renderThread, &QThread::started, m_renderer, &DX11Renderer::execute);
	connect(this, &DX11Widget::stopRendering, m_renderer, &DX11Renderer::stop);
	connect(&m_renderThread, &QThread::finished, m_renderer, &QObject::deleteLater);

	// Arbitrary Events Widget -> Renderer
	connect(this, &DX11Widget::resize, m_renderer, &DX11Renderer::onResize);
	connect(this, &DX11Widget::controlEvent, m_renderer, &DX11Renderer::onControlEvent);
	connect(this, &DX11Widget::createMeshTriggered, m_renderer, &DX11Renderer::onCreateMesh);
	connect(this, &DX11Widget::createSkyTriggered, m_renderer, &DX11Renderer::onCreateSky);
	connect(this, &DX11Widget::reloadShadersTriggered, m_renderer, &DX11Renderer::reloadShaders);

	// Arbitrary Events Renerer -> Widget
	connect(m_renderer, &DX11Renderer::logit, this, &DX11Widget::logit);

	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	setFocusPolicy(Qt::StrongFocus); // Set keyboard focus on mouseclick
	setFocus();

	m_renderThread.start();
}

DX11Widget::~DX11Widget()
{
	m_renderThread.quit();
	m_renderThread.wait();
}

void DX11Widget::createMesh(const QString& name, const QString& path)
{
	emit createMeshTriggered(name, path);
}

void DX11Widget::createSky(const QString& name)
{
	emit createSkyTriggered(name);
}

void DX11Widget::reloadShaders()
{
	emit reloadShadersTriggered();
}

void DX11Widget::cleanUp()
{
	emit stopRendering();
}

// Trigger update of current settings
void DX11Widget::applySettings()
{
	m_renderer->getCamera()->computeViewMatrix(); // Settings may have changed
}

void DX11Widget::logit(const QString& str)
{
	Logger::logit(str);
}

void DX11Widget::paintEvent(QPaintEvent* event)
{
	return QWidget::paintEvent(event);
}

void DX11Widget::resizeEvent(QResizeEvent* event)
{
	emit resize(event->size().width(), event->size().height());

	return QWidget::resizeEvent(event);
}

void DX11Widget::keyPressEvent(QKeyEvent * event)
{
	m_renderer->getCamera()->handleControlEvent(event);
	return QWidget::keyPressEvent(event);
}

void DX11Widget::keyReleaseEvent(QKeyEvent * event)
{
	m_renderer->getCamera()->handleControlEvent(event);
	return QWidget::keyReleaseEvent(event);
}

void DX11Widget::mouseMoveEvent(QMouseEvent * event)
{
	m_renderer->getCamera()->handleControlEvent(event);
	return QWidget::mouseMoveEvent(event);
}

void DX11Widget::mousePressEvent(QMouseEvent * event)
{
	m_renderer->getCamera()->handleControlEvent(event);
	return QWidget::mousePressEvent(event);
}

void DX11Widget::mouseReleaseEvent(QMouseEvent * event)
{
	m_renderer->getCamera()->handleControlEvent(event);
	return QWidget::mouseReleaseEvent(event);
}

void DX11Widget::wheelEvent(QWheelEvent * event)
{
	m_renderer->getCamera()->handleControlEvent(event);
	return QWidget::wheelEvent(event);
}