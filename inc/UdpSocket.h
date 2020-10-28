#ifndef UDPSOCKET_H
#define UDPSOCKET_H
#include <iostream>
#include <string>
#include <queue>
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
#ifdef _LINUX
#include <bits/stdc++.h>
#endif


using std::string;
using std::queue;
using std::get;

#define MAXBUFLEN 1024
#define LOSS_RATE 0

class UdpSocket {
public:
	char* serverPort;
	unsigned long byteSent;
	unsigned long byteReceived;
	queue<string> qMessages;

	void bindServer();
	void sendMessage(string ip, string port, string message);
	UdpSocket(char* port);

};
#endif //UDPSOCKET_H
