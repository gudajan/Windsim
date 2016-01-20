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
	: m_executable(),
	m_cmdArguments(),
	m_running(false),
	m_process(),
	m_pipe(NULL),
	m_overlap(),
	m_sharedHandle(NULL),
	m_sharedAddress(nullptr),
	m_logger(logger)
{
	setCommandLine(cmdline);
}

void Simulator::start(const Message::Resolution& res, const Message::VoxelSize& vs)
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
		m_running = false;
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

	// Create named pipe
	m_pipe = CreateNamedPipe(L"\\\\.\\pipe\\simulatorPipe", PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, g_bufsize, g_bufsize, 1000, NULL);
	if (m_pipe == INVALID_HANDLE_VALUE)
	{
		log("WARNING: Starting of simulator '" + m_executable + m_cmdArguments + "' failed. Failed to create named Pipe with error '" + std::to_string(GetLastError()) + "'!");
		m_running = false;
		return;
	}

	// Create windows child process
	if (!createProcess(exe, args)) return;

	if (!waitForConnect())
	{
		stop();
		return;
	}

	m_running = true;

	// Create new shared memory
	createSharedMemory(res.x * res.y * res.z * sizeof(char), L"Local\\voxelgrid");

	// Built message data
	std::vector<BYTE> data(sizeof(Message::MessageType) + sizeof(Message::Resolution) + sizeof(Message::VoxelSize), 0);
	Message::MessageType type = Message::Init;
	std::memcpy(data.data(), &type, sizeof(type));
	std::memcpy(data.data() + sizeof(type), &res, sizeof(res));
	std::memcpy(data.data() + sizeof(type) + sizeof(res), &vs, sizeof(vs));

	sendToProcess(data);
	log("INFO: Sent Init signal to simulator.");
}

void Simulator::updateDimensions(const Message::Resolution& res, const Message::VoxelSize& vs)
{
	std::vector<BYTE> data(sizeof(Message::MessageType) + sizeof(Message::Resolution) + sizeof(Message::VoxelSize), 0);
	Message::MessageType type = Message::UpdateDimensions;
	std::memcpy(data.data(), &type, sizeof(type));
	std::memcpy(data.data() + sizeof(type), &res, sizeof(res));
	std::memcpy(data.data() + sizeof(type) + sizeof(res), &vs, sizeof(vs));

	if (sendToProcess(data))
		log("INFO: Updated voxel grid dimensions of simulator.");
	else
		log("ERROR: Failed to update voxel grid dimensions of simulator! Failed to send data!");

}

void Simulator::stop()
{
	removeSharedMemory();

	// Execute only if process was created
	if (m_process.hProcess != NULL)
	{
		DWORD exitCode;
		BOOL success = GetExitCodeProcess(m_process.hProcess, &exitCode);
		if (exitCode == STILL_ACTIVE)
		{
			std::vector<BYTE> data(sizeof(Message::MessageType));
			Message::MessageType type = Message::Exit;
			std::memcpy(data.data(), &type, sizeof(Message::MessageType));
			sendToProcess(data);

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
	std::vector<BYTE> data(sizeof(Message::MessageType));
	Message::MessageType type = Message::UpdateData;
	std::memcpy(data.data(), &type, sizeof(Message::MessageType));

	sendToProcess(data);
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

void Simulator::log(const std::string& msg)
{
	if (m_logger)
		m_logger->logit(QString::fromStdString(msg));
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

bool Simulator::waitForConnect()
{
	// Wait for connection of simulator
	HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (event == NULL)
	{
		log("WARNING: Starting of simulator '" + m_executable + m_cmdArguments + "' failed. Failed to create event with error '" + std::to_string(GetLastError()) + "'!");
		m_running = false;
		return false;
	}
	m_overlap.hEvent = event;
	m_overlap.Offset = NULL;
	m_overlap.OffsetHigh = NULL;

	if (ConnectNamedPipe(m_pipe, &m_overlap))
	{
		log("WARNING: ConnectNamedPipe failed with '" + std::to_string(GetLastError()) + "'!");
		m_running = false;
		return false;
	}
	switch (GetLastError())
	{
	case ERROR_IO_PENDING:
		break;
	case ERROR_PIPE_CONNECTED:
		if (SetEvent(m_overlap.hEvent))
			break;
	default:
	{
		log("WARNING: ConnectNamedPipe failed with '" + std::to_string(GetLastError()) + "'!");
		m_running = false;
		return false;
	}
	}

	DWORD ret = WaitForSingleObject(event, INFINITE);
	if (ret != WAIT_OBJECT_0)
	{
		if (ret == WAIT_ABANDONED_0)
			log("WARNING: WaitForSingleObject abandoned wait!");
		else if (ret == WAIT_TIMEOUT)
			log("WARNING: WaitForSingleObject timed out!");
		else
			log("WARNING: WaitForSingleObject failed with '" + std::to_string(GetLastError()) + "'!");

		return false;
	}

	return true;
}

bool Simulator::sendToProcess(const std::vector<BYTE>& msg)
{
	if (msg.size() > g_bufsize)
	{
		log("ERROR: Message bigger than buffer size '" + std::to_string(g_bufsize) + "'!");
		return false;
	}

	DWORD bytesWritten;
	DWORD success = WriteFile(m_pipe, msg.data(), msg.size(), &bytesWritten, &m_overlap);

	if (success && bytesWritten == msg.size()) // Write completed
	{
		log("INFO: Message sent!");
		return true;
	}

	DWORD error = GetLastError();
	if (!success && (error == ERROR_IO_PENDING)) // Write is still pending.
	{
		log("INFO: Pending write!");
		return true;
	}

	log("ERROR: Invalid pipe state!");
	return false;
}

bool Simulator::createSharedMemory(int size, const std::wstring& name)
{
	m_sharedHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, NULL, size, name.c_str());

	if (m_sharedHandle == NULL)
	{
		log("ERROR: CreateFileMapping failed with '" + std::to_string(GetLastError()) + "'!");
		return false;
	}

	m_sharedAddress = MapViewOfFile(m_sharedHandle, FILE_MAP_ALL_ACCESS, 0, 0, size);

	if (m_sharedAddress == nullptr)
	{
		log("ERROR: MapViewOfFile failed with '" + std::to_string(GetLastError()) + "'!");
		CloseHandle(m_sharedHandle);
		return false;
	}
	return true;
}

bool Simulator::removeSharedMemory()
{
	if (m_sharedAddress)
		UnmapViewOfFile(m_sharedAddress);

	if (m_sharedHandle)
		CloseHandle(m_sharedHandle);

	return true;
}