#ifndef UDPSOCKET_H
#define UDPSOCKET_H
#include <iostream>
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
using namespace std;
using std::string;
using std::queue;

#define MAXBUFLEN 1024
#define LOSS_RATE 0

class UdpSocket {
public:
	string serverPort;
	unsigned long byteSent;
	unsigned long byteReceived;
	queue<string> qMessages;

	void bindServer();
	void sendMessage(string ip, string port, string message);
	UdpSocket(string port);

};
#endif //UDPSOCKET_H
