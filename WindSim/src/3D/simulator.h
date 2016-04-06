#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <DirectXMath.h>

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>
#include <QDateTime>
#include <QJsonObject>

#include <vector>
#include <mutex>

#include <WindTunnelLib/WindTunnel.h>

class DX11Renderer;

class Simulator : public QObject
{
	Q_OBJECT
public:
	static bool initOpenCLNecessary();
	static void initOpenCL();
	static void shutdownOpenCL();
	static QMutex& mutex();

	static QJsonObject getSmokeSettingsDefault();
	static QJsonObject getLineSettingsDefault();

	Simulator(const QString& settingsFile, const DirectX::XMUINT3& resolution, const DirectX::XMFLOAT3& voxelSize, DX11Renderer* renderer = nullptr, QObject* parent = nullptr);

	void continueSim(bool skip = false); // Continue simulation after simulation results are processed by rendering thread, say if future steps should be skipped
	void reinitWindTunnel(); // Called from the rendering thread when static OpenCL was reinitialized; the static m_openCLMutex must be locked
	std::mutex& getRunningMutex() { return m_simRunning; }; // Get mutex, which indicates if simulation is currently running or not

	// Get vectors for writing
	std::vector<wtl::CellType>& getCellTypes() { return m_cellTypes; };
	std::vector<float>& getSolidVelocity() { return m_solidVelocity; };

	// Get vectors for reading
	const std::vector<float>& getVelocity() const { return m_velocity; };
	const std::vector<float>& getPressure() const { return m_pressure; };
	const std::vector<float>& getDensity() const { return m_density; };
	const std::vector<float>& getDensitySum() const { return m_densitySum; };
	const std::vector<char>& getLines() const { return m_lines; };
	const int getReseedCounter() const { return m_reseedCounter; };
	const int getNumLines() const { return m_numLines; };

signals:
	void stepDone(); // One simulation thread done, local output vectors filled
	void simUpdated();
	void simulatorReady();

public slots:
	void start(); // Start/continue thread loop
	void stop(); // Stop thread loop
	void pause(); // Pause thread loop

	void changeSimSettings(const QString& settingsFile); // Called when the json settings file changed
	void resetSimulation();
	void createWindTunnel(const QString& settingsFile); // Construct new windtunnel
	void updateGrid(); // Update CellTypes and solid velocity from local vectors
	void setGridDimension(const DirectX::XMUINT3& resolution, const DirectX::XMFLOAT3& voxelSize); // Update grid dimensions (empties cellTypes until next updateGrid)

	void changeSmokeSettings(const QJsonObject& settings);
	void changeLineSettings(const QJsonObject& settings);

private slots:
	void step(); // Thread loop

private:
	void log(const QString& msg);
	bool checkContinue();

	static QMutex m_openCLMutex;
	static int m_clDevice;
	static int m_clPlatform;

	wtl::WindTunnel m_windTunnel;

	// WindTunnel creation parameters
	QString m_settingsFile;

	QJsonObject m_smokeSettingsGUI;
	QJsonObject m_lineSettingsGUI;

	// WindTunnel input
	std::vector<wtl::CellType> m_cellTypes;
	std::vector<float> m_solidVelocity;

	// WindTunnel output
	std::vector<float> m_velocity;
	std::vector<float> m_pressure;
	std::vector<float> m_density;
	std::vector<float> m_densitySum;
	std::vector<char> m_lines;
	int m_reseedCounter;
	int m_numLines;

	// Grid variables
	DirectX::XMUINT3 m_resolution;
	DirectX::XMFLOAT3 m_voxelSize;

	// Simulation settings
	bool m_simSmoke;
	bool m_simLines;

	// Thread synchronization
	QElapsedTimer m_elapsedTimer;
	QTimer m_simTimer;
	QMutex m_simMutex;
	QWaitCondition m_waitCond;
	bool m_doWait; // Indicates if thread must wait on condition
	bool m_isWaiting; // Indicates if thread is currently waiting on condition and must be woke up
	bool m_skipSteps;

	std::mutex m_simRunning; // If sim thread holds lock -> sim is running/ timers running, use std::mutex as QMutex does not provide
	std::unique_lock<std::mutex> m_simulatorLock;

	DX11Renderer* m_renderer;
};

#endif