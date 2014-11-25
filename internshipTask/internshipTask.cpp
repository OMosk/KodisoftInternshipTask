// internshipTask.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ProcessLauncher.h"
int _tmain(int argc, _TCHAR* argv[])
{

	ProcessLauncher pl(L"C:\\Users\\alex3_000\\Documents\\Visual Studio 2013\\Projects\\Release\\TestProcess.exe");
	_getch();
	pl.stop();
	return 0;
}

