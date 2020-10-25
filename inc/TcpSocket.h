#ifndef TCPSOCKET_H
#define TCPSOCKET_H
#include <iostream>
#include <string>
#include <map>
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
#include <queue>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include "Directory.h"
#include "MessageTypes.h"
#include "Messages.h"

using std::string;
using std::to_string;
using std::map;

#define MAXBUFLEN 1024
#define MAX_CLIENTS 10
static const char * OK = "OK\n";
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t dir_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

void closeFdAfterWrite(int fd);
void closeFd(int fd);
void *processTcpRequests(void *tcpSocket);

class TcpSocket {
public:
	string serverPort;
    int serverSocket;

	vector<tuple<string, string>> request_targets;
	Messages outgoingReq;
	map<int, int> thread_to_ind;
	unsigned long byteSent;
    Directory * dir;
	volatile int clientsCount;
	volatile int clients[MAX_CLIENTS];
	volatile int endSession[MAX_CLIENTS+1];

	TcpSocket(string port, Directory * direct);
	int outgoingConnection(tuple<string, string>);
	int outgoingConnection(string host, string port);
	int receivePutRequest(int fd, string target);
	int receiveGetRequest(int fd, string target);
	int sendGetRequest(int fd, string target, string local, int node_initiated);
	int sendPutRequest(int fd, string local, string target);
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
