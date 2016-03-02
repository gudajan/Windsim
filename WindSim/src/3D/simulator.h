#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "msgDef.h"
#include "pipe.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <forward_list>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>

#include <Windows.h>

class Logger;

typedef std::tuple<std::unique_ptr<std::condition_variable>, std::unique_ptr<std::mutex>, bool> cv_tuple;
typedef std::unordered_map<MsgFromSimProc, cv_tuple> cv_map;

class Simulator
{
public:
	Simulator(const std::string& cmdline = "", Logger* logger = nullptr);

	void loop(); // Simulator thread loop

	void postMessageToSim(const MsgToSim& msg); // Posting message to simulator
	std::shared_ptr<MsgToRenderer> getMessageFromSim(); // Get message from simulator
	std::vector<std::shared_ptr<MsgToRenderer>> getAllMessagesFromSim();
	std::vector<uint32_t>& getCellGrid() { return m_cellGrid; };
	std::mutex& getVoxelGridMutex() { return m_voxelGridMutex; };
	float* getGridVel() { return m_sharedGridVel; };
	float* getVelocity() { return m_sharedVelocity; };
	std::mutex& getVelocityMutex() { return m_velocityMutex; };

	bool isRunning() const { return m_running.load(); };
	void setInitialized(bool initialized) { m_simInitialized = initialized; };

private:
	void postMessageToRender(const MsgToRenderer& msg);
	std::shared_ptr<MsgToSim> getMessageFromRender();
	std::vector<std::shared_ptr<MsgToSim>> getAllMessagesFromRender();
	bool waitForPipeMsg(MsgFromSimProc type, int secToWait);

	bool mergeSimMsg(std::shared_ptr<MsgToSim>& older, std::shared_ptr<MsgToSim>& newer);
	bool mergeRenderMsg(std::shared_ptr<MsgToRenderer>& older, std::shared_ptr<MsgToRenderer>& newer);

	void start(); // Create Process
	void initSimulation(const DimMsg& msg); // Init simulation, including voxel grid dimensions
	void stop(); // Exit/Terminate Process
	void updateGrid(); // Send updateGrid signal to simulation process
	void copyGrid();
	void fillVelocity(); // Send fillVelocity signal to simulation process

	bool setCommandLine(const std::string& cmdline); // Returns if a simulator restart is necessary

	bool createProcess(const std::wstring& exe, const std::wstring& args);
	bool createSharedMemory(int size, const std::wstring& gridName, const std::wstring& gridVelName, const std::wstring& velocityName);
	bool removeSharedMemory();

	void log(const std::string& msg);

	std::mutex m_toSimMutex;
	std::deque<std::shared_ptr<MsgToSim>> m_toSimMsgQueue;
	std::mutex m_toRenderMutex;
	std::deque<std::shared_ptr<MsgToRenderer>> m_toRenderMsgQueue;

	std::mutex m_voxelGridMutex; // Indicates that the voxel grid is somehow busy (be it the temporary cell grid or the grid in shared memory
	std::mutex m_velocityMutex;

	std::string m_executable;
	std::string m_cmdArguments;
	DimMsg::Resolution m_resolution;
	DimMsg::VoxelSize m_voxelSize;

	std::atomic_bool m_running;
	std::atomic_bool m_simInitialized;
	std::atomic_bool m_exit;

	PROCESS_INFORMATION m_process;
	Pipe m_pipe;

	cv_map m_pipeCV; // Synchronize pipe messages with waiting async commands like UpdateGrid

	HANDLE m_sharedGridHandle;
	char* m_sharedGrid;
	HANDLE m_sharedGridVelHandle;
	float* m_sharedGridVel;
	HANDLE m_sharedVelocityHandle;
	float* m_sharedVelocity;

	std::vector<uint32_t> m_cellGrid;

	Logger* m_logger;

};

#endif