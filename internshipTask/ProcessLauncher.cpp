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
	std::lock_guard<std::mutex> guard(processInformationMutex);
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));	
	if (!CreateProcess(NULL,   // No module name (use command line)
		commandLine,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		//CREATE_NEW_CONSOLE,//
		DETACHED_PROCESS,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return false;
	}
	std::lock_guard<std::mutex> callbackLock(startCallbackMutex);
	if (onProcStart)onProcStart();
	return true;
}
bool ProcessLauncher::stopProc(){
	std::lock_guard<std::mutex> lock(processInformationMutex);
	TerminateProcess(pi.hProcess, 1);// CHANGE EXIT CODE
	DWORD exitCode;
	if (GetExitCodeProcess(pi.hProcess, &exitCode)){
		if (exitCode != 259){
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			std::lock_guard<std::mutex> callbackLock(manualStopCallbackMutex);
			if (onProcManuallyStopped) onProcManuallyStopped();
			return true;
		}
	}
	else{
		printf("GetExitCodeProcess failed (%d).\n", GetLastError());
		//return false;
	}
	return false;
}
bool ProcessLauncher::restartProc(){
	if (stopProc())
		return runProc();

	return false;
}
bool ProcessLauncher::start()
{	
	//std::lock_guard<std::mutex> lock(processStatusMutex);
	if (getStatus() == ProcessStatus::STOPPED){
		//lastAction = LastAction::START;
		setLastAction(LastAction::START);
		if (runProc()){
			setProcessStatus(ProcessStatus::IS_WORKING); //processStatus = ProcessStatus::IS_WORKING;
			showInformation();
			std::thread(&ProcessLauncher::waitForProcess, this).detach();
			return true;
		}
		
	}
	return false;
}
bool ProcessLauncher::stop()
{
	//si.
	if (getStatus() == ProcessStatus::IS_WORKING){
		//lastAction = LastAction::STOP;
		setLastAction(LastAction::STOP);
		if (stopProc()){
			//processStatus = ProcessStatus::STOPPED;			
			setProcessStatus(ProcessStatus::STOPPED);
			return true;
		}
	}
	return false;
}
bool ProcessLauncher::restart()
{
	if (getStatus() == ProcessStatus::IS_WORKING){
		//lastAction = LastAction::RESTART;
		setLastAction(LastAction::RESTART);
		//processStatus = ProcessStatus::RESTARTING;
		setProcessStatus(ProcessStatus::RESTARTING);
		if (restartProc()){
			//processStatus = ProcessStatus::IS_WORKING;
			setProcessStatus(ProcessStatus::IS_WORKING);
			std::thread(&ProcessLauncher::waitForProcess, this).detach();
			return true;
		}
		else{
			//processStatus = ProcessStatus::STOPPED;
			DWORD exitCode;
			if (GetExitCodeProcess(pi.hProcess, &exitCode)){
				if (exitCode == 259) setProcessStatus(ProcessStatus::IS_WORKING);
				else setProcessStatus(ProcessStatus::STOPPED);
				
			}
			
		}
		
		//waitingThread.detach();
	}
	return false;
}

ProcessStatus ProcessLauncher::getStatus() const{
	std::lock_guard<std::mutex> lock(processStatusMutex);
	return processStatus;
}
HANDLE ProcessLauncher::getHandle() const{
	std::lock_guard<std::mutex> lock(processInformationMutex);
	return pi.hProcess;
}
DWORD ProcessLauncher::getId() const{
	std::lock_guard<std::mutex> lock(processInformationMutex);
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
		if (getLastAction() == LastAction::START){
			//printf("process closed by itself\n");
			//processStatus = ProcessStatus::STOPPED;
			setProcessStatus(ProcessStatus::STOPPED);

			crashCallbackMutex.lock();
			if(onProcCrash)onProcCrash();//Callback
			crashCallbackMutex.unlock();
			
			if (!runProc()) break;
			else setProcessStatus(ProcessStatus::IS_WORKING);//processStatus = ProcessStatus::IS_WORKING;
			//printf("process closed by itself\n");
		}
		else{
			break;
		}
	} while (true);
	//processStatus = ProcessStatus::STOPPED;
	setProcessStatus(ProcessStatus::STOPPED);

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
	std::lock_guard<std::mutex> lock(startCallbackMutex);
	this->onProcStart = onProcStart;
}
void ProcessLauncher::setOnProcCrash(std::function<void()> onProcCrash){
	std::lock_guard<std::mutex> lock(crashCallbackMutex);
	this->onProcCrash = onProcCrash;
}
void ProcessLauncher::setOnProcManuallyStopped(std::function<void()> onProcManuallyStopped){
	std::lock_guard<std::mutex> lock(manualStopCallbackMutex);
	this->onProcManuallyStopped = onProcManuallyStopped;
}

ProcessLauncher::LastAction ProcessLauncher::getLastAction(){
	std::lock_guard<std::mutex> lock(lastActionMutex);
	return lastAction;
}
void ProcessLauncher::setLastAction(LastAction lastAction){
	std::lock_guard<std::mutex> lock(lastActionMutex);
	this->lastAction = lastAction;
}
void ProcessLauncher::setProcessStatus(ProcessStatus processStatus){
	std::lock_guard<std::mutex> lock(processStatusMutex);
	this->processStatus = processStatus;
}