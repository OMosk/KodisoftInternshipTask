#include "ProcessLauncher.h"
#include <cstdio>
#include <tchar.h>
ProcessLauncher::ProcessLauncher(TCHAR* commandLine)
{
	this->commandLine = _tcsdup(commandLine);
	start();	
}
void ProcessLauncher::stop()
{
	//si.
	TerminateProcess(pi.hProcess, 1);	
	DWORD exitCode;
	if (GetExitCodeProcess(pi.hProcess, &exitCode))
	{
		if (exitCode == 259) processStatus = ProcessStatus::IS_WORKING;
		else
		{
			processStatus = ProcessStatus::STOPPED;
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
	else
	{
		printf("GetExitCodeProcess failed (%d).\n", GetLastError());
		return;
	}

	
}

void ProcessLauncher::start()
{
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	//LPTSTR szCmdline = _tcsdup(commandLine);

	// Start the child process. 
	if (!CreateProcess(NULL,   // No module name (use command line)
		commandLine,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return;
	}
	DWORD exitCode;
	if (GetExitCodeProcess(pi.hProcess, &exitCode))
	{
		if (exitCode == 259) processStatus = ProcessStatus::IS_WORKING;
		else processStatus = ProcessStatus::STOPPED;
	}
	else{
		printf("GetExitCodeProcess failed (%d).\n", GetLastError());
		return;
	}

}

void ProcessLauncher::restart()
{
	processStatus = ProcessStatus::RESTARTING;
	stop();
	start();
}

ProcessStatus ProcessLauncher::getStatus(){

}
ProcessLauncher::~ProcessLauncher()
{
}
