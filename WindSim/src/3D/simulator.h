#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>

#include <vector>

#include <WindTunnelLib/IWindTunnel.h>

class Logger;

class Simulator : public QObject
{
	Q_OBJECT
public:
	Simulator(Logger* logger = nullptr, QObject* parent = nullptr);

	void continueSim(); // Continue simulation after simulation results are processed by rendering thread

	// Get vectors for writing
	std::vector<wtl::CellType>& getCellTypes() { return m_cellTypes; };
	std::vector<float>& getSolidVelocity() { return m_solidVelocity; };

	// Get vectors for reading
	const std::vector<float>& getVelocity() const { return m_velocityXYZW; };
	const std::vector<float>& getDensity() const { return m_density; };
	const std::vector<float>& getDensitySum() const { return m_densitySum; };
	const std::vector<char>& getLines() const { return m_lines; };
	const int getReseedCounter() const { return m_reseedCounter; };

signals:
	void stepDone(); // One simulation thread done, local output vectors filled
	void simUpdated();

public slots:
	void start(); // Start thread loop
	void stop(); // Stop thread loop

	void createWindTunnel(int clDevice, int clPlatform, const QString& settingsFile); // Construct new windtunnel, (re-)initializes OpenCL
	void updateGrid(); // Update CellTypes and solid velocity from local vectors
	void setGridDimensions(const DirectX::XMUINT3& resolution, const DirectX::XMFLOAT3& voxelSize); // Update grid dimensions (empties cellTypes until next updateGrid)

private slots:
	void step(); // Thread loop

private:
	void log(const QString& msg);

	wtl::WindTunnel m_windTunnel;

	// WindTunnel input
	std::vector<wtl::CellType> m_cellTypes;
	std::vector<float> m_solidVelocity;

	// WindTunnel output
	std::vector<float> m_velocityXYZW; // DirectX needs RGBA Texture for sampling, Velocity from opencl are only 3D vectors
	std::vector<float> m_velocity;
	std::vector<float> m_density;
	std::vector<float> m_densitySum;
	std::vector<char> m_lines;
	int m_reseedCounter;

	// Grid variables
	DirectX::XMUINT3 m_resolution;
	DirectX::XMFLOAT3 m_voxelSize;

	// Simulation settings
	bool m_sim;
	bool m_simSmoke;
	bool m_simLines;

	// Thread synchronization
	QElapsedTimer m_elapsedTimer;
	QTimer m_simTimer;
	QMutex m_simMutex;
	QWaitCondition m_waitCond;
	bool m_doWait; // Indicates if thread must wait on condition
	bool m_isWaiting; // Indicates if thread is currently waiting on condition and must be woke up

	Logger* m_logger;
};

#endif