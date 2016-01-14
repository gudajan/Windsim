#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>

#include <Windows.h>

#include <DirectXMath.h>

class Logger;

class Simulator
{
public:
	Simulator(const std::string& cmdline = "", Logger* logger = nullptr);

	void start(const DirectX::XMUINT3& resolution, const DirectX::XMFLOAT3& voxelSize); // Create Process
	void updateDimensions(const DirectX::XMUINT3& resolution, const DirectX::XMFLOAT3& voxelSize); // Update VoxelGrid dimensions
	void stop(); // Exit/Terminate Process

	void update(std::vector<char>& voxelGrid); // Check signals from process (i.e. set ready if signal finished received), send voxelgrid, receive velocity grid

	bool isRunning() const { return m_running; };
	bool setCommandLine(const std::string& cmdline); // Returns if a simulator restart is necessary

private:
	void log(const std::string& msg);

	std::string m_executable;
	std::string m_cmdArguments;
	bool m_running;
	PROCESS_INFORMATION m_process;

	Logger* m_logger;

};

#endif