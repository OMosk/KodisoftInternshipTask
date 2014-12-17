#include "ProcessMonitor.h"
//#include <cstdio>
#include <tchar.h>
#include <thread>

#include <iostream>
#include <sstream>
#include <string>
#include <winternl.h>
#include <cwchar>
ProcessMonitor::ProcessMonitor(wchar_t* commandLine, bool startAtCreation, Logger* logger){
	this->logger = logger;
	this->commandLine = _tcsdup(commandLine);
	processStatus = ProcessStatus::STOPPED;
	lastAction = LastAction::STOP;
	previousActionFinished = true;
	if (startAtCreation)
		start();
}
ProcessMonitor::ProcessMonitor(unsigned long pid, Logger* logger){
	this->logger = logger;
	this->commandLine = getCommandLineForProcessByPID(pid);
	previousActionFinished = true;
	if (this->commandLine == NULL){
		
		std::wstringstream message;
		message << "Process ["<<pid<<"] not found. Error = " << GetLastError();		
		logMessage(message.str().c_str());

		setLastAction(LastAction::STOP);
		setProcessStatus(ProcessStatus::STOPPED);
		this->pid = 0;
		this->pHandle = NULL;
		this->tHandle = NULL;
	}
	else{
		this->pid = pid;
		this->pHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
		this->tHandle = NULL;
		setLastAction(LastAction::START);
		setProcessStatus(ProcessStatus::IS_WORKING);
		logMessage(L"Watching process", true);
		waitingThread = std::thread(&ProcessMonitor::waitForProcess, this);
	}
}

bool ProcessMonitor::runProc(){	
	
	
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
		DETACHED_PROCESS,             
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		){		
		std::wstringstream message;
		message << "Process creation failed. Error = " << GetLastError();
		logMessage(message.str().c_str());	

		

		return false;
	}
	pid = pi.dwProcessId;
	pHandle = pi.hProcess;
	tHandle = pi.hThread;
	processInformationMutex.unlock();
	
	logMessage(L"Process created", true);

	std::lock_guard<std::mutex> callbackLock(startCallbackMutex);
	if (onProcStart)std::thread(onProcStart).detach();

	

	return true;
}
bool ProcessMonitor::stopProc(){
	
	processInformationMutex.lock();
	TerminateProcess(pHandle, 1);
	DWORD exitCode;
	if (GetExitCodeProcess(pHandle, &exitCode)){
		processInformationMutex.unlock();
		if (exitCode != 259){
			logMessage(L"Process manually stopped", true);
			if(pHandle!=NULL)CloseHandle(pHandle);
			if(tHandle!=NULL)CloseHandle(tHandle);
			
			pHandle = tHandle = NULL;

			std::lock_guard<std::mutex> callbackLock(manualStopCallbackMutex);
			if (onProcManuallyStopped)std::thread(onProcManuallyStopped).detach(); 
			
			

			return true;
		}
		else{
			logMessage(L"Process manual shutdown failed", true);
		}		
	}
	else
		processInformationMutex.unlock();		
		
	
	return false;
}
bool ProcessMonitor::restartProc(){
	
	if (stopProc()){
		previousActionFinished = true;
		actionFinishedCondition.notify_one();
		return runProc();
	}
	
	return false;
}
bool ProcessMonitor::start(){	
	std::unique_lock <std::mutex> uniqueOperationLock(operationMutex);
	while (!previousActionFinished) actionFinishedCondition.wait(uniqueOperationLock);

	previousActionFinished = false;
	std::cout << "start() started" << std::endl;

	if (getStatus() == ProcessStatus::STOPPED){		
		setLastAction(LastAction::START);
		if (runProc()){
			setProcessStatus(ProcessStatus::IS_WORKING);
			if (waitingThread.joinable()) waitingThread.join();			
			waitingThread = std::thread(&ProcessMonitor::waitForProcess, this);
			
			std::cout << "start() finished" << std::endl;
			previousActionFinished = true;
			actionFinishedCondition.notify_one();
			
			return true;
		}
		else{
			//?
		}
	}
	std::cout << "start() finished" << std::endl;
	previousActionFinished = true;
	actionFinishedCondition.notify_one();
	return false;
}
bool ProcessMonitor::stop()
{
	std::unique_lock <std::mutex> uniqueOperationLock(operationMutex);
	while (!previousActionFinished) actionFinishedCondition.wait(uniqueOperationLock);

	previousActionFinished = false;
	std::cout << "stop() started" << std::endl;
	if (getStatus() == ProcessStatus::IS_WORKING){
		setLastAction(LastAction::STOP);
		if (stopProc()){
			setProcessStatus(ProcessStatus::STOPPED);
			std::cout << "stop() finished" << std::endl;
			previousActionFinished = true;
			actionFinishedCondition.notify_one();

			return true;
		}
	}
	std::cout << "stop() finished" << std::endl;
	previousActionFinished = true;
	actionFinishedCondition.notify_one();
	return false;
}
bool ProcessMonitor::restart()
{
	std::unique_lock <std::mutex> uniqueOperationLock(operationMutex);
	while (!previousActionFinished) actionFinishedCondition.wait(uniqueOperationLock);

	previousActionFinished = false;
	std::cout << "restart() started" << std::endl;
	if (getStatus() == ProcessStatus::IS_WORKING){
		setLastAction(LastAction::RESTART);
		setProcessStatus(ProcessStatus::RESTARTING);
		if (restartProc()){
			if(waitingThread.joinable()) waitingThread.join();
			waitingThread = std::thread(&ProcessMonitor::waitForProcess, this);
			
			setProcessStatus(ProcessStatus::IS_WORKING);	
			std::cout << "restart() finished" << std::endl;
			previousActionFinished = true;
			actionFinishedCondition.notify_one();
			
			return true;
		}
		else{
			DWORD exitCode;
			if (GetExitCodeProcess(pi.hProcess, &exitCode)){
				if (exitCode == 259) setProcessStatus(ProcessStatus::IS_WORKING);
				else setProcessStatus(ProcessStatus::STOPPED);				
			}
			else{
				
			}
		}				
	}
	std::cout << "restart() finished" << std::endl;
	previousActionFinished = true;
	actionFinishedCondition.notify_one();
	return false;
}

ProcessStatus ProcessMonitor::getStatus() const{
	std::lock_guard<std::mutex> lock(processStatusMutex);
	return processStatus;
}
HANDLE ProcessMonitor::getHandle() const{
	std::lock_guard<std::mutex> lock(processInformationMutex);
	return pHandle;
}
DWORD ProcessMonitor::getPID() const{
	std::lock_guard<std::mutex> lock(processInformationMutex);
	return pid;
}
ProcessMonitor::~ProcessMonitor()
{
	stop();
	if(waitingThread.joinable())waitingThread.join();
	delete[] commandLine;
}
void ProcessMonitor::waitForProcess(){
	do{
		WaitForSingleObject(pHandle, INFINITE);

		if (getLastAction() == LastAction::START || (getLastAction() == LastAction::RESTART && getStatus() != ProcessStatus::RESTARTING)){
			logMessage(L"Process closed by itself", true);
			
			setProcessStatus(ProcessStatus::STOPPED);

			crashCallbackMutex.lock();
			if(onProcCrash)std::thread(onProcCrash).detach();
			crashCallbackMutex.unlock();
			
			if (!runProc()){
				break;
			}
			else {
				setProcessStatus(ProcessStatus::IS_WORKING);				
			}
		}
		else{
			break;
		}
	} while (true);
}
/*void ProcessMonitor::showInformation(){
	std::cout << getPID() << " " << getHandle() << std::endl;
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
}*/
void ProcessMonitor::setOnProcStart(std::function<void()> onProcStart){
	std::lock_guard<std::mutex> lock(startCallbackMutex);
	this->onProcStart = onProcStart;
}
void ProcessMonitor::setOnProcCrash(std::function<void()> onProcCrash){
	std::lock_guard<std::mutex> lock(crashCallbackMutex);
	this->onProcCrash = onProcCrash;
}
void ProcessMonitor::setOnProcManuallyStopped(std::function<void()> onProcManuallyStopped){
	std::lock_guard<std::mutex> lock(manualStopCallbackMutex);
	this->onProcManuallyStopped = onProcManuallyStopped;
}

ProcessMonitor::LastAction ProcessMonitor::getLastAction(){
	std::lock_guard<std::mutex> lock(lastActionMutex);
	return lastAction;
}
void ProcessMonitor::setLastAction(LastAction lastAction){
	std::lock_guard<std::mutex> lock(lastActionMutex);
	this->lastAction = lastAction;
}
void ProcessMonitor::setProcessStatus(ProcessStatus processStatus){
	std::lock_guard<std::mutex> lock(processStatusMutex);
	this->processStatus = processStatus;
}
TCHAR* ProcessMonitor::getCommandLine() const {
	std::lock_guard<std::mutex> lock(processInformationMutex);
	return commandLine;
}

wchar_t* ProcessMonitor::getCommandLineForProcessByPID(unsigned long pid){
	typedef NTSTATUS(NTAPI *_NtQueryInformationProcess)(
		HANDLE ProcessHandle,
		DWORD ProcessInformationClass, /* can't be bothered defining the whole enum */
		PVOID ProcessInformation,
		DWORD ProcessInformationLength,
		PDWORD ReturnLength
		);
	PROCESS_BASIC_INFORMATION pinfo;

	HMODULE hmod = LoadLibrary(L"ntdll.dll");
	_NtQueryInformationProcess NtQueryInformationProcess = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
	if (hProcess == NULL) return NULL;
	LONG status = NtQueryInformationProcess(hProcess,
		PROCESSINFOCLASS::ProcessBasicInformation,
		(PVOID)&pinfo,
		sizeof(PVOID)* 6,
		NULL);
	if (status != 0){
		CloseHandle(hProcess);
		FreeLibrary(hmod);
		return NULL;
	}
	PPEB ppeb = (PPEB)((PVOID*)&pinfo)[1];
	PPEB ppebCopy = (PPEB)malloc(sizeof(PEB));
	BOOL result = ReadProcessMemory(hProcess,
		ppeb,
		ppebCopy,
		sizeof(PEB),
		NULL);
	
	PRTL_USER_PROCESS_PARAMETERS pRtlProcParam = ppebCopy->ProcessParameters;
	PRTL_USER_PROCESS_PARAMETERS pRtlProcParamCopy =
		(PRTL_USER_PROCESS_PARAMETERS)malloc(sizeof(RTL_USER_PROCESS_PARAMETERS));
	result = ReadProcessMemory(hProcess,
		pRtlProcParam,
		pRtlProcParamCopy,
		sizeof(RTL_USER_PROCESS_PARAMETERS),
		NULL);	
	
	PWSTR wBuffer = pRtlProcParamCopy->CommandLine.Buffer;
	USHORT len = pRtlProcParamCopy->CommandLine.Length;
	PWSTR wBufferCopy = (PWSTR)new wchar_t[len/2 + 1];
	result = ReadProcessMemory(hProcess,
		wBuffer,
		wBufferCopy, // command line goes here
		len,
		NULL);	

	CloseHandle(hProcess);
	FreeLibrary(hmod);
	wBufferCopy[len / 2] = L'\0';

	return wBufferCopy;
}
void ProcessMonitor::logMessage(const wchar_t* message, bool showPID){
	if (logger){
		std::wstringstream messageStream;
		messageStream << L"ProcessMonitor: ";
		if(showPID)messageStream << "[" << getPID() << "] ";
		messageStream << message;
		logger->log(messageStream.str());
	}
}