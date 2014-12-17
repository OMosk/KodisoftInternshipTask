#pragma once
#include <Windows.h>
#include <thread>
#include <mutex>
#include <functional>
#include <string>
#include <condition_variable>

#include "Logger.h"
enum ProcessStatus{
	IS_WORKING, RESTARTING, STOPPED
};
class ProcessMonitor
{
	enum LastAction{
		START, STOP, RESTART
	};

	mutable std::mutex processStatusMutex, 
		lastActionMutex, 
		startCallbackMutex, 
		crashCallbackMutex, 
		manualStopCallbackMutex, 
		processInformationMutex,
		operationMutex;
	std::condition_variable actionFinishedCondition;

	bool previousActionFinished;

	LastAction lastAction;
	ProcessStatus processStatus;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	TCHAR *commandLine;
	DWORD pid;
	HANDLE pHandle;
	HANDLE tHandle;
	
	Logger* logger;
	
	std::thread waitingThread;
	std::function<void()> onProcStart;
	std::function<void()> onProcCrash;
	std::function<void()> onProcManuallyStopped;

	bool runProc();
	bool stopProc();
	bool restartProc();
	void waitForProcess();
//	void showInformation();
	LastAction getLastAction();
	void setLastAction(LastAction lastAction);
	void setProcessStatus(ProcessStatus processStatus);
	void logMessage(const wchar_t* message, bool showPID = false);
	wchar_t* getCommandLineForProcessByPID(unsigned long pid);
public:
	ProcessMonitor(wchar_t* commandLine, bool startAtCreation = true, Logger* logger = NULL);
	ProcessMonitor(unsigned long pid, Logger* logger = NULL);
	bool restart();
	bool stop();
	bool start();
	ProcessStatus getStatus() const;
	HANDLE getHandle() const;
	DWORD getPID() const;
	wchar_t* getCommandLine() const;
	void setOnProcStart(std::function<void()> onProcStart);
	void setOnProcCrash(std::function<void()> onProcCrash);
	void setOnProcManuallyStopped(std::function<void()> onProcManuallyStopped);
	~ProcessMonitor();
};

