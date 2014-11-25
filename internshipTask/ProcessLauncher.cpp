#include "ProcessLauncher.h"
#include <cstdio>
#include <tchar.h>
#include <thread>
#include <iostream>
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
	printf("start()\n");
	showInformation();
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
	//std::thread  waiting
	//waitingThread = new std::thread({ waitForProcess(); });
	waitingThread = new std::thread(&ProcessLauncher::waitForProcess, this);
}

void ProcessLauncher::restart()
{
	
	stop();
	processStatus = ProcessStatus::RESTARTING;
	start();
}

ProcessStatus ProcessLauncher::getStatus(){
	return processStatus;
}
HANDLE ProcessLauncher::getHandle(){
	return pi.hProcess;
}
DWORD ProcessLauncher::getId(){
	return pi.dwProcessId;
}
ProcessLauncher::~ProcessLauncher()
{
}
void ProcessLauncher::waitForProcess(){
	WaitForSingleObject(pi.hProcess, INFINITE);
	printf("process closed by itself\n");
	//std::thread startProcessThread(&ProcessLauncher::start, this);
	start();
}
void ProcessLauncher::showInformation(){
	std::cout << getId() << " " << getHandle() << std::endl;
	switch (getStatus()){
	case	ProcessStatus::IS_WORKING:
		std::cout << "IS_WORKING" << std::endl;
		break;
	case ProcessStatus::RESTARTING:
		std::cout << "RESTARTING" << std::endl;
		break;
	case ProcessStatus::STOPPED:
		std::cout << "STOPPED" << std::endl;
		break;
	}
}