#include "dx11widget.h"
#include "staticLogger.h"
#include "settings.h"
#include "../3D/simulator.h"

#include "settingsDialog.h"

#include <QResizeEvent>
#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QInputDialog>


DX11Widget::DX11Widget(QWidget* parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),
	m_renderThread(),
	m_renderer(nullptr),
	m_overlay(new TextOverlay(this)),
	m_printInfo(false)
{
	m_overlay->showText(conf.opencl.showInfo);
	m_overlay->show();

	if (Simulator::initOpenCLNecessary())
	{
		static auto info = wtl::getOpenCLInfo();
		if (info.size() == 0 || std::accumulate(info.begin(), info.end(), 0, [](int v, const std::pair<wtl::description, std::vector<wtl::description>>& e) { return v + static_cast<int>(e.second.size()); }) == 0)
		{
			QMessageBox::critical(parent, "OpenCL Error", "No OpenCL platforms and devices available! Install the necessary SDK's by Intel or AMD!");
			throw std::runtime_error("ERROR: Failed to initialize OpenCL!");
		}
		if (conf.opencl.device < 0 || conf.opencl.platform < 0)
		{
			QMessageBox::information(parent, "OpenCL Information", "Please enter an OpenCL platform and device in the next dialogs!");
			QStringList platforms;
			for (const auto& p : info)
				platforms << QString::fromStdString(p.first.at("name"));
			bool ok;
			QString platform = QInputDialog::getItem(parent, "Please choose a OpenCL platform", "OpenCL Platform:", platforms, 0, false, &ok);
			if (!ok)
				throw std::runtime_error("ERROR: Failed to initialize OpenCL!");

			int pid = std::distance(platforms.begin(), std::find(platforms.begin(), platforms.end(), platform));
			QStringList devices;
			for (const auto& d : info[pid].second)
				devices << QString::fromStdString(d.at("name") + " " + d.at("vendor"));
			QString device = QInputDialog::getItem(parent, "Please choose a OpenCL device", "OpenCL Device:", devices, 0, false, &ok);
			if (!ok)
				throw std::runtime_error("ERROR: Failed to initialize OpenCL!");

			int did = std::distance(devices.begin(), std::find(devices.begin(), devices.end(), device));

			conf.opencl.platform = pid;
			conf.opencl.device = did;
		}
		Simulator::initOpenCL();
	}


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
	connect(this, &DX11Widget::mouseDoubleClick, m_renderer, &DX11Renderer::onMouseDoubleClick);
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

	QTimer* timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(enableInfoPrint()));
	timer->start(1000);

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
	m_overlay->showText(conf.opencl.showInfo);

	emit settingsChanged();
}

void DX11Widget::logit(const QString& str)
{
	StaticLogger::logit(str);
}

void DX11Widget::drawFps(float fps)
{
	m_overlay->setFps(fps);
	m_overlay->repaint();
}

void DX11Widget::drawText(const QString& str)
{
	if (conf.opencl.showInfo)
	{
		m_overlay->setText(str);
		m_overlay->repaint();
	}
	if (conf.opencl.printInfo && m_printInfo)
	{
		QStringList s = str.split("\n");
		s.removeFirst();
		StaticLogger::logit(s.join("\n"));
		m_printInfo = false;
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

void DX11Widget::mouseDoubleClickEvent(QMouseEvent* event)
{
	emit mouseDoubleClick(event->globalPos(), event->button(), event->modifiers());
	return QWidget::mouseDoubleClickEvent(event);
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