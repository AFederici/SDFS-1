#ifndef MEMBER_H
#define MEMBER_H

#include <iostream>
#include <string>

#include "Utils.h"

using std::string;
using std::to_string;
using std::tuple;

class Member {
public:
	string ip;
	string udpPort;
	string tcpPort;
	int timestamp;
	int heartbeatCounter;
	int failed_flag;
	tuple<string, string, string> identity();
	Member();
	Member(string nodeIp, string udpPort, int nodeTimestamp, int heartbeatCounter);
	Member(string nodeIp, string udpPort);
	Member(string nodeIp, string udpPort, int nodeTimestamp);
	Member(string nodeIp, string udpPort, string tcpPort, int nodeTimestamp, int heartbeatCounter);
	string toString();
	string tcpId();
};


#endif //MEMBER_H
