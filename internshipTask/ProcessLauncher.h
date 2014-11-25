#pragma once
#include <Windows.h>
#include <thread>
enum ProcessStatus{
	IS_WORKING, RESTARTING, STOPPED
};
class ProcessLauncher
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	TCHAR *commandLine;
	ProcessStatus processStatus;
	std::thread *startProcess;
	std::thread *waitingThread;
	void waitForProcess();
	void showInformation();
public:
	ProcessLauncher(TCHAR* commandLine);
	void restart();
	void stop();
	void start();
	ProcessStatus getStatus();
	HANDLE getHandle();
	DWORD getId();
	~ProcessLauncher();
};

