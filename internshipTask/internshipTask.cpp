// internshipTask.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ProcessLauncher.h"
#include "Logger.h"
#include <iostream>
#include <string>
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
int _tmain(int argc, _TCHAR* argv[])
{
	//_TCHAR commandLine[] = L"C:\\Users\\alex3_000\\Documents\\Visual Studio 2013\\Projects\\Release\\TestProcess.exe";
	//_TCHAR commandLine[] = L"C:\\Users\\alex3_000\\Documents\\Visual Studio 2013\\Projects\\Release\\TestProcess.exe";
	Logger logger(L"log.log");
	//logger.log(L"First");
	//logger.log(L"SECOND");
	
	_TCHAR commandLine[] = L"notepad.exe";

	ProcessLauncher pl(commandLine, true, &logger);
	
	pl.setOnProcStart(bind(echoStart));
	pl.setOnProcCrash(bind(echoCrash));
	pl.setOnProcManuallyStopped(bind(echoMS));

	wcout << commandLine << endl << pl.getId() << endl << pl.getHandle() << endl;
	
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

