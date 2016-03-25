#include "dx11widget.h"
#include "staticLogger.h"
#include "settings.h"

#include <QResizeEvent>
#include <QApplication>
#include <QMessageBox>
#include <QPainter>


DX11Widget::DX11Widget(QWidget* parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),
	m_renderThread(),
	m_renderer(nullptr),
	m_overlay(parent)
{
	m_overlay.show();

	// Create DirectX Renderer on our widget
	// Parent MUST NOT be 'this', because we want to move this object to another thread, which is not possible for child objects
	m_renderer = new DX11Renderer(winId(), width(), height(), nullptr);

	if (!m_renderer->init())
	{
		QMessageBox::critical(parent, "DirectX Error", "Initialization of DirectX 11 failed. Please check stdout for more information!");
		throw std::runtime_error("ERROR: Failed to initialize DirectX 11!");
	}

	// Rendering should be executed in another thread, so 3D rendering does not block the GUI
	m_renderer->moveToThread(&m_renderThread);

	// Start/Stop rendering
	connect(&m_renderThread, &QThread::started, m_renderer, &DX11Renderer::execute);
	connect(this, &DX11Widget::stopRendering, m_renderer, &DX11Renderer::stop);
	connect(&m_renderThread, &QThread::finished, m_renderer, &QObject::deleteLater);

	// Arbitrary Events Widget -> Renderer
	connect(this, &DX11Widget::resize, m_renderer, &DX11Renderer::onResize);
	connect(this, &DX11Widget::mouseMove, m_renderer, &DX11Renderer::onMouseMove);
	connect(this, &DX11Widget::mousePress, m_renderer, &DX11Renderer::onMousePress);
	connect(this, &DX11Widget::mouseRelease, m_renderer, &DX11Renderer::onMouseRelease);
	connect(this, &DX11Widget::keyPress, m_renderer, &DX11Renderer::onKeyPress);
	connect(this, &DX11Widget::keyRelease, m_renderer, &DX11Renderer::onKeyRelease);
	connect(this, &DX11Widget::wheelUse, m_renderer, &DX11Renderer::onWheelUse);
	connect(this, &DX11Widget::mouseEnter, m_renderer, &DX11Renderer::onMouseEnter);
	connect(this, &DX11Widget::mouseLeave, m_renderer, &DX11Renderer::onMouseLeave);
	connect(this, &DX11Widget::reloadShadersTriggered, m_renderer, &DX11Renderer::reloadShaders);
	connect(this, &DX11Widget::settingsChanged, m_renderer, &DX11Renderer::changeSettings);

	// Arbitrary Events Renerer -> Widget
	connect(m_renderer->getLogger(), &Logger::log, this, &DX11Widget::logit);
	connect(m_renderer, &DX11Renderer::updateFPS, this, &DX11Widget::drawFps);
	connect(m_renderer, &DX11Renderer::drawText, this, &DX11Widget::drawText);

	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	setFocusPolicy(Qt::StrongFocus); // Set keyboard focus on mouseclick
	setFocus();
	setMouseTracking(true);

	m_renderThread.start();
}

DX11Widget::~DX11Widget()
{
	m_renderThread.wait();
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
	m_overlay.showText(conf.opencl.showInfo);

	emit settingsChanged();
}

void DX11Widget::logit(const QString& str)
{
	StaticLogger::logit(str);
}

void DX11Widget::drawFps(float fps)
{
	m_overlay.setFps(fps);
	m_overlay.repaint();
}

void DX11Widget::drawText(const QString& str)
{
	if (conf.opencl.showInfo)
	{
		m_overlay.setText(str);
		m_overlay.repaint();
	}
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
	emit keyPress(event->key());

	return QWidget::keyPressEvent(event);
}

void DX11Widget::keyReleaseEvent(QKeyEvent * event)
{
	emit keyRelease(event->key());

	return QWidget::keyReleaseEvent(event);
}

void DX11Widget::mouseMoveEvent(QMouseEvent * event)
{

	emit mouseMove(event->pos(), event->globalPos(), event->modifiers());
	return QWidget::mouseMoveEvent(event);
}

void DX11Widget::mousePressEvent(QMouseEvent * event)
{
	emit mousePress(event->globalPos(), event->button(), event->modifiers());
	return QWidget::mousePressEvent(event);
}

void DX11Widget::mouseReleaseEvent(QMouseEvent * event)
{
	emit mouseRelease(event->globalPos(), event->button(), event->modifiers());
	return QWidget::mouseReleaseEvent(event);
}

void DX11Widget::wheelEvent(QWheelEvent * event)
{
	emit wheelUse(event->angleDelta());
	return QWidget::wheelEvent(event);
}

void DX11Widget::enterEvent(QEvent* event)
{
	emit mouseEnter();
	return QWidget::enterEvent(event);
}

void DX11Widget::leaveEvent(QEvent* event)
{
	emit mouseLeave();
	return QWidget::enterEvent(event);
}