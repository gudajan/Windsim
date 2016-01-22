#include "simulator.h"
#include "logger.h"

#include <QString>
#include <QFileInfo>
#include <QDir>

#include <DirectXMath.h>

#include <algorithm>

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
	m_ready(std::atomic<bool>(true)),
	m_process(),
	m_pipe(logger),
	m_sharedHandle(NULL),
	m_sharedGrid(nullptr),
	m_cellGrid(),
	m_logger(logger)
{
	setCommandLine(cmdline);
}

void Simulator::loop()
{
	bool run = true;
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
			// We are now accessing the shared (between render and simulation thread) vector containing the grid cells (4 voxel per cells) -> prevent render thread from writing to the grid cells
			// Additionally we are accessing the shared memory with the external simulation process
			// We become ready again when the external simulator process has finished accessing the shared memory, because only then we are allowed to write to the shared memory again
			m_ready = false;
			std::shared_ptr<DimMsg> dm = std::dynamic_pointer_cast<DimMsg>(msg);
			m_resolution = dm->res;
			m_voxelSize = dm->vs;
			initSimulation(*dm);
			break;
		}
		case(MsgToSim::UpdateGrid) :
		{
			m_ready = false;
			copyGrid();
			update();
			break;
		}
		case(MsgToSim::SimulatorCmd) :
		{
			std::shared_ptr<StrMsg> sm = std::dynamic_pointer_cast<StrMsg>(msg);
			if (setCommandLine(sm->str))
			{
				stop();
				start();
				m_ready = false;
				DimMsg msg;
				msg.type = DimMsg::InitSim;
				msg.res = m_resolution;
				msg.vs = m_voxelSize;
				initSimulation(msg);
			}
			break;
		}
		case(MsgToSim::Exit) :
			stop();
			run = false;
			break;
		case(MsgToSim::Empty) :
			break;
		default:
			log("WARNING: Simulation thread received invalid message!");
			break;
		}

		// Check for messages from simulation process
		std::vector<std::vector<BYTE>> data;
		if (m_pipe.receive(data))
		{
			for (auto msg : data)
			{
				MsgFromSim type;
				std::memcpy(&type, msg.data(), sizeof(MsgFromSim));
				if (type == MsgFromSim::FinishedShmAccess)
				{
					m_ready = true;
				}
				else
				{
					log("WARNING: Received invalid Message from external simulation process!");
				}
			}
		}
	}
}

void Simulator::postMessageToSim(const MsgToSim& msg)
{
	std::lock_guard<std::mutex> guard(m_toSimMutex);
	MsgToSim* m;
	if (msg.type == DimMsg::InitSim || msg.type == DimMsg::UpdateDimensions)
		m = new DimMsg(dynamic_cast<const DimMsg&>(msg));
	else if (msg.type == StrMsg::SimulatorCmd)
		m = new StrMsg(dynamic_cast<const StrMsg&>(msg));
	else
		m = new MsgToSim(msg);
	m_toSimMsgQueue.push(std::shared_ptr<MsgToSim>(m));
}

std::shared_ptr<MsgToRenderer> Simulator::getMessageFromSim()
{
	std::lock_guard<std::mutex> guard(m_toRenderMutex);
	if (m_toRenderMsgQueue.empty())
		return std::shared_ptr<MsgToRenderer>(new MsgToRenderer{ MsgToRenderer::Empty });

	std::shared_ptr<MsgToRenderer> msg = m_toRenderMsgQueue.front();
	m_toRenderMsgQueue.pop();
	return msg;
}

void Simulator::postMessageToRender(const MsgToRenderer& msg)
{
	std::lock_guard<std::mutex> guard(m_toRenderMutex);
	// Currently only empty messages
	m_toRenderMsgQueue.push(std::shared_ptr<MsgToRenderer>(new MsgToRenderer(msg)));
}

std::shared_ptr<MsgToSim> Simulator::getMessageFromRender()
{
	std::lock_guard<std::mutex> guard(m_toSimMutex);
	if (m_toSimMsgQueue.empty())
		return std::shared_ptr<MsgToSim>(new MsgToSim{ MsgToSim::Empty });

	std::shared_ptr<MsgToSim> msg = m_toSimMsgQueue.front();
	m_toSimMsgQueue.pop();
	return msg;
}

void Simulator::start()
{
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
	removeSharedMemory();
	createSharedMemory(msg.res.x * msg.res.y * msg.res.z * sizeof(char), L"Local\\voxelgrid");

	copyGrid();

	// Build message data
	// Build each member alone to remove padding within the struct
	std::vector<BYTE> data(sizeof(DimMsg::MsgType) + sizeof(DimMsg::Resolution) + sizeof(DimMsg::VoxelSize), 0);
	std::memcpy(data.data(), &msg.type, sizeof(DimMsg::MsgType));
	std::memcpy(data.data() + sizeof(DimMsg::MsgType), &msg.res, sizeof(DimMsg::Resolution));
	std::memcpy(data.data() + sizeof(DimMsg::MsgType) + sizeof(DimMsg::Resolution), &msg.vs, sizeof(DimMsg::VoxelSize));

	if (m_pipe.send(data))
		log("INFO: Sent (re-)initialization data to simulator.");
	else
		log("ERROR: Failed to (re-)initialize the simulation! Failed to send data!");
}

void Simulator::stop()
{
	removeSharedMemory();
	//TODO: close and remove named pipe

	// Execute only if process was created
	if (m_process.hProcess != NULL)
	{
		DWORD exitCode;
		BOOL success = GetExitCodeProcess(m_process.hProcess, &exitCode);
		if (exitCode == STILL_ACTIVE)
		{

			std::vector<BYTE> data(sizeof(MsgToSim::MsgType));
			MsgToSim::MsgType type = MsgToSim::Exit;
			std::memcpy(data.data(), &type, sizeof(MsgToSim::MsgType));
			if (m_pipe.send(data))
				log("INFO: Sent 'Exit'!");

			if (WaitForSingleObject(m_process.hProcess, 5000) != WAIT_OBJECT_0) // Wait for 5 seconds
				TerminateProcess(m_process.hProcess, 0);
		}

		// Wait until child process exits.
		WaitForSingleObject(m_process.hProcess, INFINITE);

		CloseHandle(m_process.hProcess);

		ZeroMemory(&m_process, sizeof(m_process)); // i.e. set process handle to NULL

		m_running = false;
	}
}

void Simulator::update()
{
	std::vector<BYTE> data(sizeof(MsgToSim::MsgType));
	MsgToSim::MsgType type = MsgToSim::UpdateGrid;
	std::memcpy(data.data(), &type, sizeof(MsgToSim::MsgType));
	if (m_pipe.send(data))
		log("INFO: Sent 'UpdateGrid'.");
}

void Simulator::copyGrid()
{
	// Lock mutex
	std::lock_guard<std::mutex> guard(m_cellGridMutex);

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
					m_sharedGrid[index + j] = static_cast<char>((cell >> (8 * j)) & 0xff); // returns the j-th voxel value of the i-th cell
				}
			}
		}
	}
	//Mutex is released
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

bool Simulator::createSharedMemory(int size, const std::wstring& name)
{
	m_sharedHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, NULL, size, name.c_str());

	if (m_sharedHandle == NULL)
	{
		log("ERROR: CreateFileMapping failed with '" + std::to_string(GetLastError()) + "'!");
		return false;
	}

	m_sharedGrid = static_cast<char*>(MapViewOfFile(m_sharedHandle, FILE_MAP_ALL_ACCESS, 0, 0, size));

	if (m_sharedGrid == nullptr)
	{
		log("ERROR: MapViewOfFile failed with '" + std::to_string(GetLastError()) + "'!");
		CloseHandle(m_sharedHandle);
		return false;
	}
	return true;
}

bool Simulator::removeSharedMemory()
{
	if (m_sharedGrid)
		UnmapViewOfFile(m_sharedGrid);

	if (m_sharedHandle)
		CloseHandle(m_sharedHandle);

	return true;
}

void Simulator::log(const std::string& msg)
{
	if (m_logger)
		m_logger->logit(QString::fromStdString(msg));
}