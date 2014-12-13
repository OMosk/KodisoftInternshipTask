#include "ProcessLauncher.h"
#include <cstdio>
#include <tchar.h>
#include <thread>

#include <iostream>
#include <sstream>
ProcessLauncher::ProcessLauncher(TCHAR* commandLine, bool startAtCreation, Logger* logger)
{
	this->logger = logger;
	this->commandLine = _tcsdup(commandLine);
	processStatus = ProcessStatus::STOPPED;
	if(startAtCreation)start();	
}

bool ProcessLauncher::runProc(){
	//std::lock_guard<std::mutex> guard(processInformationMutex);
	processInformationMutex.lock();
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
		if (logger){
			std::wstringstream message;
			message << "Process creation failed. Error = " << GetLastError();
			logger->log(message.str());
		}
		return false;
	}
	processInformationMutex.unlock();
	
	std::lock_guard<std::mutex> callbackLock(startCallbackMutex);
	if (onProcStart)std::thread(onProcStart).detach();//onProcStart();
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
			if (onProcManuallyStopped)std::thread(onProcManuallyStopped).detach(); //onProcManuallyStopped();
			
			return true;
		}
	}
	else{
		//printf("GetExitCodeProcess failed (%d).\n", GetLastError());
		if (logger){
			std::wstringstream message;
			message << "GetExitCodeProcess failed. PID = " << getId() << ". Error = "<<GetLastError();
			logger->log(message.str());
		}
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
	
	if (getStatus() == ProcessStatus::STOPPED){		
		setLastAction(LastAction::START);
		if (runProc()){
			setProcessStatus(ProcessStatus::IS_WORKING);

			if (waitingThread.joinable()) waitingThread.join();
			
			waitingThread = std::thread(&ProcessLauncher::waitForProcess, this);

			if (logger){
				std::wstringstream message;
				message << "Process created. PID = " << getId();
				logger->log(message.str());
			}
			return true;
		}		
	}
	return false;
}
bool ProcessLauncher::stop()
{
	if (getStatus() == ProcessStatus::IS_WORKING){
		setLastAction(LastAction::STOP);
		if (stopProc()){
			setProcessStatus(ProcessStatus::STOPPED);
			if (logger){
				std::wstringstream message;
				message << "Process stopped. PID = " << getId();
				logger->log(message.str());
			}
			return true;
		}
	}
	return false;
}
bool ProcessLauncher::restart()
{
	if (getStatus() == ProcessStatus::IS_WORKING){
		setLastAction(LastAction::RESTART);
		setProcessStatus(ProcessStatus::RESTARTING);
		if (restartProc()){
			setProcessStatus(ProcessStatus::IS_WORKING);
			
			if(waitingThread.joinable()) waitingThread.join();
			waitingThread = std::thread(&ProcessLauncher::waitForProcess, this);
			
			if (logger){
				std::wstringstream message;
				message << "Process restarted. PID = " << getId();
				logger->log(message.str());
			}
			return true;
		}
		else{
			DWORD exitCode;
			if (GetExitCodeProcess(pi.hProcess, &exitCode)){
				if (exitCode == 259) setProcessStatus(ProcessStatus::IS_WORKING);
				else setProcessStatus(ProcessStatus::STOPPED);				
			}
			else{
				if (logger){
					std::wstringstream message;
					message << "GetExitCodeProcess failed. PID = " << getId() << ". Error = " << GetLastError();
					logger->log(message.str());
				}
			}
		}				
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
	stop();
	if(waitingThread.joinable())waitingThread.join();
}
void ProcessLauncher::waitForProcess(){
	do{
		WaitForSingleObject(pi.hProcess, INFINITE);
		if (getLastAction() == LastAction::START || (getLastAction() == LastAction::RESTART && getStatus() != ProcessStatus::RESTARTING)){
			if (logger){
				std::wstringstream message;
				message << "Process closed by itselft. PID = " << getId();
				logger->log(message.str());
			}
			
			setProcessStatus(ProcessStatus::STOPPED);

			crashCallbackMutex.lock();
			if(onProcCrash)std::thread(onProcCrash).detach();//Callback
			crashCallbackMutex.unlock();
			
			if (!runProc()){
				setProcessStatus(ProcessStatus::STOPPED);
				break;
			}
			else {
				setProcessStatus(ProcessStatus::IS_WORKING);
				if (logger){
					std::wstringstream message;
					message << "Process created. PID = " << getId();
					logger->log(message.str());
				}
			}
		}
		else{
			break;
		}
	} while (true);
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