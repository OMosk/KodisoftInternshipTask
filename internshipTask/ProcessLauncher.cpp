#include "ProcessLauncher.h"
#include <cstdio>
#include <tchar.h>
#include <thread>
#include <iostream>
ProcessLauncher::ProcessLauncher(TCHAR* commandLine)
{
	this->commandLine = _tcsdup(commandLine);
	processStatus = ProcessStatus::STOPPED;
	start();	
}

bool ProcessLauncher::runProc(){
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	
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
		return false;
	}
	if (onProcStart)onProcStart();
	return true;
}
bool ProcessLauncher::stopProc(){
	TerminateProcess(pi.hProcess, 1);// CHANGE EXIT CODE
	DWORD exitCode;
	if (GetExitCodeProcess(pi.hProcess, &exitCode))
	{
		if (exitCode != 259){
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			if (onProcManuallyStopped) onProcManuallyStopped();
			return true;
		}
	}
	else
	{
		printf("GetExitCodeProcess failed (%d).\n", GetLastError());
		return false;
	}
	return false;
}
bool ProcessLauncher::restartProc(){
	if (stopProc())
		return runProc();

	return false;
}
void ProcessLauncher::start()
{
	if (processStatus == ProcessStatus::STOPPED){
		lastAction = LastAction::START;
		if(runProc()) processStatus = ProcessStatus::IS_WORKING;
		showInformation();
		waitingThread = std::thread(&ProcessLauncher::waitForProcess, this);
		waitingThread.detach();
	}
}
void ProcessLauncher::stop()
{
	//si.
	if (processStatus == ProcessStatus::IS_WORKING){	
		lastAction = LastAction::STOP;
		if (stopProc()){
			processStatus = ProcessStatus::STOPPED;			
		}
	}
}
void ProcessLauncher::restart()
{
	if (processStatus == ProcessStatus::IS_WORKING){
		lastAction = LastAction::RESTART;
		processStatus = ProcessStatus::RESTARTING;
		if (restartProc()){
			processStatus = ProcessStatus::IS_WORKING;
		}
		else{
			processStatus = ProcessStatus::STOPPED;
		}
		waitingThread = std::thread(&ProcessLauncher::waitForProcess, this);
		waitingThread.detach();
	}
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
	/*if (processStatus == ProcessStatus::IS_WORKING){
		stopProc();
	}*/
	stop();
}
void ProcessLauncher::waitForProcess(){
	do{
		WaitForSingleObject(pi.hProcess, INFINITE);
		if (lastAction == LastAction::START){
			printf("process closed by itself\n");
			processStatus = ProcessStatus::STOPPED;
			if(onProcCrash)onProcCrash();//Callback
			if (!runProc()) break;
			else processStatus = ProcessStatus::IS_WORKING;
			//printf("process closed by itself\n");
		}
		else{
			break;
		}
	} while (true);
	processStatus = ProcessStatus::STOPPED;
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
void ProcessLauncher::setOnProcStart(std::function<void()> onProcStart){
	this->onProcStart = onProcStart;
}
void ProcessLauncher::setOnProcCrash(std::function<void()> onProcCrash){
	this->onProcCrash = onProcCrash;
}
void ProcessLauncher::setOnProcManuallyStopped(std::function<void()> onProcManuallyStopped){
	this->onProcManuallyStopped = onProcManuallyStopped;
}