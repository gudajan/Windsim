#include "pipe.h"
#include "logger.h"

#include <iostream>

const int Pipe::g_bufferSize = 512;

bool Pipe::connect(const std::wstring& name, bool create)
{
	if (create)
	{
		// Create named pipe and wait for connection
		m_pipe = CreateNamedPipe(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, g_bufferSize, g_bufferSize, 1000, NULL);
		if (m_pipe == INVALID_HANDLE_VALUE)
		{
			log("WARNING: Failed to create named Pipe with error '" + std::to_string(GetLastError()) + "'!");
			return false;
		}

		if (!waitForConnect())
			return false;
	}
	else
	{
		// Connect to pipe
		m_pipe = CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		if (m_pipe == INVALID_HANDLE_VALUE)
		{
			log("ERROR: Failed to connect to named pipe. Error : " + std::to_string(GetLastError()));
			return false;
		}

		DWORD mode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
		BOOL success = SetNamedPipeHandleState(m_pipe, &mode, NULL, NULL);
		if (!success)
		{
			log("ERROR: Failed to set named pipe handle state. Error : " + std::to_string(GetLastError()));
			return false;
		}
	}

	// Initialize overlapped structures and events for async/overlapped reads and writes
	ZeroMemory(&m_read, sizeof(OVERLAPPED));

	HANDLE event = CreateEvent(NULL, FALSE, TRUE, NULL); // Initialized with signaled, automatically reset
	if (event == NULL)
	{
		log("ERROR: Failed to create read event. Error : " + std::to_string(GetLastError()));
		return false;
	}
	m_read.hEvent = event;

	event = CreateEvent(NULL, FALSE, TRUE, NULL); // Initialized with signaled, automatically reset
	if (event == NULL)
	{
		log("ERROR: Failed to create write event. Error : " + std::to_string(GetLastError()));
		return false;
	}
	m_write.hEvent = event;

	return true;
}

void Pipe::close(bool isServer)
{
	if (isServer)
	{
		BOOL success = DisconnectNamedPipe(m_pipe);
		if (!success)
			log("WARNING: DisconnectNamedPipe failed with '" + std::to_string(GetLastError()) + "'!");
	}

	CloseHandle(m_pipe);
	m_pipe = NULL;
	ZeroMemory(&m_read, sizeof(OVERLAPPED));
	ZeroMemory(&m_write, sizeof(OVERLAPPED));
}

int Pipe::receive(std::vector<std::vector<BYTE>>& data)
{
	std::lock_guard<std::mutex> guard(m_pipeMutex);

	if (!m_pipe)
		return -1;

	int readData = 0;

	// Check if currently pending reads
	if (!HasOverlappedIoCompleted(&m_read))
	{
		return 0;
	}

	// No pending reads -> Get last read result
	DWORD bytesRead;
	BOOL success = GetOverlappedResult(m_pipe, &m_read, &bytesRead, FALSE);
	if (!success)
	{
		DWORD error = GetLastError();
		log("ERROR: GetOverlappedResult failed. Error : " + std::to_string(error));
		if (error == ERROR_BROKEN_PIPE) // Pipe disconnected/crashed
			close(false);
		return -1;
	}

	// Last read operation finished and read a message (stored in m_readData)
	if (bytesRead > 0)
	{
		data.push_back(std::vector<BYTE>(bytesRead, 0));
		std::copy(m_readData.begin(), std::next(m_readData.begin(), bytesRead), data[0].begin());
		readData = 1;
	}

	// Start next async read operation
	bool moreMessages = true;
	while (moreMessages)
	{
		success = ReadFile(m_pipe, m_readData.data(), g_bufferSize, &bytesRead, &m_read);
		if (success && bytesRead > 0) // Immediately read new messages
		{
			data.push_back(std::vector<BYTE>(bytesRead, 0));
			std::copy(m_readData.begin(), std::next(m_readData.begin(), bytesRead), data[data.size()-1].begin());
			readData = 1;
			continue;
		}
		DWORD error = GetLastError();
		if (!success && (error == ERROR_IO_PENDING)) // Expected: currently no message -> start async wait
		{
			moreMessages = false;
		}
		else
		{
			log("ERROR: ReadFile failed. Error : " + std::to_string(error));
			moreMessages = false;
		}
	}
	return readData;
}

bool Pipe::send(const std::vector<BYTE>& msg)
{
	std::lock_guard<std::mutex> guard(m_pipeMutex);

	if (!m_pipe)
		return false;

	if (msg.size() > g_bufferSize)
	{
		log("ERROR: Message bigger than buffer size '" + std::to_string(g_bufferSize) + "'!");
		return false;
	}

	DWORD bytesWritten;
	BOOL success = WriteFile(m_pipe, msg.data(), static_cast<DWORD>(msg.size()), &bytesWritten, &m_write);

	if (success && bytesWritten == msg.size()) // Write completed
	{
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

bool Pipe::waitForConnect()
{
	// Wait for connection of simulator
	HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (event == NULL)
	{
		log("WARNING: Failed to create event for connection with error '" + std::to_string(GetLastError()) + "'!");
		return false;
	}
	OVERLAPPED overlap;
	overlap.hEvent = event;
	overlap.Offset = NULL;
	overlap.OffsetHigh = NULL;

	if (ConnectNamedPipe(m_pipe, &overlap))
	{
		log("WARNING: ConnectNamedPipe failed with '" + std::to_string(GetLastError()) + "'!");
		return false;
	}
	switch (GetLastError())
	{
	case ERROR_IO_PENDING:
		break;
	case ERROR_PIPE_CONNECTED:
		if (SetEvent(overlap.hEvent))
			break;
	default:
		log("WARNING: ConnectNamedPipe failed with '" + std::to_string(GetLastError()) + "'!");
		return false;
	}

	DWORD ret = WaitForSingleObject(event, INFINITE);
	if (ret != WAIT_OBJECT_0)
	{
		if (ret == WAIT_ABANDONED_0)
			log("WARNING: WaitForSingleObject (connection) abandoned wait!");
		else if (ret == WAIT_TIMEOUT)
			log("WARNING: WaitForSingleObject (connection) timed out!");
		else
			log("WARNING: WaitForSingleObject (connection) failed with '" + std::to_string(GetLastError()) + "'!");

		return false;
	}

	return true;
}

void Pipe::log(const std::string& msg)
{
	if (m_logger)
		m_logger->logit(QString::fromStdString(msg));
}