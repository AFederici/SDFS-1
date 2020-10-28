#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <utility>
#include "MessageTypes.h"

#define LOGFILE "logs.txt"

using std::string;

class Logger{
public:
	string filename;
	Logger();
	Logger(string fileName);
	int printTheLog(LogType type, string s);
private:
	int writeToFile(string messages);
};

#endif //LOGGER_H
