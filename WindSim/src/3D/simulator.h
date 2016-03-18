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

#include <WindTunnelLib/WindTunnel.h>

class Logger;

class Simulator : public QObject
{
	Q_OBJECT
public:
	static void initOpenCL();

	Simulator(Logger* logger = nullptr, QObject* parent = nullptr);

	void continueSim(bool stop = false); // Continue simulation after simulation results are processed by rendering thread

	// Get vectors for writing
	std::vector<wtl::CellType>& getCellTypes() { return m_cellTypes; };
	std::vector<float>& getSolidVelocity() { return m_solidVelocity; };

	// Get vectors for reading
	const std::vector<float>& getVelocity() const { return m_velocity; };
	const std::vector<float>& getDensity() const { return m_density; };
	const std::vector<float>& getDensitySum() const { return m_densitySum; };
	const std::vector<char>& getLines() const { return m_lines; };
	const int getReseedCounter() const { return m_reseedCounter; };
	const int getNumLines() const { return m_numLines; };

signals:
	void stepDone(); // One simulation thread done, local output vectors filled
	void simUpdated();

public slots:
	void start(); // Start thread loop
	void stop(); // Stop thread loop

	void changeSimSettings(const QString& settingsFile); // Called when the json settings file changed
	bool createWindTunnel(const QString& settingsFile); // Construct new windtunnel
	void updateGrid(); // Update CellTypes and solid velocity from local vectors
	void setGridDimensions(const DirectX::XMUINT3& resolution, const DirectX::XMFLOAT3& voxelSize); // Update grid dimensions (empties cellTypes until next updateGrid)

	void runSimulation(bool enabled); // Pause/Continue simulation
	void changeSmokeSettings(const QJsonObject& settings);
	void changeLineSettings(const QJsonObject& settings);

private slots:
	void step(); // Thread loop

private:
	void log(const QString& msg);

	static QMutex m_openCLMutex;
	static int m_clDevice;
	static int m_clPlatform;
	static bool m_pauseSim;

	wtl::WindTunnel m_windTunnel;

	// WindTunnel creation parameters
	QString m_settingsFile;
	QDateTime m_fileLastModified;

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
	int m_numLines;

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
	bool m_stop;

	Logger* m_logger;
};

#endif