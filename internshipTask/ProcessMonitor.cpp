#include "ProcessMonitor.h"
//#include <cstdio>
#include <tchar.h>
#include <thread>

//#include <iostream>
#include <sstream>
#include <string>
#include <winternl.h>
#include <cwchar>
ProcessMonitor::ProcessMonitor(const wchar_t* commandLine, bool startAtCreation, Logger* logger){
	//Initialization
	this->logger = logger;
	this->commandLine = _tcsdup(commandLine);
	processStatus = ProcessStatus::STOPPED;
	lastAction = LastAction::STOP;
	previousActionFinished = true;

	//Start process if needed
	if (startAtCreation)
		start();
}
ProcessMonitor::ProcessMonitor(unsigned long pid, Logger* logger){
	//Initialization
	this->logger = logger;
	this->commandLine = getCommandLineForProcessByPID(pid);
	previousActionFinished = true;
	//If command line wasn't extracted
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
		//if command line was extracted;
		this->pid = pid;
		this->pHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);
		this->tHandle = NULL;//just for unification with ProcessInformation structure
		setLastAction(LastAction::START);
		setProcessStatus(ProcessStatus::IS_WORKING);
		logMessage(L"Watching process", true);
		waitingThread = std::thread(&ProcessMonitor::waitForProcess, this);//wait for process closing in another thread
	}
}

bool ProcessMonitor::runProc(){	
	//run a new process
	processInformationMutex.lock();//for thread-safe
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
		pid = 0;
		pHandle = tHandle = NULL;
		processInformationMutex.unlock();
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

	std::lock_guard<std::mutex> callbackLock(startCallbackMutex);//lock a callback function
	if (onProcStart)std::thread(onProcStart).detach();	//execute a callback in another thread, caller don't want to wait

	return true;// process was successfully started
}
bool ProcessMonitor::stopProc(){
	//stop already running process
	processInformationMutex.lock();
	TerminateProcess(pHandle, 1);//Don't know what exit code will be better for this case
	DWORD exitCode;
	if (GetExitCodeProcess(pHandle, &exitCode)){
		processInformationMutex.unlock();
		if (exitCode != 259){// 259 == RUNNING
			logMessage(L"Process manually stopped", true);
			if(pHandle!=NULL)CloseHandle(pHandle);//Closing handles
			if(tHandle!=NULL)CloseHandle(tHandle);
			
//			pHandle = tHandle = NULL;

			std::lock_guard<std::mutex> callbackLock(manualStopCallbackMutex);//lock a callback function
			if (onProcManuallyStopped)std::thread(onProcManuallyStopped).detach(); //execute a callback in another thread, caller don't want to wait

			return true;// process was successfully stopped
		}
		else{
			logMessage(L"Process manual shutdown failed", true);
		}		
	}
	else
		processInformationMutex.unlock();		
		
	
	return false;//process was not stopped or can't get exit code
}
bool ProcessMonitor::restartProc(){	
	//restart already running process
	if (stopProc()){
		return runProc();//if successfully stopped then result depends on start
	}	
	return false;//process was not stopped;
}
bool ProcessMonitor::start(){	
	//interface for starting a process
	std::unique_lock <std::mutex> uniqueOperationLock(operationMutex);
	while (!previousActionFinished) actionFinishedCondition.wait(uniqueOperationLock);//wait until another action(stop, restart) finish
	previousActionFinished = false;
	

	if (getStatus() == ProcessStatus::STOPPED){
		//start a new process only if current is stopped
		setLastAction(LastAction::START);
		if (runProc()){
			//if process was started
			setProcessStatus(ProcessStatus::IS_WORKING);//update status
			if (waitingThread.joinable()) waitingThread.join();			
			waitingThread = std::thread(&ProcessMonitor::waitForProcess, this);//wait for process closing in another thread
			
			previousActionFinished = true;
			actionFinishedCondition.notify_one();//next action can start working
			
			return true;//process was successfully started
		}		
	}
	
	previousActionFinished = true;
	actionFinishedCondition.notify_one();
	return false;//Process was already running or can't start a new process
}
bool ProcessMonitor::stop(){
	//interface for stopping a process
	std::unique_lock <std::mutex> uniqueOperationLock(operationMutex);
	while (!previousActionFinished) actionFinishedCondition.wait(uniqueOperationLock);//wait until another action(start, restart) finish
	previousActionFinished = false;
	
	if (getStatus() == ProcessStatus::IS_WORKING){
		//stop process only if it's running
		setLastAction(LastAction::STOP);
		if (stopProc()){
			//if process was successfully stopped
			setProcessStatus(ProcessStatus::STOPPED);//update status
			previousActionFinished = true;
			actionFinishedCondition.notify_one();//next action can start working

			return true;//process was successfuly stopped
		}
	}

	previousActionFinished = true;
	actionFinishedCondition.notify_one();//next action can start working
	return false;//process was not running or can't stop process
}
bool ProcessMonitor::restart(){
	std::unique_lock <std::mutex> uniqueOperationLock(operationMutex);
	while (!previousActionFinished) actionFinishedCondition.wait(uniqueOperationLock);//wait until another action(start, stort) finish
	previousActionFinished = false;

	if (getStatus() == ProcessStatus::IS_WORKING){
		//restart process only if it's running
		setLastAction(LastAction::RESTART);
		setProcessStatus(ProcessStatus::RESTARTING);//update status
		if (restartProc()){
			//if process was successfully restarted
			setProcessStatus(ProcessStatus::IS_WORKING);//update status
			if(waitingThread.joinable()) waitingThread.join();//wait for previous call of waitFunction to finish
			waitingThread = std::thread(&ProcessMonitor::waitForProcess, this);//wait for process closing in another thread			
				
			previousActionFinished = true;
			actionFinishedCondition.notify_one();//next action can start working
			
			return true;
		}
		else{
			//if something gone wrong
			DWORD exitCode;
			if (GetExitCodeProcess(pi.hProcess, &exitCode)){
				//checking exit code
				if (exitCode == 259) setProcessStatus(ProcessStatus::IS_WORKING);//259==RUNNING
				else setProcessStatus(ProcessStatus::STOPPED);//any another exit code indicate crash or closing
			}
			else{
				//can't get exit code
				setProcessStatus(ProcessStatus::STOPPED);
			}
		}				
	}

	previousActionFinished = true;
	actionFinishedCondition.notify_one();//next action can start working
	return false;//Process wasn't restarted or process wasn't restarted correct
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
ProcessMonitor::~ProcessMonitor(){
	//destructor
	stop();//stop current process
	if(waitingThread.joinable())waitingThread.join();//wait for thread 
	delete[] commandLine;//free memory
}
void ProcessMonitor::waitForProcess(){
	//this function waits for process closing and restart process if needed
	do{
		WaitForSingleObject(pHandle, INFINITE);//wait for process closing

		if (getLastAction() == LastAction::START || (getLastAction() == LastAction::RESTART && getStatus() != ProcessStatus::RESTARTING)){
		//if (getLastAction() != LastAction::STOP){
			//must restart process only if it was closed by itself
			logMessage(L"Process closed by itself", true);
			
			setProcessStatus(ProcessStatus::STOPPED);//update status

			crashCallbackMutex.lock();
			if(onProcCrash)std::thread(onProcCrash).detach();//execute callback
			crashCallbackMutex.unlock();
			
			if (!runProc()){
				break;//if we can't start process back we leave
			}
			else {
				setProcessStatus(ProcessStatus::IS_WORKING);//if we can, update status
			}
		}
		else{
			break;//we closed that process 
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
	//method returns commandLine by process id
	//DOES NOT WORK if called from 32-bit for 64-bit process, other cases work fine
	typedef NTSTATUS(NTAPI *_NtQueryInformationProcess)(
		HANDLE ProcessHandle,
		DWORD ProcessInformationClass, 
		PVOID ProcessInformation,
		DWORD ProcessInformationLength,
		PDWORD ReturnLength
		);//define a pointer to function
	PROCESS_BASIC_INFORMATION pinfo;

	HMODULE hmod = LoadLibrary(L"ntdll.dll");//load debug library
	//get address of needed function
	_NtQueryInformationProcess NtQueryInformationProcess = (_NtQueryInformationProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), 
																								"NtQueryInformationProcess");
	
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, pid);//open process
	if (hProcess == NULL) return NULL;//if we can't open process, we can't get command line
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
	PWSTR wBufferCopy = (PWSTR)new wchar_t[len/2 + 1];// additional 2 bytes for \0
	result = ReadProcessMemory(hProcess,
		wBuffer,
		wBufferCopy, // command line goes here
		len,
		NULL);	

	CloseHandle(hProcess);//closing handle of process
	FreeLibrary(hmod);//closing handle of library
	wBufferCopy[len / 2] = L'\0';//adding end of string symbol

	return wBufferCopy;
}
void ProcessMonitor::logMessage(const wchar_t* message, bool showPID){
	//method sends message to the logger
	if (logger){
		std::wstringstream messageStream;
		messageStream << L"ProcessMonitor: ";
		if(showPID)messageStream << "[" << getPID() << "] ";
		messageStream << message;
		logger->log(messageStream.str());
	}
}