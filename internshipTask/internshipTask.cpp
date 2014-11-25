// internshipTask.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ProcessLauncher.h"
#include <iostream>

using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
	_TCHAR commandLine[] = L"C:\\Users\\alex3_000\\Documents\\Visual Studio 2013\\Projects\\Release\\TestProcess.exe";
	ProcessLauncher pl(commandLine);
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
	return 0;
}

