// internshipTask.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ProcessMonitor.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
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
void actionPerformer(ProcessMonitor *monitor, Logger *logger, int id){
	srand(time(0));
	for (int i = 0; i < 250; i++)	{
		wstringstream ss;
		ss << "thread " <<id;
		switch (rand() % 3){
		case 0: 		
			ss << " start";
			logger->log(ss.str());
			monitor->start();
			break;
		case 1:	
			ss<< " stop";
			logger->log(ss.str());
			monitor->stop();
			break;
		case 2: 
			ss << " restart";
			logger->log(ss.str());
			monitor->restart();
			break;
		}
		
		Sleep(100);
	}
}
int _tmain(int argc, _TCHAR* argv[])
{
	Logger logger(L"log.log");
	
	//_TCHAR commandLine[] = L"notepad.exe";
	wstring commandLine;
	int type = 0;
	//ProcessMonitor pl(commandLine, true, &logger);
	while (type != 1 && type != 2){
		cout  << "1 - by command line" << endl << "2 - by pid" << endl;
		cin >> type;
	}
	if (type == 1){
		cout << "Enter commandLine: ";
		wcin >> commandLine;
		ProcessMonitor pl((commandLine.c_str()), true, &logger);
		pl.setOnProcStart(bind(echoStart));
		pl.setOnProcCrash(bind(echoCrash));
		pl.setOnProcManuallyStopped(bind(echoMS));
		if (pl.getCommandLine() != NULL)
			wcout << "Command line: "<< pl.getCommandLine() << endl;
		cout <<"PID: "<< pl.getPID() << endl;
		//cout <<"ProcessHandle: "<< pl.getHandle() << endl;

		cout << "Status: ";
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
		cout << "Commands: "<<endl << "  start" << endl << "  stop" << endl << "  restart" << endl << "  exit" << endl;
		wstring command;
		do
		{
			wcin >> command;
			if (command == L"stop") pl.stop();
			if (command == L"start") pl.start();
			if (command == L"restart") pl.restart();
			if (command == L"exit") break;

		} while (true);

	}
	else{
		cout << "Enter PID: ";
		unsigned long pid;
		cin >> pid;
		ProcessMonitor pl(pid, &logger);
		pl.setOnProcStart(bind(echoStart));
		pl.setOnProcCrash(bind(echoCrash));
		pl.setOnProcManuallyStopped(bind(echoMS));
		if (pl.getCommandLine() != NULL)
			wcout << "Command line: " << pl.getCommandLine() << endl;
		else{
			cout << "Cannot open process" << endl;
			getchar();
			getchar();
			return 0;
		}
		cout << "PID: " << pl.getPID() << endl;
		//cout <<"ProcessHandle: "<< pl.getHandle() << endl;

		cout << "Status: ";
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
		cout << "Commands: " << endl << "  start" << endl << "  stop" << endl << "  restart" << endl << "  exit" << endl;
		wstring command;
		do
		{
			wcin >> command;
			if (command == L"stop") pl.stop();
			if (command == L"start") pl.start();
			if (command == L"restart") pl.restart();
			if (command == L"exit") break;

		} while (true);
	}

	/*unsigned long pid;
	cin >> pid;
	ProcessMonitor pl(pid, &logger);
	pl.setOnProcStart(bind(echoStart));
	pl.setOnProcCrash(bind(echoCrash));
	pl.setOnProcManuallyStopped(bind(echoMS));
	/*
	thread t1(actionPerformer, &pl, &logger, 1);
	thread t2(actionPerformer, &pl, &logger, 2);
	thread t3(actionPerformer, &pl, &logger, 3);
	t1.join();
	t2.join();
	t3.join();
	*/
	/*
	thread tStop1(&ProcessMonitor::stop, &pl);
	thread t3(&ProcessMonitor::start, &pl);
	thread tStop2(&ProcessMonitor::restart, &pl);
	tStop1.join();
	t3.join();
	tStop2.join();
	*/
	/*
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

	} while (true);
	*/
	return 0;
}

