#include "simulator.h"
#include "logger.h"

#include <QThread>
#include <QFileInfo>

#include <DirectXMath.h>

#include <Windows.h>

using namespace DirectX;

using namespace wtl;

const static int g_bufsize = 512;

Simulator::Simulator(Logger* logger, QObject* parent)
	: QObject(parent)
	, m_windTunnel()
	, m_clDevice(-1)
	, m_clPlatform(-1)
	, m_settingsFile("")
	, m_cellTypes()
	, m_solidVelocity()
	, m_velocityXYZW()
	, m_velocity()
	, m_density()
	, m_densitySum()
	, m_lines()
	, m_reseedCounter(0)
	, m_numLines(0)
	, m_resolution(0, 0, 0)
	, m_voxelSize(0.0f, 0.0f, 0.0f)
	, m_sim(true)
	, m_simSmoke(true)
	, m_simLines(true)
	, m_elapsedTimer()
	, m_simTimer(this)
	, m_simMutex(QMutex::NonRecursive)
	, m_waitCond()
	, m_doWait(false)
	, m_isWaiting(false)
	, m_stop(false)
	, m_logger(logger)
	, m_initialized(false)
{
	connect(&m_simTimer, &QTimer::timeout, this, &Simulator::step);
}

void Simulator::continueSim(bool stop)
{
	QMutexLocker lock(&m_simMutex);

	if (m_isWaiting)
		m_waitCond.wakeAll();
	else
		m_doWait = false;

	if (stop) m_stop = true;
}

void Simulator::start()
{
	m_simTimer.start(0);
	m_elapsedTimer.start();
}

void Simulator::stop()
{
	m_simTimer.stop();
	thread()->quit(); // Quit the thread
}

void Simulator::createWindTunnel(int clDevice, int clPlatform, const QString& settingsFile)
{
	// Only recreate the wind tunnel if settings have changed
	QDateTime lastMod;
	if (clDevice != m_clDevice || clPlatform != m_clPlatform || settingsFile != m_settingsFile || (lastMod = QFileInfo(settingsFile).lastModified()) > m_fileLastModified)
	{
		log("INFO: Create virtual WindTunnel with OpenCL device " + QString::number(clDevice) + ", platform " + QString::number(clPlatform) + " and settings file " + settingsFile + ".");
		m_windTunnel = WindTunnel(clDevice, clPlatform, settingsFile.toStdString());
		m_clDevice = clDevice;
		m_clPlatform = clPlatform;
		m_settingsFile = settingsFile;
		m_fileLastModified = lastMod;
	}
}

void Simulator::updateGrid()
{
	m_windTunnel.updateGrid(m_cellTypes); // , m_solidVelocity);
	m_initialized = true;
	emit simUpdated();
}

void Simulator::setGridDimensions(const XMUINT3& resolution, const XMFLOAT3& voxelSize)
{
	log("INFO: Setting grid dimensions to (" + QString::number(resolution.x) + ", " + QString::number(resolution.y) + ", " + QString::number(resolution.z) + ") with cell size (" + QString::number(voxelSize.x, 'g', 2) + ", " + QString::number(voxelSize.y, 'g', 2) + ", " + QString::number(voxelSize.z, 'g', 2) + ") ...");
	m_resolution = resolution;
	m_voxelSize = voxelSize;

	m_windTunnel.setGridDimension(resolution, voxelSize);
	int lineBufferSize = m_windTunnel.getLineBufferSize();

	int size = resolution.x * resolution.y * resolution.z;

	m_cellTypes.resize(size);
	m_solidVelocity.resize(size * 4); // float4

	m_velocityXYZW.resize(size * 4);
	m_velocity.resize(size * 3); // float3
	m_density.resize(size);
	m_densitySum.resize(size);
	m_lines.resize(lineBufferSize);

	log("INFO: Done.");
	emit simUpdated();
}

void Simulator::step()
{
	{
		QMutexLocker lock(&m_simMutex);
		if (m_stop)
			return;
	}

	if (!m_initialized)
		return;

	qint64 elapsedTime = m_elapsedTimer.nsecsElapsed();
	m_elapsedTimer.restart();

	QElapsedTimer timer;

	if (!m_sim)
		return;

	timer.start();
	m_windTunnel.step(static_cast<double>(elapsedTime) * 1.0e-9 ); // nanosec to sec
	OutputDebugStringA(("Sim step lasted " + std::to_string(timer.nsecsElapsed() * 1e-6) + "msec\n").c_str());

	// Wait until the simulation results of last step were processed
	m_simMutex.lock();
	if (m_doWait) // Only wait if simulation is not stopped already
	{
		m_isWaiting = true;

		timer.restart();
		m_waitCond.wait(&m_simMutex);
		OutputDebugStringA(("Sim waited " + std::to_string(timer.nsecsElapsed() * 1e-6) + "msec\n").c_str());
		m_isWaiting = false;
	}
	else
	{
		m_doWait = true;
	}
	m_simMutex.unlock();

	m_windTunnel.fillVelocity(m_velocity);
	if (m_simSmoke)
		m_windTunnel.fillDensity(m_density, m_densitySum);
	if (m_simLines)
		m_windTunnel.fillLines(m_lines, m_reseedCounter, m_numLines);

	emit stepDone();
}
void Simulator::log(const QString& msg)
{
	if (m_logger)
		m_logger->logit(msg);
}

