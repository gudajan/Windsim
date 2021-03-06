#ifndef PIPE_H
#define PIPE_H

#include <Windows.h>
#include <string>
#include <vector>
#include <mutex>

class Logger;

class Pipe
{
public:
	Pipe(Logger* logger) : m_pipe(NULL), m_read(), m_readData(g_bufferSize, 0), m_write(), m_logger(logger) {};

	bool connect(const std::wstring& name, bool create, DWORD secToWait); // WAIT/BLOCK until connected to pipe, pipe connected as overlapped for asynchronous reads
	void close(bool isServer);
	int receive(std::vector<std::vector<BYTE>>& data); // Check for messages, if messages received -> fill data and return 1, otherwise nothing received -> return 0, on error return -1
	bool send(const std::vector<BYTE>& data); // Write to pipe message and return;

	const static int g_bufferSize;
private:
	bool waitForConnect(DWORD secToWait);
	void log(const std::string& msg);

	std::mutex m_pipeMutex;

	HANDLE m_pipe;

	// Structures containing events, which get signaled by the OS if an asynchronous operation finished
	OVERLAPPED m_read;
	std::vector<BYTE> m_readData;
	OVERLAPPED m_write;

	Logger* m_logger;
};
#endif