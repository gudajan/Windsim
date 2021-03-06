#include "simulator.h"
#include "dx11Renderer.h"
#include "settings.h"

#include <QThread>

#include <DirectXMath.h>

#include <Windows.h>

using namespace DirectX;

using namespace wtl;

const int stepTimesSaved = 20;
const float stepTimesWeight = 1.0 / stepTimesSaved;

QMutex Simulator::m_openCLMutex;
int Simulator::m_clDevice = -2;
int Simulator::m_clPlatform = -2;

bool Simulator::initOpenCLNecessary()
{
	return (conf.opencl.device != m_clDevice || conf.opencl.platform != m_clPlatform);
}

bool Simulator::initOpenCL()
{
	m_clDevice = conf.opencl.device;
	m_clPlatform = conf.opencl.platform;
	WindTunnel::shutdownOpenCL();
	WindTunnel::initOpenCL(m_clDevice, m_clPlatform);
	return true;
}

void Simulator::shutdownOpenCL()
{
	WindTunnel::shutdownOpenCL();
}

QMutex& Simulator::mutex()
{
	return m_openCLMutex;
};

QJsonObject Simulator::getSmokeSettingsDefault()
{
	QJsonObject tmp{ { "x", 0.125 }, { "y", 0.5 }, { "z", 0.5 } };
	return QJsonObject{ { "enabled", true }, { "seedRadius", 0.125 }, { "seedPosition", tmp } };
}

QJsonObject Simulator::getLineSettingsDefault()
{
	QJsonObject tmp{ { "X", 0.125 }, { "Y", 0.5 }, { "Z", 0.5 } };
	return QJsonObject{ { "enabled", false }, { "orientation", "Z" }, { "type", "Streakline" }, { "position", tmp } };
}

Simulator::Simulator(const QString& settingsFile, const XMUINT3& resolution, const XMFLOAT3& voxelSize, DX11Renderer* renderer, QObject* parent)
	: QObject(parent)
	, m_windTunnel()
	, m_settingsFile(settingsFile)
	, m_smokeSettingsGUI(getSmokeSettingsDefault())
	, m_lineSettingsGUI(getLineSettingsDefault())
	, m_cellTypes()
	, m_velocity()
	, m_pressure()
	, m_density()
	, m_densitySum()
	, m_lines()
	, m_reseedCounter(0)
	, m_numLines(0)
	, m_timeStep(0.0)
	, m_resolution(resolution)
	, m_voxelSize(voxelSize)
	, m_simSmoke(true)
	, m_simLines(false)
	, m_simTimer(this)
	, m_simMutex(QMutex::NonRecursive)
	, m_waitCond()
	, m_doWait(false)
	, m_isWaiting(false)
	, m_skipSteps(false)
	, m_simRunning()
	, m_simulatorLock(m_simRunning, std::defer_lock)
	, m_renderer(renderer)
	, m_stepTimer()
	, m_totalStepTimes(stepTimesSaved, 0.0)
{

	connect(&m_simTimer, &QTimer::timeout, this, &Simulator::step);

	createWindTunnel(settingsFile);
	setGridDimension(m_resolution, m_voxelSize);
}

void Simulator::continueSim(bool skip)
{
	QMutexLocker lock(&m_simMutex);

	if (m_isWaiting)
		m_waitCond.wakeAll();
	else
		m_doWait = false;

	// Make sure further step events do not block if necessary (e.g. for grid resize)
	if(skip) m_skipSteps = true; // Only switch on
}

void Simulator::reinitWindTunnel()
{
	// This is called when the sim mutex is already locked
	log("INFO: Reinitializing WindTunnel...");
	// Silently recreate windtunnel with the existing settings
	// Necessary when the static OpenCL context and queue are recreated
	m_windTunnel = WindTunnel(m_settingsFile.toStdString());
	m_windTunnel.setGridDimension(m_resolution, m_voxelSize);
	changeSmokeSettings(m_smokeSettingsGUI);
	changeLineSettings(m_lineSettingsGUI);
	log("INFO: Done.");
}

void Simulator::start()
{
	m_simTimer.start(1);
	{
		QMutexLocker lock(&m_simMutex);
		m_skipSteps = false;
	}
	if (!m_simulatorLock.owns_lock())
		m_simulatorLock.lock();

	m_stepTimer.start();
}

void Simulator::stop()
{
	m_simTimer.stop();
	if (m_simulatorLock.owns_lock())
		m_simulatorLock.unlock();
	thread()->quit(); // Quit the thread
}

void Simulator::pause()
{
	m_simTimer.stop();
	m_skipSteps = true; // make sure, no waiting step events are "processed"
	if (m_simulatorLock.owns_lock())
		m_simulatorLock.unlock();
}

void Simulator::changeSimSettings(const QString& settingsFile)
{
	createWindTunnel(settingsFile);
	setGridDimension(m_resolution, m_voxelSize);
}

void Simulator::resetSimulation()
{
	log("INFO: Reset WindTunnel.");
	m_windTunnel.reset();
}

void Simulator::createWindTunnel(const QString& settingsFile)
{
	// Only create one windtunnel at a time (releasing windtunnel includes opencl operations)
	QMutexLocker lock(&m_openCLMutex);
	log("INFO: Create virtual WindTunnel with settings file '" + settingsFile + "'.");
	m_windTunnel = WindTunnel(settingsFile.toStdString()); // Move assignment
}

void Simulator::updateGrid()
{
	// Skip if currently windTunnels not available
	if (!checkContinue()) return;

	m_windTunnel.updateGrid(m_cellTypes);
	OutputDebugStringA("INFO: Updated celltypes in WindTunnel!\n");
	emit simUpdated();
}

void Simulator::setGridDimension(const XMUINT3& resolution, const XMFLOAT3& voxelSize)
{
	{
		// Only one windtunnel init operation (with opencl) at a time
		QMutexLocker lock(&m_openCLMutex);
		log("INFO: Setting grid dimensions to (" + QString::number(resolution.x) + ", " + QString::number(resolution.y) + ", " + QString::number(resolution.z) + ") with cell size (" + QString::number(voxelSize.x, 'g', 2) + ", " + QString::number(voxelSize.y, 'g', 2) + ", " + QString::number(voxelSize.z, 'g', 2) + ") ...");
		m_resolution = resolution;
		m_voxelSize = voxelSize;

		m_windTunnel.setGridDimension(resolution, voxelSize);

		int lineBufferSize = m_windTunnel.getLineBufferSize();

		int size = resolution.x * resolution.y * resolution.z;

		m_cellTypes.resize(size);

		m_velocity.resize(size * 4); // float3 + 1 padding
		m_pressure.resize(size);
		m_density.resize(size);
		m_densitySum.resize(size);
		m_lines.resize(lineBufferSize);

		log("INFO: Done.");

		emit simulatorReady(); // Set sim availbale
		emit simUpdated(); // Enable grid updates
	}

	// Reinitialize settings from GUI
	changeSmokeSettings(m_smokeSettingsGUI);
	changeLineSettings(m_lineSettingsGUI);

	QMutexLocker lock(&m_simMutex);
	m_skipSteps = false; // Steps were skipped during resize
}

void Simulator::changeSmokeSettings(const QJsonObject& settings)
{
	m_smokeSettingsGUI = settings;

	if (!checkContinue()) return;

	m_simSmoke = settings["enabled"].toBool();
	m_windTunnel.smokeSim(m_simSmoke ? Smoke::Enabled : Smoke::Disabled);
	const QJsonObject& pos = settings["seedPosition"].toObject();
	m_windTunnel.setSmokePosition({ static_cast<float>(pos["x"].toDouble()), static_cast<float>(pos["y"].toDouble()), static_cast<float>(pos["z"].toDouble()) });
	m_windTunnel.setSmokeSeedRadius(static_cast<float>(settings["seedRadius"].toDouble()));

}

void Simulator::changeLineSettings(const QJsonObject& settings)
{
	m_lineSettingsGUI = settings;

	if (!checkContinue()) return;

	m_simLines = settings["enabled"].toBool();
	m_windTunnel.lineSim(m_simLines ? Line::Enabled : Line::Disabled);
	QString ori = settings["orientation"].toString();
	m_windTunnel.setLineOrientation(ori == "X" ? Line::X : (ori == "Y" ? Line::Y : Line::Z));
	QString type = settings["type"].toString();
	m_windTunnel.setLineType(type == "Streamline" ? Line::StreamLine : Line::StreakLine);
	m_windTunnel.setLinePosition(static_cast<float>(settings["position"].toObject()[ori].toDouble()));
}

void Simulator::step()
{
	// Skip step if windtunnels not available
	if (!checkContinue()) return;

	{
		QMutexLocker lock(&m_simMutex);
		if (m_skipSteps)
			return;
	}


	QElapsedTimer timer;

	timer.start();
	m_timeStep = m_windTunnel.step(); // nanosec to sec
	OutputDebugStringA(("INFO: Simulation step lasted " + std::to_string(timer.nsecsElapsed() * 1e-6) + "msec\n").c_str());

	// Wait until the simulation results of last step were processed
	m_simMutex.lock();
	if (m_doWait) // Only wait if simulation is not stopped already
	{
		m_isWaiting = true;

		timer.restart();
		m_waitCond.wait(&m_simMutex);
		OutputDebugStringA(("INFO: Simulation waited for " + std::to_string(timer.nsecsElapsed() * 1e-6) + "msec\n").c_str());
		m_isWaiting = false;
	}
	else
	{
		m_doWait = true;
	}
	m_simMutex.unlock();

	m_windTunnel.fillVelocity(m_velocity);
	m_windTunnel.fillPressure(m_pressure);
	if (m_simSmoke)
		m_windTunnel.fillDensity(m_density, m_densitySum);
	if (m_simLines)
		m_windTunnel.fillLines(m_lines, m_reseedCounter, m_numLines);

	// Calculate average steps per second here, as it depends on the number of times this function is called and not on the elapsed time for calculating the simulation step itself
	long long et = m_stepTimer.nsecsElapsed();
	m_stepTimer.restart();
	m_totalStepTimes.pop_back();
	m_totalStepTimes.push_front(1.0e9 / et); // 1 sec / elapsedTime nsec = fps
	float sps = std::accumulate(m_totalStepTimes.begin(), m_totalStepTimes.end(), 0.0, [](float acc, const float& val) {return acc + stepTimesWeight * val; });

	m_renderer->drawInfo(QString::fromStdString(m_windTunnel.getOpenCLStats() + "Avg steps per sec: " + std::to_string(sps)));

	emit stepDone();
}

void Simulator::log(const QString& msg)
{
	OutputDebugStringA((msg.toStdString() + "\n").c_str());
	m_renderer->getLogger()->logit(msg);
}

bool Simulator::checkContinue()
{
	if (!m_openCLMutex.tryLock(1000))
		return false;

	m_openCLMutex.unlock();
	return true;
}

