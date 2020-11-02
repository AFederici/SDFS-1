#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <iostream>
#include <string>
#include <map>
#include <utility>
#include <queue>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include "Directory.h"
#include "MessageTypes.h"
#include "Messages.h"
#ifdef __linux__
#include <bits/stdc++.h>
#endif

using std::string;
using std::to_string;
using std::map;
using std::get;

#define MAXBUFLEN 1024
#define MAX_CLIENTS 10
static const char * OK = "OK\n";
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gen_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t dir_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

void closeFdAfterWrite(int fd);
void closeFd(int fd);
void *processTcpRequests(void *tcpSocket);

class TcpSocket {
public:
	char* serverPort;
    int serverSocket;

	vector<tuple<string, string, string>> request_targets;
	Messages outgoingReq;
	Messages repairReq;
	map<pthread_t, int> thread_to_ind;
	unsigned long byteSent;
    Directory * dir;
	volatile int clientsCount;
	volatile int clients[MAX_CLIENTS];
	volatile int endSession[MAX_CLIENTS+1];

	TcpSocket(char* port, Directory * direct);
	int outgoingConnection(tuple<string, string>);
	int outgoingConnection(string host, string port);
	int receivePutRequest(int fd, string target);
	int receiveGetRequest(int fd, string target);
	int sendGetRequest(int fd, string target, string local);
	int sendPutRequest(int fd, string local, string target, int node_initiated);
	int sendMessage(int fd, MessageType mt, const char * buffer);
	int sendOK(int fd);
	int receiveMessage(int fd);
	int sendFile(int fd, string filename, string target);
	int receiveFile(int fd, string local_file);
	void cleanup();
	void setupServer();
	void runServer();

};
#endif //TCPSOCKET_H
