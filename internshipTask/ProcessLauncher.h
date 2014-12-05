#pragma once
#include <Windows.h>
#include <thread>
#include <functional>
enum ProcessStatus{
	IS_WORKING, RESTARTING, STOPPED
};
class ProcessLauncher
{
	enum LastAction{
		START, STOP, RESTART
	};
	LastAction lastAction;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	TCHAR *commandLine;
	ProcessStatus processStatus;
	
	std::thread waitingThread;
	std::function<void()> onProcStart;
	std::function<void()> onProcCrash;
	std::function<void()> onProcManuallyStopped;

	bool runProc();
	bool stopProc();
	bool restartProc();
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
	void setOnProcStart(std::function<void()> onProcStart);
	void setOnProcCrash(std::function<void()> onProcCrash);
	void setOnProcManuallyStopped(std::function<void()> onProcManuallyStopped);
	~ProcessLauncher();
};

