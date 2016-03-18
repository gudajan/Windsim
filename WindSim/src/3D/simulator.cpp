#include "simulator.h"
#include "logger.h"
#include "settings.h"

#include <QThread>
#include <QFileInfo>

#include <DirectXMath.h>

#include <Windows.h>

using namespace DirectX;

using namespace wtl;

QMutex Simulator::m_openCLMutex;
int Simulator::m_clDevice = -1;
int Simulator::m_clPlatform = -1;
bool Simulator::m_pauseSim = true;

void Simulator::initOpenCL()
{
	{
		QMutexLocker lock(&m_openCLMutex);
		m_pauseSim = true;
	}

	if (conf.opencl.device != m_clDevice || conf.opencl.platform != m_clPlatform)
	{
		m_clDevice = conf.opencl.device;
		m_clPlatform = conf.opencl.platform;
		WindTunnel::shutdownOpenCL();
		WindTunnel::initOpenCL(m_clDevice, m_clPlatform);
	}

	{
		QMutexLocker lock(&m_openCLMutex);
		m_pauseSim = false;
	}
}

Simulator::Simulator(Logger* logger, QObject* parent)
	: QObject(parent)
	, m_windTunnel()
	, m_settingsFile("<NONE>") // Needed so it is different to the default string '<' character not allowed in file names -> no valid filename
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
{
	connect(&m_simTimer, &QTimer::timeout, this, &Simulator::step);
}

void Simulator::continueSim(bool stop)
{
	{
		QMutexLocker lock(&m_openCLMutex);
		if(m_pauseSim)
			return;
	}

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

void Simulator::changeSimSettings(const QString& settingsFile)
{
	if (createWindTunnel(settingsFile))
		setGridDimensions(m_resolution, m_voxelSize);
}

bool Simulator::createWindTunnel(const QString& settingsFile)
{
	// Only recreate the wind tunnel if settings have changed or cl device/platform changed
	QDateTime lastMod;
	if (settingsFile != m_settingsFile || (lastMod = QFileInfo(settingsFile).lastModified()) > m_fileLastModified)
	{
		log("INFO: Create virtual WindTunnel with settings file " + settingsFile + ".");
		m_windTunnel = WindTunnel(settingsFile.toStdString()); // Move assignment
		m_settingsFile = settingsFile;
		m_fileLastModified = lastMod;
		return true;
	}
	return false;
}

void Simulator::updateGrid()
{
	{
		QMutexLocker lock(&m_openCLMutex);
		if (m_pauseSim)
			return;
	}

	m_windTunnel.updateGrid(m_cellTypes); // , m_solidVelocity);
	emit simUpdated();
}

void Simulator::setGridDimensions(const XMUINT3& resolution, const XMFLOAT3& voxelSize)
{
	{
		QMutexLocker lock(&m_openCLMutex);
		if (m_pauseSim)
			return;
	}

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

void Simulator::runSimulation(bool enabled)
{
	m_sim = enabled;
}

void Simulator::changeSmokeSettings(const QJsonObject& settings)
{
	m_simSmoke = settings["enabled"].toBool();
	m_windTunnel.smokeSim(m_simSmoke ? Smoke::Enabled : Smoke::Disabled);
	const QJsonObject& pos = settings["seedPosition"].toObject();
	m_windTunnel.setSmokePosition({ static_cast<float>(pos["x"].toDouble()), static_cast<float>(pos["y"].toDouble()), static_cast<float>(pos["z"].toDouble()) });
	m_windTunnel.setSmokeSeedRadius(static_cast<float>(settings["seedRadius"].toDouble()));
}

void Simulator::changeLineSettings(const QJsonObject& settings)
{
	m_simLines = settings["enabled"].toBool();
	m_windTunnel.lineSim(m_simLines ? Line::Enabled : Line::Disabled);
	QString tmp = settings["orientation"].toString();
	m_windTunnel.setLineOrientation(tmp == "X" ? Line::X : (tmp == "Y" ? Line::Y : Line::Z));
	tmp = settings["type"].toString();
	m_windTunnel.setLineType(tmp == "Streamline" ? Line::StreamLine : Line::StreakLine);
	m_windTunnel.setLinePosition(static_cast<float>(settings["position"].toDouble()));
}

void Simulator::step()
{
	{
		QMutexLocker lock(&m_openCLMutex);
		if (m_pauseSim)
			return;
	}
	{
		QMutexLocker lock(&m_simMutex);
		if (m_stop)
			return;
	}

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

