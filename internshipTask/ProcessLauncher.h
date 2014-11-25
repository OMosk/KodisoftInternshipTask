#pragma once
#include <Windows.h>
enum ProcessStatus{
	IS_WORKING, RESTARTING, STOPPED
};
class ProcessLauncher
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	TCHAR *commandLine;
	ProcessStatus processStatus;
public:
	ProcessLauncher(TCHAR* commandLine);
	void restart();
	void stop();
	void start();
	ProcessStatus getStatus();
	~ProcessLauncher();
};

