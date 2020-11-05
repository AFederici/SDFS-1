#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "Utils.h"
#include "Messages.h"
#include "Modes.h"
#include "Member.h"
#include "UdpSocket.h"
#include "TcpSocket.h"
#include "Logger.h"
#ifdef __linux__
#include <bits/stdc++.h>
#endif

using namespace std;

#define INTRODUCER "fa20-cs425-g02-01.cs.illinois.edu"
#define UDP_PORT "4950"
#define TCP_PORT "7777"
#define REP 4 // file replicas

#define LOGGING_FILE_NAME "logs.txt"

// --- parameters (stay tuned) ---
#define T_period 500000 // in microseconds
#define T_timeout 3 // in T_period
#define T_cleanup 3 // in T_period
#define N_b 3 // how many nodes GOSSIP want to use
// ------

#define T_switch 3 // in seconds

static pthread_mutex_t repair_mutex = PTHREAD_MUTEX_INITIALIZER;

void *runUdpServer(void *udpSocket);
void *runSenderThread(void *node);
void *runTcpServer(void* tcpSocket);
void *runTcpClient(void *tcpSocket);
void *runRepairThread(void *tcpSocket);

class Node {
public:
	pthread_t thread_arr[8];
	// (ip_addr, port_num, timestamp at insertion) -> (hb_count, timestamp, fail_flag)
	map<tuple<string, string, string>, tuple<int, int, int>> membershipList;
	Member nodeInformation;
	UdpSocket *udpServent;
	TcpSocket *tcpServent;
	string introducerIP;
	char * tcpPort;
	char * udpPort;
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
	map<string, tuple<int, set<tuple<string, string, string>>>> replicas_list;

	Node();
	Node(char* p1, char* p2, ModeType mode);
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
	// switched to public functions
	void handlePut(string s1, string s2);
	void handleGet(string s1, string s2);
	void readSdfsMessage(string m);
	void handleDelete(string s1);
	vector<tuple<string, string, string>> getTcpTargets(string s1);

private:
	string populateMembershipMessage();
	string populateIntroducerMembershipMessage();
	string populateFileLocationMessage(); //file version of above
	void readMessage(string message);
	void processHeartbeat(string message);
	vector<tuple<string,string, string>> getRandomNodesToGossipTo();

	void threadConsistency(string s1);
	void mergeFileSystem(string m);
};

#endif //NODE_H
