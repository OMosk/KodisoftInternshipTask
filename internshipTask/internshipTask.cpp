// internshipTask.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ProcessMonitor.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include <cstdlib>
using namespace std;
void echoStart(){
	cout << "Started.Callback" << endl;
}
void echoCrash(){
	cout << "Crash.Callback" << endl; 
}
void echoMS(){
	cout << "ManuallyStopped.Callback" << endl;
}
void actionPerformer(ProcessMonitor *monitor){
	srand(time(0));
	for (int i = 0; i < 50; i++)	{
		switch (rand() % 3){
		case 0: monitor->start();
			break;
		case 1:	monitor->stop();
			break;
		case 2: monitor->restart();
			break;
		}
		//Sleep(100);
	}
}
int _tmain(int argc, _TCHAR* argv[])
{
	//_TCHAR commandLine[] = L"C:\\Users\\alex3_000\\Documents\\Visual Studio 2013\\Projects\\Release\\TestProcess.exe";
	//_TCHAR commandLine[] = L"C:\\Users\\alex3_000\\Documents\\Visual Studio 2013\\Projects\\Release\\TestProcess.exe";
	Logger logger(L"log.log");
	//logger.log(L"First");
	//logger.log(L"SECOND");
	
	_TCHAR commandLine[] = L"notepad.exe";

	ProcessMonitor pl(commandLine, true, &logger);
	unsigned long pid;
	//cin >> pid;
	//ProcessMonitor pl(pid, &logger);
	pl.setOnProcStart(bind(echoStart));
	pl.setOnProcCrash(bind(echoCrash));
	pl.setOnProcManuallyStopped(bind(echoMS));
	
	thread t1(actionPerformer, &pl);
	thread t2(actionPerformer, &pl);
	thread t3(actionPerformer, &pl);
	t1.join();
	t2.join();
	t3.join();
	
	/*
	thread tStop1(&ProcessMonitor::stop, &pl);
	thread t3(&ProcessMonitor::start, &pl);
	thread tStop2(&ProcessMonitor::restart, &pl);
	tStop1.join();
	t3.join();
	tStop2.join();
	*/
	
	pl.start();

	if (pl.getCommandLine()!=NULL)
		wcout << pl.getCommandLine() << endl;
	cout << pl.getPID() << endl;
	cout << pl.getHandle() << endl;
	
	switch (pl.getStatus()){
	case	ProcessStatus::IS_WORKING:
		cout << "IS_WORKING" << endl;
		break;
	case ProcessStatus::RESTARTING:
		cout << "RESTARTING" << endl;
		break;
	case ProcessStatus::STOPPED:
		cout << "STOPPED" << endl;
		break;
	}
	//_getch();
	wstring command;
	do
	{
		wcin >> command;
		if (command == L"stop") pl.stop();
		if (command == L"start") pl.start();
		if (command == L"restart") pl.restart();
		if (command == L"exit") break;
		
		/*switch (pl.getStatus()){
		case	ProcessStatus::IS_WORKING:
			cout << "IS_WORKING" << endl;
			break;
		case ProcessStatus::RESTARTING:
			cout << "RESTARTING" << endl;
			break;
		case ProcessStatus::STOPPED:
			cout << "STOPPED" << endl;
			break;
		}
		*/

	} while (true);
	/*
	pl.restart();
	switch (pl.getStatus()){
	case	ProcessStatus::IS_WORKING:
		cout << "IS_WORKING" << endl;
		break;
	case ProcessStatus::RESTARTING:
		cout << "RESTARTING" << endl;
		break;
	case ProcessStatus::STOPPED:
		cout << "STOPPED" << endl;
		break;
	}
	_getch();
	pl.stop();
	switch (pl.getStatus()){
	case	ProcessStatus::IS_WORKING:
		cout << "IS_WORKING" << endl;
		break;
	case ProcessStatus::RESTARTING:
		cout << "RESTARTING" << endl;
		break;
	case ProcessStatus::STOPPED:
		cout << "STOPPED" << endl;
		break;
	}
	_getch();
	*/
	return 0;
}

