#include "simulator.h"
#include "logger.h"

#include <QString>
#include <QFileInfo>
#include <QDir>

#include <DirectXMath.h>

using namespace DirectX;

Simulator::Simulator(const std::string& exe, Logger* logger)
	: m_executable(exe),
	m_running(false),
	m_process(),
	m_logger(logger)
{
}

void Simulator::start(const XMUINT3& resolution, const XMFLOAT3& voxelSize)
{
	std::wstring exe(m_executable.begin(), m_executable.end());

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

	log("INFO: Starting simulator '" + m_executable + "'...");

	// Create windows child process
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	ZeroMemory(&m_process, sizeof(m_process));

	if (!CreateProcess(NULL, const_cast<LPWSTR>(exe.c_str()), NULL, NULL, FALSE, 0, NULL, QFileInfo(QString::fromStdWString(exe)).absoluteDir().absolutePath().toStdWString().c_str(), &si, &m_process))
	{
		log("WARNING: Starting of simulator in '" + m_executable + "' in child process failed!\n" \
			"         Continuing without simulation.");
		m_running = false;
		return;
	}

	CloseHandle(m_process.hThread); // Thread handle not needed anymore
	m_process.hThread = NULL;

	m_running = true;

}

void Simulator::updateDimensions(const XMUINT3& resolution, const XMFLOAT3& voxelSize)
{

}

void Simulator::stop()
{
	// Execute only if process was created
	if (m_process.hProcess != NULL)
	{
		DWORD exitCode;
		BOOL success = GetExitCodeProcess(m_process.hProcess, &exitCode);
		if (exitCode == STILL_ACTIVE)
			// TODO: perhaps send signal to simulator, to give it a chance to exit itself (ExitProcess())
			TerminateProcess(m_process.hProcess, 0);

		// Wait until child process exits.
		WaitForSingleObject(m_process.hProcess, INFINITE);

		CloseHandle(m_process.hProcess);

		ZeroMemory(&m_process, sizeof(m_process)); // i.e. set process handle to NULL

		m_running = false;
	}
}

void Simulator::update(std::vector<char>& voxelGrid)
{

}

bool Simulator::setExecutable(const std::string& executable)
{
	if (executable == m_executable)
		return false;

	m_executable = executable;
	return true;
}

void Simulator::log(const std::string& msg)
{
	if (m_logger)
		m_logger->logit(QString::fromStdString(msg));
}