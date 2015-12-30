#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>

#include <Windows.h>

class Logger;

class Simulator
{
public:
	Simulator(const std::string& executable = "", Logger* logger = nullptr);

	void start();
	void stop();

	void update(std::vector<char>& voxelGrid); // Check signals from process (i.e. set ready if signal finished received), send voxelgrid, receive velocity grid

	bool isValid() const { return m_valid; };
	bool setExecutable(const std::string& executable); // Returns if a simulator restart is necessary

private:
	void log(const std::string& msg);

	std::string m_executable;
	bool m_valid;
	PROCESS_INFORMATION m_process;

	Logger* m_logger;

};

#endif