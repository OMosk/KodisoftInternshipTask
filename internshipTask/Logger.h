#pragma once
#include <fstream>
#include <string>
#include <mutex>
class Logger
{
	std::mutex m;
	std::wofstream out;
	std::wstring fileName;
public:
	Logger(std::wstring fileName);
	void log(std::wstring message);
	~Logger();
};

