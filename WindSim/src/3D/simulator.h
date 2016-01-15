#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>

#include <Windows.h>

class Logger;

namespace Message
{
	enum MessageType { Init, UpdateDimensions, UpdateData };

	struct Resolution
	{
		int x;
		int y;
		int z;
	};
	struct VoxelSize
	{
		float x;
		float y;
		float z;
	};
}


class Simulator
{
public:
	Simulator(const std::string& cmdline = "", Logger* logger = nullptr);

	void start(const Message::Resolution& res, const Message::VoxelSize& vs); // Create Process
	void updateDimensions(const Message::Resolution& res, const Message::VoxelSize& vs); // Update VoxelGrid dimensions
	void stop(); // Exit/Terminate Process

	void update(std::vector<char>& voxelGrid); // Check signals from process (i.e. set ready if signal finished received), send voxelgrid, receive velocity grid

	bool isRunning() const { return m_running; };
	bool setCommandLine(const std::string& cmdline); // Returns if a simulator restart is necessary

private:
	void log(const std::string& msg);

	bool createProcess(const std::wstring& exe, const std::wstring& args);
	bool waitForConnect();
	bool sendToProcess(const BYTE* msg, int size);

	std::string m_executable;
	std::string m_cmdArguments;
	bool m_running;
	PROCESS_INFORMATION m_process;
	HANDLE m_pipe;
	OVERLAPPED m_overlap;

	Logger* m_logger;

};

#endif