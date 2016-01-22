#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "msgDef.h"
#include "pipe.h"

#include <string>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <memory>

#include <Windows.h>

class Logger;

class Simulator
{
public:
	Simulator(const std::string& cmdline = "", Logger* logger = nullptr);

	void loop(); // Simulator thread loop

	void postMessageToSim(const MsgToSim& msg); // Posting message to simulator
	std::shared_ptr<MsgToRenderer> getMessageFromSim(); // Get message from simulator
	std::vector<uint32_t>& getCellGrid() { return m_cellGrid; };
	std::mutex& getCellGridMutex() { return m_cellGridMutex; }

	bool isRunning() const { return m_running.load(); };
	bool isReady() const { return m_ready.load(); };

private:
	void postMessageToRender(const MsgToRenderer& msg);
	std::shared_ptr<MsgToSim> getMessageFromRender();

	void start(); // Create Process
	void initSimulation(const DimMsg& msg); // Init simulation, including voxel grid dimensions
	void stop(); // Exit/Terminate Process
	void update(); // Send updateGrid signal to simulation process
	void copyGrid();

	bool setCommandLine(const std::string& cmdline); // Returns if a simulator restart is necessary

	bool createProcess(const std::wstring& exe, const std::wstring& args);
	bool createSharedMemory(int size, const std::wstring& name);
	bool removeSharedMemory();

	void log(const std::string& msg);

	std::mutex m_toSimMutex;
	std::queue<std::shared_ptr<MsgToSim>> m_toSimMsgQueue;
	std::mutex m_toRenderMutex;
	std::queue<std::shared_ptr<MsgToRenderer>> m_toRenderMsgQueue;

	std::string m_executable;
	std::string m_cmdArguments;
	DimMsg::Resolution m_resolution;
	DimMsg::VoxelSize m_voxelSize;

	std::atomic_bool m_running;
	std::atomic_bool m_ready;

	PROCESS_INFORMATION m_process;
	Pipe m_pipe;

	HANDLE m_sharedHandle;
	char* m_sharedGrid;

	std::mutex m_cellGridMutex;
	std::vector<uint32_t> m_cellGrid;

	Logger* m_logger;

};

#endif