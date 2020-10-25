#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#include "Messages.h"
#include "Modes.h"
#include "Member.h"
#include "UdpSocket.h"
#include "TcpSocket.h"
#include "Logger.h"
#include "Utils.h"

using namespace std;

#define INTRODUCER "fa20-cs425-g02-01.cs.illinois.edu"
#define UDP_PORT "4950"
#define TCP_PORT "7777"


#define LOGGING_FILE_NAME "logs.txt"

// --- parameters (stay tuned) ---
#define T_period 500000 // in microseconds
#define T_timeout 3 // in T_period
#define T_cleanup 3 // in T_period
#define N_b 3 // how many nodes GOSSIP want to use
// ------

#define T_switch 3 // in seconds

pthread_t threads[7];

void *runUdpServer(void *udpSocket);
void *runSenderThread(void *node);
void *runTcpServer(void* tcpSocket)
void *runTcpClient(void *tcpSocket);

class Node {
public:
	// (ip_addr, port_num, timestamp at insertion) -> (hb_count, timestamp, fail_flag)
	map<tuple<string, string, string>, tuple<int, int, int>> membershipList;
	Member nodeInformation;
	UdpSocket *udpServent;
	TcpSocket *tcpServent;

	int localTimestamp;
	int heartbeatCounter;
	time_t startTimestamp;
	ModeType runningMode;
	Logger* logWriter;
	bool activeRunning;
	bool prepareToSwitch;

	Member masterInformation;
	set<tuple<string, string, string>> votes;
	string maxIP;
	// (ip,port,timestamp) -> (dir_size, vector [(file, file_heartbeat, status)])
	map<tuple<string, string, string>, tuple<int, map<string, tuple<int, int>>>> file_system;
	// file -> ( status, set<replica locations> )
	map<string, tuple<int, set<tuple<string, string, string>>> replicas_list;

	Node();
	Node(ModeType mode);
	int heartbeatToNode();
	int directoryToNode(); //file version of above
	int joinSystem(Member introdcuer);
	int listen();
	int failureDetection();
	void updateNodeHeartbeatAndTime();
	void computeAndPrintBandwidth(double diff);
	int requestSwitchingMode();
	int SwitchMyMode();
	void debugMembershipList();
	void masterDetection();
	void orderReplication(); //for master
	void updateDirIntoFileSystem();

private:
	string populateMembershipMessage();
	string populateIntroducerMembershipMessage();
	string populateFileLocationMessage(); //file version of above
	void readMessage(string message);
	void processHeartbeat(string message);
	vector<tuple<string,string, string>> getRandomNodesToGossipTo();

	void handlePut(string s1, string s2);
	void handleGet(string s1, string s2);
	void handleDelete(string s1);
	void setTcpTargets();
	void threadConsistency();
};

#endif //NODE_H
