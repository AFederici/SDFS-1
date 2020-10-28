#ifndef MEMBER_H
#define MEMBER_H

#include <iostream>
#include <string>
#include <utility>
#include "Utils.h"
#ifdef __linux__ 
#include <bits/stdc++.h>
#endif

using std::string;
using std::to_string;
using std::tuple;
using std::get;

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
