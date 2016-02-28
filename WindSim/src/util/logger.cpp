#include "logger.h"
#include <exception>
#include <iostream>

void Logger::logit(const QString& msg)
{
	try
	{
		emit log(msg);
	}
	catch(const std::exception& ex)
	{
		std::cout << "WARNING: Caught exception while writing to message log! Messge: '" << ex.what() << "'\n";
	}
}