#pragma once
#include <Windows.h>
#include <thread>
#include <mutex>
#include <functional>

#include "Logger.h"
enum ProcessStatus{
	IS_WORKING, RESTARTING, STOPPED
};
class ProcessLauncher
{
	enum LastAction{
		START, STOP, RESTART
	};

	mutable std::mutex processStatusMutex, lastActionMutex, startCallbackMutex, crashCallbackMutex, manualStopCallbackMutex, processInformationMutex;

	LastAction lastAction;
	ProcessStatus processStatus;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	TCHAR *commandLine;
	
	Logger* logger;
	
	std::thread waitingThread;
	std::function<void()> onProcStart;
	std::function<void()> onProcCrash;
	std::function<void()> onProcManuallyStopped;

	bool runProc();
	bool stopProc();
	bool restartProc();
	void waitForProcess();
	void showInformation();
	LastAction getLastAction();
	void setLastAction(LastAction lastAction);
	void setProcessStatus(ProcessStatus processStatus);
public:
	ProcessLauncher(TCHAR* commandLine, bool startAtCreation = true, Logger* logger = NULL);
	bool restart();
	bool stop();
	bool start();
	ProcessStatus getStatus() const;
	HANDLE getHandle() const;
	DWORD getId() const;
	void setOnProcStart(std::function<void()> onProcStart);
	void setOnProcCrash(std::function<void()> onProcCrash);
	void setOnProcManuallyStopped(std::function<void()> onProcManuallyStopped);
	~ProcessLauncher();
};

