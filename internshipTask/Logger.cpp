#include "Logger.h"
#include <ctime>


Logger::Logger(std::wstring fileName)
{
	out = std::wofstream(fileName, std::ofstream::out | std::wofstream::app);
}
void Logger::log(std::wstring message){

	std::time_t t = std::time(NULL);
	char mbstr[100];
	if (std::strftime(mbstr, sizeof(mbstr), "%c", std::localtime(&t))) {
		//std::cout << mbstr << '\n';		
		m.lock();
		out << mbstr << ": " << message << std::endl;
		m.unlock();
	}
	else{
		m.lock();
		out << message << std::endl;
		m.unlock();
	}
	
}

Logger::~Logger()
{
	out.close();
}
