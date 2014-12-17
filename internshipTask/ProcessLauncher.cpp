#include "ProcessLauncher.h"
#include <cstdio>
#include <tchar.h>
#include <thread>

#include <iostream>
#include <sstream>
#include <string>
#include <winternl.h>
#include <cwchar>
ProcessLauncher::ProcessLauncher(TCHAR* commandLine, bool startAtCreation, Logger* logger)
{
	this->logger = logger;
	this->commandLine = _tcsdup(commandLine);
	processStatus = ProcessStatus::STOPPED;
	lastAction = LastAction::STOP;
	if (startAtCreation){
		start();
		
	}
}
ProcessLauncher::ProcessLauncher(unsigned long pid, Logger* logger){
	//HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
	this->logger = logger;
	this->commandLine = getCommandLineForProcessByPID(pid);
	if (this->commandLine == NULL){
		
		std::wstringstream message;
		message << "Process not found. Error = " << GetLastError();		
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
		waitingThread = std::thread(&ProcessLauncher::waitForProcess, this);
	}
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
	if (onProcStart)std::thread(onProcStart).detach();//onProcStart();
	return true;
}
bool ProcessLauncher::stopProc(){
	//std::lock_guard<std::mutex> lock(processInformationMutex);
	processInformationMutex.lock();
	//TerminateProcess(pi.hProcess, 1);// CHANGE EXIT CODE
	TerminateProcess(pHandle, 1);
	DWORD exitCode;
	if (GetExitCodeProcess(pHandle, &exitCode)){
		processInformationMutex.unlock();
		if (exitCode != 259){
			logMessage(L"Process manually stopped", true);
			if(pHandle!=NULL)CloseHandle(pHandle);
			if(tHandle!=NULL)CloseHandle(tHandle);
			//processInformationMutex.unlock();
			std::lock_guard<std::mutex> callbackLock(manualStopCallbackMutex);
			if (onProcManuallyStopped)std::thread(onProcManuallyStopped).detach(); //onProcManuallyStopped();
			
			return true;
		}
		else{
			logMessage(L"Process manual shutdown failed", true);
		}
		
	}
	else{
		processInformationMutex.unlock();
		//printf("GetExitCodeProcess failed (%d).\n", GetLastError());
		
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

			
			
			return true;
		}
		else{

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
			//setProcessStatus(ProcessStatus::IS_WORKING);
			
			if(waitingThread.joinable()) waitingThread.join();
			waitingThread = std::thread(&ProcessLauncher::waitForProcess, this);
			setProcessStatus(ProcessStatus::IS_WORKING);
			
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
	return false;
}

ProcessStatus ProcessLauncher::getStatus() const{
	std::lock_guard<std::mutex> lock(processStatusMutex);
	return processStatus;
}
HANDLE ProcessLauncher::getHandle() const{
	std::lock_guard<std::mutex> lock(processInformationMutex);
	return pHandle;
}
DWORD ProcessLauncher::getPID() const{
	std::lock_guard<std::mutex> lock(processInformationMutex);
	return pid;
}
ProcessLauncher::~ProcessLauncher()
{
	stop();
	if(waitingThread.joinable())waitingThread.join();
}
void ProcessLauncher::waitForProcess(){
	do{
		//WaitForSingleObject(pi.hProcess, INFINITE);
		WaitForSingleObject(pHandle, INFINITE);
		if (getLastAction() == LastAction::START || (getLastAction() == LastAction::RESTART && getStatus() != ProcessStatus::RESTARTING)){
			logMessage(L"Process closed by itself", true);
			
			setProcessStatus(ProcessStatus::STOPPED);

			crashCallbackMutex.lock();
			if(onProcCrash)std::thread(onProcCrash).detach();//Callback
			crashCallbackMutex.unlock();
			
			if (!runProc()){
				//setProcessStatus(ProcessStatus::STOPPED);
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
void ProcessLauncher::showInformation(){
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
TCHAR* ProcessLauncher::getCommandLine() const {
	std::lock_guard<std::mutex> lock(processInformationMutex);
	return commandLine;
}

TCHAR* ProcessLauncher::getCommandLineForProcessByPID(unsigned long pid){
	typedef NTSTATUS(NTAPI *_NtQueryInformationProcess)(
		HANDLE ProcessHandle,
		DWORD ProcessInformationClass, /* can't be bothered defining the whole enum */
		PVOID ProcessInformation,
		DWORD ProcessInformationLength,
		PDWORD ReturnLength
		);
	PROCESS_BASIC_INFORMATION pinfo;
//	cin >> pid;
	HMODULE hmod = LoadLibrary(L"ntdll.dll");
	//NtQueryInformationProcess();
	//_NtQueryInformationProcess queryInfoProc = (_NtQueryInformationProcess)GetProcAddress(hmod, "NtQueryInformationProcess");
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
	//printf("NTSTATUS = %X\n", status);

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
	
//	printf("ImagePath=%s\n", pRtlProcParam->ImagePathName.Buffer);
//	printf("CommandLine=%s\n", pRtlProcParam->CommandLine.Buffer);
	
	PWSTR wBuffer = pRtlProcParamCopy->CommandLine.Buffer;
	USHORT len = pRtlProcParamCopy->CommandLine.Length;
	//PWSTR wBufferCopy = (PWSTR)malloc(len/2);
	PWSTR wBufferCopy = (PWSTR)new wchar_t[len/2 + 1];
	result = ReadProcessMemory(hProcess,
		wBuffer,
		wBufferCopy, // command line goes here
		len,
		NULL);
	
	//std::wcout << wBufferCopy << endl;
	CloseHandle(hProcess);
	FreeLibrary(hmod);
	wBufferCopy[len / 2] = L'\0';


	/*wchar_t* resultString = new wchar_t[len/2 + 1];
	wcsncpy(resultString, (wchar_t*)wBufferCopy, wcslen(wBufferCopy));
	//resultString[wcslen(wBufferCopy)] = L'\0';
	resultString[wcslen(wBufferCopy)] = 0;
	//wcscpy(resultString, wBufferCopy);
	delete[] wBufferCopy;
	return (TCHAR*)resultString;*/
	return (TCHAR*)wBufferCopy;
}
void ProcessLauncher::logMessage(const wchar_t* message, bool showPID){
	if (logger){
		std::wstringstream messageStream;
		messageStream << L"ProcessLauncher: ";
		if(showPID)messageStream << "[" << getPID() << "] ";
		messageStream << message;
		logger->log(messageStream.str());
	}
}