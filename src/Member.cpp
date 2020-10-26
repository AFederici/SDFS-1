#include "../inc/Member.h"

Member::Member(string nodeIp, string portUdp, string portTcp, int nodeTimestamp, int nodeHeartbeatCounter)
{
	ip = nodeIp;
	udpPort = portUdp;
	tcpPort = portTcp;
	failed_flag = 0;
	timestamp = nodeTimestamp;
	heartbeatCounter = nodeHeartbeatCounter;
}

Member::Member(string nodeIp, string nodePort, int nodeTimestamp, int nodeHeartbeatCounter)
{
	ip = nodeIp;
	udpPort = nodePort;
	failed_flag = 0;
	timestamp = nodeTimestamp;
	heartbeatCounter = nodeHeartbeatCounter;
}

Member::Member(string nodeIp, string nodePort)
{
	ip = nodeIp;
	udpPort = nodePort;
	timestamp = 0;
	failed_flag = 0;
	heartbeatCounter = 0;
}

Member::Member(string nodeIp, string nodePort, int nodeTimestamp)
{
	ip = nodeIp;
	udpPort = nodePort;
	timestamp = nodeTimestamp;
	failed_flag = 0;
	heartbeatCounter = 0;
}

string Member::toString()
{
	string message = ip + "::" + udpPort + "::" + to_string(timestamp);
	return message;
}

string Member::tcpId(){
	string id = ip + "::" + tcpPort + "::" + to_string(timestamp);
	return id;
}

tuple<string, string, string> Member::identity(){
	tuple<string, string, string> key(ip, udpPort, to_string(timestamp)); // fixed typo
	return key;
};

Member::Member(){
	ip = "";
	udpPort = "";
	tcpPort = "";
	timestamp = 0;
	failed_flag = 0;
	heartbeatCounter = 0;
}
