#include "simulator.h"
#include "logger.h"

#include <QString>
#include <QFileInfo>
#include <QDir>

Simulator::Simulator(const std::string& exe, Logger* logger)
	: m_executable(exe),
	m_valid(false),
	m_process(),
	m_logger(logger)
{
}

void Simulator::start()
{
	std::wstring exe(m_executable.begin(), m_executable.end());

	// Check if simulator exists
	DWORD attributes = GetFileAttributes(exe.c_str());
	DWORD error = GetLastError();
	if (attributes == INVALID_FILE_ATTRIBUTES && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND))
	{
		log("WARNING: No valid Simulator executable specified. Current value is '" + m_executable + "'!\n" \
		  	"         Continuing without simulation.");
		m_valid = false;
		return;
	}

	log("INFO: Starting simulator '" + m_executable + "'...");
	m_valid = true;

	// Create windows child process
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	ZeroMemory(&m_process, sizeof(m_process));

	/*if (!CreateProcess(exe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, QFileInfo(QString::fromStdWString(exe)).absoluteDir().absolutePath().toStdWString().c_str(), &si, &m_process))
	{
		log("WARNING: Starting of simulator in '" + m_executable + "' in child process failed!\n" \
			"         Continuing without simulation.");
		m_valid = false;
		return;
	}

	CloseHandle(m_process.hThread);*/

	m_valid = true;

}

void Simulator::stop()
{
	//DWORD exitCode;
	//BOOL success = GetExitCodeProcess(m_process.hProcess, &exitCode);
	//if (exitCode == STILL_ACTIVE)
	//	// TODO: perhapse send signal to simulator, to give it a chance to exit itself (ExitProcess())
	//	TerminateProcess(m_process.hProcess, 0);

	//// Wait 10 seconds until child process exits.
	//WaitForSingleObject(m_process.hProcess, INFINITE);

	////if (m_process.hThread != NULL) // Process was successfully created
	////	CloseHandle(m_process.hThread);
	//if (m_process.hProcess != NULL)
	//	CloseHandle(m_process.hProcess);
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