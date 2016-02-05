#include "simulator.h"
#include "logger.h"

#include <QString>
#include <QFileInfo>
#include <QDir>

#include <DirectXMath.h>

#include <algorithm>
#include <future>

using namespace DirectX;

const static int g_bufsize = 512;

Simulator::Simulator(const std::string& cmdline, Logger* logger)
	:
	m_toSimMutex(),
	m_toSimMsgQueue(),
	m_toRenderMutex(),
	m_toRenderMsgQueue(),
	m_executable(),
	m_cmdArguments(),
	m_resolution({ 0, 0, 0 }),
	m_voxelSize({ 0.0f, 0.0f, 0.0f }),
	m_running(std::atomic<bool>(false)),
	m_simInitialized(std::atomic<bool>(false)),
	m_process(),
	m_pipe(logger),
	m_pipeCV(),
	m_sharedGridHandle(NULL),
	m_sharedGrid(nullptr),
	m_sharedVelocityHandle(NULL),
	m_sharedVelocity(nullptr),
	m_voxelGridMutex(),
	m_velocityMutex(),
	m_cellGrid(),
	m_logger(logger)
{
	// Initialize non-copyable mutexes and conditional variables without copying
	m_pipeCV.emplace(MsgFromSimProc::Initialized, cv_tuple(std::make_unique<std::condition_variable>(), std::make_unique<std::mutex>(), false));
	m_pipeCV.emplace(MsgFromSimProc::FinishedVoxelGridAccess, cv_tuple(std::make_unique<std::condition_variable>(), std::make_unique<std::mutex>(), false));
	m_pipeCV.emplace(MsgFromSimProc::FinishedVelocityAccess, cv_tuple(std::make_unique<std::condition_variable>(), std::make_unique<std::mutex>(), false));
	m_pipeCV.emplace(MsgFromSimProc::ClosedShm, cv_tuple(std::make_unique<std::condition_variable>(), std::make_unique<std::mutex>(), false));

	setCommandLine(cmdline);
}

void Simulator::loop()
{
	bool run = true;

	//
	std::forward_list<std::future<void>> msgHandlers;

	while (run)
	{
		// Check for messages from render thread
		std::shared_ptr<MsgToSim> msg = getMessageFromRender();

		switch (msg->type)
		{
		case(MsgToSim::StartProcess) :
		{
			start();
			break;
		}
		case(MsgToSim::InitSim) :
		case(MsgToSim::UpdateDimensions) :
		{
			if (isRunning())
			{
				msgHandlers.push_front(std::async(std::launch::async, &Simulator::initSimulation, this, *std::dynamic_pointer_cast<DimMsg>(msg)));
			}
			break;
		}
		case(MsgToSim::UpdateGrid) :
		{
			if (isRunning())
			{
				if (m_simInitialized.load())
				{
					// Intellisense shows return type of async as future<void(&)()>, which is wrong. Instead a future<void> is returned
					msgHandlers.push_front(std::async(std::launch::async, &Simulator::updateGrid, this));
				}
			}
			break;
		}
		case(MsgToSim::FinishedVelocityAccess) :
		{
			if (isRunning())
			{
				if (m_simInitialized.load())
				{
					msgHandlers.push_front(std::async(std::launch::async, &Simulator::fillVelocity, this));
				}
			}
			break;
		}
		case(MsgToSim::SimulatorCmd) :
		{
			// At this point, the command line is already changed. We would not receive the message if it was irrelevant (i.e. same command line)
			// Wait until all futures finished, so now async operation fails, when shared memory is removed
			std::for_each(msgHandlers.begin(), msgHandlers.end(), [](std::future<void>& f){f.get(); });
			msgHandlers.clear();
			stop();
			start();
			if (isRunning())
			{
				auto simMsg = std::dynamic_pointer_cast<StrMsg>(msg);
				DimMsg dimMsg;
				dimMsg.type = DimMsg::InitSim;
				dimMsg.res = simMsg->res;
				dimMsg.vs = simMsg->vs;
				msgHandlers.push_front(std::async(std::launch::async, &Simulator::initSimulation, this, dimMsg));
			}
			break;
		}
		case(MsgToSim::Exit) :
			// Wait until all futures finished
			std::for_each(msgHandlers.begin(), msgHandlers.end(), [](std::future<void>& f){f.get(); });
			msgHandlers.clear();
			stop();
			run = false;
			break;
		case(MsgToSim::Empty) :
			break;
		default:
			log("WARNING: Simulation thread received invalid message!");
			break;
		}

		// Clear old async threads, which are finished
		msgHandlers.remove_if([](std::future<void>& f){ if (f.wait_for(std::chrono::seconds(0)) == std::future_status::ready) { f.get(); return true; } return false; });

		// Check for messages from named pipe and signal conditional variable
		std::vector<std::vector<BYTE>> data;
		if (m_pipe.receive(data) > 0) // Message received
		{
			for (auto msg : data)
			{
				MsgFromSimProc t;
				std::memcpy(&t, msg.data(), sizeof(MsgFromSimProc));

				cv_tuple& cv = m_pipeCV[t];
				{
					std::lock_guard<std::mutex> lock(*std::get<1>(cv));
					std::get<2>(cv) = true;// Set bool to signaled
				}
				std::get<0>(cv)->notify_one(); // Notify condition variable
			}
		}

	}
}

void Simulator::postMessageToSim(const MsgToSim& msg)
{
	std::lock_guard<std::mutex> guard(m_toSimMutex);

	std::shared_ptr<MsgToSim> m;
	if (msg.type == DimMsg::InitSim || msg.type == DimMsg::UpdateDimensions)
		m = std::shared_ptr<MsgToSim>(new DimMsg(dynamic_cast<const DimMsg&>(msg)));
	else if (msg.type == StrMsg::SimulatorCmd)
		m = std::shared_ptr<MsgToSim>(new StrMsg(dynamic_cast<const StrMsg&>(msg)));
	else
		m = std::shared_ptr<MsgToSim>(new MsgToSim(msg));

	// Merge message queue as far as possible
	while (true)
	{
		if (m_toSimMsgQueue.empty())
		{
			// Still check if new message relevant
			if (mergeSimMsg(std::shared_ptr<MsgToSim>(new MsgToSim(MsgToSim::Empty)), m))
			{
				if (m->type != MsgToSim::Empty)
					m_toSimMsgQueue.push_back(m);
			}
			break;
		}
		std::shared_ptr<MsgToSim> oldMsg = m_toSimMsgQueue.back();
		m_toSimMsgQueue.pop_back();
		if (!mergeSimMsg(oldMsg, m))
		{
			m_toSimMsgQueue.push_back(oldMsg);
			m_toSimMsgQueue.push_back(m);
			break;
		}
	}
}

std::shared_ptr<MsgToRenderer> Simulator::getMessageFromSim()
{
	std::lock_guard<std::mutex> guard(m_toRenderMutex);
	if (m_toRenderMsgQueue.empty())
		return std::shared_ptr<MsgToRenderer>(new MsgToRenderer{ MsgToRenderer::Empty });

	std::shared_ptr<MsgToRenderer> msg = m_toRenderMsgQueue.front();
	m_toRenderMsgQueue.pop_front();
	return msg;
}

std::vector<std::shared_ptr<MsgToRenderer>> Simulator::getAllMessagesFromSim()
{
	std::lock_guard<std::mutex> guard(m_toRenderMutex);
	std::vector<std::shared_ptr<MsgToRenderer>> messages;
	messages.resize(m_toRenderMsgQueue.size(), nullptr);
	std::copy(m_toRenderMsgQueue.begin(), m_toRenderMsgQueue.end(), messages.begin());
	m_toRenderMsgQueue.clear();
	return messages;
}

void Simulator::postMessageToRender(const MsgToRenderer& msg)
{
	std::lock_guard<std::mutex> guard(m_toRenderMutex);

	std::shared_ptr<MsgToRenderer> m(new MsgToRenderer(msg));

	// Merge message queue as far as possible
	while (true)
	{
		if (m_toRenderMsgQueue.empty())
		{
			m_toRenderMsgQueue.push_back(m);
			break;
		}
		std::shared_ptr<MsgToRenderer> oldMsg = m_toRenderMsgQueue.back();
		m_toRenderMsgQueue.pop_back();
		if (!mergeRenderMsg(oldMsg, m))
		{
			m_toRenderMsgQueue.push_back(oldMsg);
			m_toRenderMsgQueue.push_back(m);
			break;
		}
	}
}

std::shared_ptr<MsgToSim> Simulator::getMessageFromRender()
{
	std::lock_guard<std::mutex> guard(m_toSimMutex);
	if (m_toSimMsgQueue.empty())
		return std::shared_ptr<MsgToSim>(new MsgToSim{ MsgToSim::Empty });

	std::shared_ptr<MsgToSim> msg = m_toSimMsgQueue.front();
	m_toSimMsgQueue.pop_front();
	return msg;
}

std::vector<std::shared_ptr<MsgToSim>> Simulator::getAllMessagesFromRender()
{
	std::lock_guard<std::mutex> guard(m_toSimMutex);
	std::vector<std::shared_ptr<MsgToSim>> messages;
	messages.resize(m_toSimMsgQueue.size(), nullptr);
	std::copy(m_toSimMsgQueue.begin(), m_toSimMsgQueue.end(), messages.begin());
	m_toSimMsgQueue.clear();
	return messages;
}

bool Simulator::waitForPipeMsg(MsgFromSimProc type, int secToWait)
{
	cv_tuple& cv = m_pipeCV[type];
	std::unique_lock<std::mutex> lock(*std::get<1>(cv));
	bool& signaled = std::get<2>(cv);
	if (!std::get<0>(cv)->wait_for(lock, std::chrono::seconds(secToWait), [&signaled](){ return signaled; }))
	{
		log("ERROR: Did not recieve expected message '" + std::to_string(static_cast<int>(type)) + "' from simulation processs!");
		return false;
	}
	std::get<2>(cv) = false; // Unset the predicate
	return true;
}

// Returns true if messages merged, merged message found in argument 'newer'
// Returns false if messages not merged, arguments remain unchanged
bool Simulator::mergeSimMsg(std::shared_ptr<MsgToSim>& older, std::shared_ptr<MsgToSim>& newer)
{
	// Merge rules:
	// old    - new
	// !Sim   - !Sim    -> return newer !Sim
	// *      - Empty   -> return *
	// Empty  - !Sim    -> return !Sim
	// Exit   - *       -> return exit
	// *      - Exit    -> return exit
	// Sim    - Dim     -> update dimensions of Sim, return Sim
	// Sim    - *       -> return Sim
	// *      - Sim     -> Check if Sim is relevant, if so return Sim else *
	// Start  - Dim     -> return both
	// Start  - Update  -> return both
	// start  - Fin     -> return both
	// *      - start   -> return both
	// Dim    - Update  -> return Dim
	// Dim    - Fin     -> return both
	// *      - Dim     -> return Dim
	// Update - Fin     -> return both
	// Fin    - Update  -> return both
	// *      - *       -> return both

	if (older->type == newer->type && older->type != MsgToSim::SimulatorCmd)
		return true;
	if (older->type == MsgToSim::Empty && newer->type != MsgToSim::SimulatorCmd)
		return true;
	if (newer->type == MsgToSim::Empty)
	{
		newer = older;
		return true;
	}
	if (older->type == MsgToSim::Exit)
	{
		newer = older;
		return true;
	}
	if (newer->type == MsgToSim::Exit)
		return true;
	if (older->type == MsgToSim::SimulatorCmd && (newer->type == MsgToSim::InitSim || newer->type == MsgToSim::UpdateDimensions))
	{
		auto tmpOld = std::dynamic_pointer_cast<StrMsg>(older);
		auto tmpNew = std::dynamic_pointer_cast<DimMsg>(newer);

		tmpOld->res = tmpNew->res;
		tmpOld->vs = tmpNew->vs;

		newer = older;
		return true;
	}
	if (older->type == MsgToSim::SimulatorCmd)
	{
		newer = older;
		return true;
	}
	if (newer->type == MsgToSim::SimulatorCmd)
	{
		auto tmpNew = std::dynamic_pointer_cast<StrMsg>(newer);
		if (setCommandLine(tmpNew->str)) // If Sim msg relevant
			return true;

		newer = older;
		return true;
	}
	if ((older->type == MsgToSim::InitSim || older->type == MsgToSim::UpdateDimensions) && newer->type == MsgToSim::UpdateGrid)
	{
		newer = older;
		return true;
	}
	if (newer->type == MsgToSim::InitSim || newer->type == MsgToSim::UpdateDimensions)
		return true;

	return false; // return both
}

bool Simulator::mergeRenderMsg(std::shared_ptr<MsgToRenderer>& older, std::shared_ptr<MsgToRenderer>& newer)
{
	// Merge rules
	// T - T -> return newer T
	// * - Empty -> return *
	// Empty - * -> return *
	// * - * -> return both

	if (older->type == newer->type)
		return true;
	if (older->type == MsgToRenderer::Empty)
		return true;
	if (newer->type == MsgToRenderer::Empty)
	{
		newer = older;
		return true;
	}

	return false;
}



void Simulator::start()
{
	OutputDebugStringA("Start simulation process!\n");

	std::wstring exe(m_executable.begin(), m_executable.end());
	std::wstring args(m_cmdArguments.begin(), m_cmdArguments.end());

	// Check if simulator exists
	DWORD attributes = GetFileAttributes(exe.c_str());
	DWORD error = GetLastError();
	if (attributes == INVALID_FILE_ATTRIBUTES && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND))
	{
		log("WARNING: No valid Simulator executable specified. Current value is '" + m_executable + "'!\n" \
		  	"         Continuing without simulation.");
		return;
	}

	if (m_process.hProcess != NULL)
	{
		DWORD exitCode;
		BOOL success = GetExitCodeProcess(m_process.hProcess, &exitCode);
		if (exitCode == STILL_ACTIVE)
		{
			log("ERROR: Simulator already running! Omit starting new process.");
			return;
		}
	}

	log("INFO: Starting simulator '" + m_executable + m_cmdArguments + "'...");

	// Create windows child process
	if (!createProcess(exe, args)) return;

	// Create named pipe and wait/block for connection
	if (!m_pipe.connect(L"\\\\.\\pipe\\simulatorPipe", true))
		return;

	m_running = true;
}

void Simulator::initSimulation(const DimMsg& msg)
{
	std::lock_guard<std::mutex> guard1(m_voxelGridMutex);
	std::lock_guard<std::mutex> guard2(m_velocityMutex);

	OutputDebugStringA("Init simulation process\n");

	m_simInitialized = false;

	// Send CloseShm message
	std::vector<BYTE> data(sizeof(MsgToSimProc));
	MsgToSimProc type = MsgToSimProc::CloseShm;
	std::memcpy(data.data(), &type, sizeof(MsgToSimProc));
	if (m_pipe.send(data))
	{
		log("INFO: Sent 'CloseShm'.");
	}
	else
	{
		OutputDebugStringA("Init simulation process, remove locks\n");
		return;
	}

	if (!waitForPipeMsg(MsgFromSimProc::ClosedShm, 5))
	{
		OutputDebugStringA("Init simulation process, remove locks\n");
		log("ERROR: Did not receive the confirmation for closing the shared memory!");
		return;
	}

	removeSharedMemory();
	if (!createSharedMemory(msg.res.x * msg.res.y * msg.res.z, L"Local\\voxelgrid", L"Local\\velocity"))
	{
		OutputDebugStringA("Init simulation process, remove locks\n");
		log("ERROR: Failed to create shared memory! Please try to reinitialize the simulation.");
		return;
	}

	m_resolution = msg.res;
	m_voxelSize = msg.vs;

	copyGrid();

	// Build message data
	// Build each member alone to remove padding within the struct
	data = std::vector<BYTE>(sizeof(DimMsg::MsgType) + sizeof(DimMsg::Resolution) + sizeof(DimMsg::VoxelSize), 0);
	std::memcpy(data.data(), &msg.type, sizeof(DimMsg::MsgType));
	std::memcpy(data.data() + sizeof(DimMsg::MsgType), &msg.res, sizeof(DimMsg::Resolution));
	std::memcpy(data.data() + sizeof(DimMsg::MsgType) + sizeof(DimMsg::Resolution), &msg.vs, sizeof(DimMsg::VoxelSize));

	if (m_pipe.send(data))
		log("INFO: Sent (re-)initialization data to simulator.");
	else
	{
		log("ERROR: Failed to (re-)initialize the simulation! Failed to send data!");
	}

	if(!waitForPipeMsg(MsgFromSimProc::Initialized, 60))
	{
		log("ERROR: Did not receive the confirmation for Initialization!");
		OutputDebugStringA("Init simulation process, remove locks\n");
		return;
	}

	m_simInitialized = true;

	// Start the velocity exchange
	if (msg.type == MsgToSim::InitSim)
		postMessageToSim({ MsgToSim::FinishedVelocityAccess });

	//Mutexes unlocked
	OutputDebugStringA("Init simulation process, remove locks\n");
}

void Simulator::stop()
{
	OutputDebugStringA("Stop simulation process!\n");

	// Execute only if process was created
	if (m_process.hProcess != NULL)
	{
		DWORD exitCode;
		BOOL success = GetExitCodeProcess(m_process.hProcess, &exitCode);
		if (exitCode == STILL_ACTIVE)
		{

			std::vector<BYTE> data(sizeof(MsgToSimProc));
			MsgToSimProc type = MsgToSimProc::Exit;
			std::memcpy(data.data(), &type, sizeof(MsgToSimProc));
			if (m_pipe.send(data))
				log("INFO: Sent 'Exit'!");
			if (WaitForSingleObject(m_process.hProcess, 5000) != WAIT_OBJECT_0) // Wait for 5 seconds
				TerminateProcess(m_process.hProcess, 0);
			m_pipe.close(true);
		}

		// Wait until child process exits.
		WaitForSingleObject(m_process.hProcess, INFINITE);

		CloseHandle(m_process.hProcess);

		ZeroMemory(&m_process, sizeof(m_process)); // i.e. set process handle to NULL

		m_running = false;
	}

	removeSharedMemory();

	m_simInitialized = false;
}

void Simulator::updateGrid()
{
	std::lock_guard<std::mutex> guard(m_voxelGridMutex);

	OutputDebugStringA("Update simulation grid!\n");

	copyGrid();

	std::vector<BYTE> data(sizeof(MsgToSim::MsgType));
	MsgToSim::MsgType type = MsgToSim::UpdateGrid;
	std::memcpy(data.data(), &type, sizeof(MsgToSim::MsgType));
	if (m_pipe.send(data))
		log("INFO: Sent 'UpdateGrid'.");
	else
		return;

	// Wait until voxelgrid access finished
	waitForPipeMsg(MsgFromSimProc::FinishedVoxelGridAccess, 5);

}

void Simulator::copyGrid()
{
	OutputDebugStringA("Copy voxel grid to shared memory!\n");

	if (!m_sharedGrid)
	{
		return;
	}

	// Copy data into grid in shared memory and decode 32 bit cells into voxels
	uint32_t xRes = m_resolution.x / 4;
	uint32_t slice = xRes * m_resolution.y;
	uint32_t realSlice = m_resolution.x * m_resolution.y;
	uint32_t cell = 0;
	uint32_t index = 0;
	for (int z = 0; z < m_resolution.z; ++z)
	{
		for (int y = 0; y < m_resolution.y; ++y)
		{
			for (int x = 0; x < xRes; ++x)
			{
				cell = m_cellGrid[x + y * xRes + z * slice]; // Index in encoded grid
				index = x * 4 + y * m_resolution.x + z * realSlice; // Index in decoded grid
				for (int j = 0; j < 4; ++j)
				{
					m_sharedGrid[index + j] = static_cast<char>((cell >> (8 * j)) & 0xff); // returns the j-th voxel value of the cell
				}
			}
		}
	}
}

void Simulator::fillVelocity()
{
	std::lock_guard<std::mutex> guard(m_velocityMutex);

	std::vector<BYTE> data(sizeof(MsgToSimProc));
	MsgToSimProc type = MsgToSimProc::FillVelocity;
	std::memcpy(data.data(), &type, sizeof(MsgToSimProc));
	m_pipe.send(data);
	//log("DEBUG: Sent 'FillVelocity'");

	waitForPipeMsg(MsgFromSimProc::FinishedVelocityAccess, 5);

	postMessageToRender({ MsgToRenderer::UpdateVelocity });
}

bool Simulator::setCommandLine(const std::string& cmdline)
{
	std::string exe = "";
	std::string args = "";

	// Split commandline into executable and arguments
	if (cmdline[0] == '\"')
	{
		auto it = std::find(std::next(cmdline.begin()), cmdline.end(), '\"');
		if (it == cmdline.end())
			log("WARNING: No valid Simulator command line specified. Unbalanced quotes in '" + cmdline + "'!\n");
		else
		{
			exe = std::string(cmdline.begin(), it);
			args = std::string(it, cmdline.end());
		}
	}
	else
	{
		auto it = std::find(cmdline.begin(), cmdline.end(), ' ');
		if (it == cmdline.end())
			exe = cmdline;
		else
		{
			exe = std::string(cmdline.begin(), it);
			args = std::string(it, cmdline.end());
		}
	}

	if (exe == m_executable && args == m_cmdArguments)
		return false;

	m_executable = exe;
	m_cmdArguments = args;
	return true;
}

bool Simulator::createProcess(const std::wstring& exe, const std::wstring& args)
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	ZeroMemory(&m_process, sizeof(m_process));

	if (!CreateProcess(NULL, const_cast<LPWSTR>((exe + args).c_str()), NULL, NULL, FALSE, 0, NULL, QFileInfo(QString::fromStdWString(exe)).absoluteDir().absolutePath().toStdWString().c_str(), &si, &m_process))
	{
		log("WARNING: Starting of simulator '" + m_executable + m_cmdArguments + "' in child process failed with error '" + std::to_string(GetLastError()) + "'!\n" \
			"         Continuing without simulation.");
		m_running = false;
		return false;
	}

	CloseHandle(m_process.hThread); // Thread handle not needed anymore
	m_process.hThread = NULL;

	return true;
}

bool Simulator::createSharedMemory(int size, const std::wstring& gridName, const std::wstring& velocityName)
{
	OutputDebugStringA(("Create shared memory with size " + std::to_string(size) + "!\n").c_str());

	// Open grid from shared memory
	m_sharedGridHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, NULL, size * sizeof(char), gridName.c_str());
	if (m_sharedGridHandle == NULL)
	{
		log("ERROR: CreateFileMapping faild to open shared memory with '" + std::to_string(GetLastError()) + "'!");
		return false;
	}

	m_sharedGrid = static_cast<char*>(MapViewOfFile(m_sharedGridHandle, FILE_MAP_WRITE, 0, 0, size * sizeof(char)));
	if (m_sharedGrid == nullptr)
	{
		log("ERROR: MapViewOfFile failed to create view of shared memory with '" + std::to_string(GetLastError()) + "'!");
		CloseHandle(m_sharedGridHandle);
		return false;
	}

	m_sharedVelocityHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, NULL, size * sizeof(float) * 3, velocityName.c_str());
	if (m_sharedVelocityHandle == NULL)
	{
		log("ERROR: CreateFileMapping faild to open shared memory with '" + std::to_string(GetLastError()) + "'!");
		return false;
	}

	m_sharedVelocity = static_cast<float*>(MapViewOfFile(m_sharedVelocityHandle, FILE_MAP_READ, 0, 0, size * sizeof(float) * 3));
	if (m_sharedVelocity == nullptr)
	{
		log("ERROR: MapViewOfFile failed to create view of shared memory with '" + std::to_string(GetLastError()) + "'!");
		CloseHandle(m_sharedVelocityHandle);
		return false;
	}

	OutputDebugStringA("Create shared memory done!\n");
	return true;
}

bool Simulator::removeSharedMemory()
{
	OutputDebugStringA("Remove shared memory!\n");

	bool error = false;
	if (m_sharedGrid)
	{
		BOOL success = UnmapViewOfFile(m_sharedGrid);
		if (!success)
		{
			log("ERROR: UnmapViewOfFile failed with '" + std::to_string(GetLastError()) + "'!");
			error = true;
		}
	}
	m_sharedGrid = nullptr;

	if (m_sharedVelocity)
	{
		BOOL success = UnmapViewOfFile(m_sharedVelocity);
		if (!success)
		{
			log("ERROR: UnmapViewOfFile failed with '" + std::to_string(GetLastError()) + "'!");
			error = true;
		}
	}
	m_sharedVelocity = nullptr;

	if (m_sharedGridHandle)
	{
		BOOL success = CloseHandle(m_sharedGridHandle);
		if (!success)
		{
			log("ERROR: CloseHandle failed with '" + std::to_string(GetLastError()) + "'!");
			error = true;
		}
	}
	m_sharedGridHandle = NULL;

	if (m_sharedVelocityHandle)
	{
		BOOL success = CloseHandle(m_sharedVelocityHandle);
		if (!success)
		{
			log("ERROR: CloseHandle failed with '" + std::to_string(GetLastError()) + "'!");
			error = true;
		}
	}
	m_sharedVelocityHandle = NULL;

	return !error;
}

void Simulator::log(const std::string& msg)
{
	if (m_logger)
		m_logger->logit(QString::fromStdString(msg));
}