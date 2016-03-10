#include "simulator.h"
#include "logger.h"

#include <QThread>

#include <DirectXMath.h>

using namespace DirectX;

using namespace wtl;

const static int g_bufsize = 512;

Simulator::Simulator(Logger* logger, QObject* parent)
	: QObject(parent)
	, m_windTunnel()
	, m_cellTypes()
	, m_solidVelocity()
	, m_velocityXYZW()
	, m_velocity()
	, m_density()
	, m_densitySum()
	, m_lines()
	, m_reseedCounter(0)
	, m_resolution(0, 0, 0)
	, m_voxelSize(0.0f, 0.0f, 0.0f)
	, m_sim(true)
	, m_simSmoke(true)
	, m_simLines(false)
	, m_elapsedTimer()
	, m_simTimer()
	, m_simMutex(QMutex::NonRecursive)
	, m_waitCond()
	, m_doWait(true)
	, m_isWaiting(false)
	, m_logger(logger)
{
}

void Simulator::continueSim()
{
	QMutexLocker lock(&m_simMutex);

	if (m_isWaiting)
		m_waitCond.wakeAll();
	else
		m_doWait = false;
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
	m_windTunnel = WindTunnel(clDevice, clPlatform, settingsFile.toStdString());
}

void Simulator::updateGrid()
{
	m_windTunnel.updateGrid(m_cellTypes, m_solidVelocity);

	emit simUpdated();
}

void Simulator::setGridDimensions(const XMUINT3& resolution, const XMFLOAT3& voxelSize)
{
	m_resolution = resolution;
	m_voxelSize = voxelSize;

	int lineBufferSize = m_windTunnel.setGridDimension(resolution, voxelSize);

	int size = resolution.x * resolution.y * resolution.z;

	m_cellTypes.resize(size);
	m_solidVelocity.resize(size * 4); // float4

	m_velocityXYZW.resize(size * 4);
	m_velocity.resize(size * 3); // float3
	m_density.resize(size);
	m_densitySum.resize(size);
	m_lines.resize(lineBufferSize);

	emit simUpdated();
}

void Simulator::step()
{
	qint64 elapsedTime = m_elapsedTimer.nsecsElapsed();
	m_elapsedTimer.restart();

	if (!m_sim)
		return;

	m_windTunnel.step(static_cast<double>(elapsedTime) * 1.0e-9 ); // nanosec to sec

	// Wait until the simulation results of last step were processed
	m_simMutex.lock();
	if (m_doWait)
	{
		m_isWaiting = true;
		m_waitCond.wait(&m_simMutex);
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
		m_windTunnel.fillLines(m_lines, m_reseedCounter);

	emit stepDone();
}
void Simulator::log(const QString& msg)
{
	if (m_logger)
		m_logger->logit(msg);
}

