#include "../inc/Node.h"

void *runUdpServer(void *udpSocket)
{
	UdpSocket* udp;
	udp = (UdpSocket*) udpSocket;
	udp->bindServer();
	pthread_exit(NULL);
}

void *runTcpServer(void* tcpSocket)
{
	TcpSocket * tcp = (TcpSocket *) tcpSocket;
	tcp->setupServer();
	tcp->runServer();
	pthread_exit(NULL);
}

void *runRepairThread(void* node){
	pthread_detach(pthread_self());
	Node * n = (Node *) node;
	auto targets = n->getTcpTargets(); // where is this function defined"
	int fd = n->tcpServent->outgoingConnection(get<0>(targets[0]), get<1>(targets[0]));
	n->tcpServent->sendPutRequest(fd, n->tcpServent->repairReq.payload, n->tcpServent->repairReq.payload, 1); // needs one more arg
	close(fd);
	n->tcpServent->repairReq = Messages(FILEDATA, "");
	return NULL;
}

void *processTcpRequests(void *tcpSocket) {
	pthread_detach(pthread_self());
	TcpSocket* tcp = (TcpSocket*) tcpSocket;
	pthread_mutex_lock(&clients_mutex);
	int id = tcp->thread_to_ind.find(pthread_self())->second;
	pthread_mutex_unlock(&clients_mutex);
	tcp->receiveMessage(tcp->clients[id]);
    close(tcp->clients[id]);
    pthread_mutex_lock(&clients_mutex);
    tcp->clients[id] = -1;
    tcp->clientsCount--;
    pthread_mutex_unlock(&clients_mutex);
	return NULL;
}

//return 0 = fail
void *runTcpClient(void* tcpSocket)
{
	TcpSocket * tcp = (TcpSocket *) tcpSocket;
	int id = tcp->thread_to_ind[pthread_self()];
	int fd = tcp->outgoingConnection(tcp->request_targets[id]);
	char * buffer = (char*)calloc(1,MAXBUFLEN);
	int fileBytes = 0;
	if (tcp->outgoingReq.type == FILEDEL){
		if (tcp->sendMessage(fd, FILEDEL, tcp->outgoingReq.payload.c_str())) free(buffer);close(fd);return (void*)0;
		if ((fileBytes = recv(fd, buffer, 1024, 0)) == -1){
			perror("write_server_put: recv");free(buffer);close(fd);return (void*)0;
		}
		buffer[fileBytes] = '\0';
		Messages msg = Messages(buffer);
		if (strcmp(msg.payload.c_str(), OK)){
			cout << "DELETION FAILED " << " | " << get<0>(tcp->request_targets[id]) <<
				"::" << get<1>(tcp->request_targets[id]) << " | " <<
				tcp->outgoingReq.payload << " | " << msg.payload << endl;
			free(buffer);close(fd);return (void*)0;
		}
	} else if (tcp->outgoingReq.type == FILEDATA){
		vector<string> ss = splitString(tcp->outgoingReq.payload, ","); // splitString in util.h? delimiter needed
		if (tcp->sendPutRequest(fd, ss[0], ss[1], 0)) { free(buffer);close(fd);return (void*)0;  }
	} else if (tcp->outgoingReq.type == FILEGET){
		vector<string> ss = splitString(tcp->outgoingReq.payload, ",");
		if (tcp->sendGetRequest(fd, ss[0], ss[1]), 0) { free(buffer);close(fd); return (void*)0; }
	}
	free(buffer);
	close(fd);return (void*)1;
}


void testMessages(UdpSocket* udp)
{
	sleep(2);
	for (int j = 0; j < 4; j++) {
		udp->sendMessage("127.0.0.1", UDP_PORT, "test message "+to_string(j));
	}
	sleep(1);
}

void *testMessages(void* tcp)
{
	TcpSocket * t = (TcpSocket *) tcp;
	sleep(2);
	cout << "p1" << endl;
	for (int j = 0; j < 4; j++) {
		int fd = t->outgoingConnection("127.0.0.1", TCP_PORT);
		t->sendMessage(fd, ACK, "yoooo AJ");
		sleep(1);
	}
	return NULL;
}

/**
 *
 * runSenderThread:
 * 1. handle messages in queue
 * 2. merge membership list
 * 3. prepare to send heartbeating
 * 4. do gossiping
 **/
void *runSenderThread(void *node)
{
	Node *nodeOwn;
	nodeOwn = (Node *) node;
	nodeOwn->activeRunning = true;

	// step: joining to the group -> just heartbeating to introducer
	Member introducer(getIP(INTRODUCER), UDP_PORT);
	nodeOwn->joinSystem(introducer);

	while (nodeOwn->activeRunning) {

		// 1. deepcopy and handle queue, and
		// 2. merge membership list
		nodeOwn->listen();

		// Volunteerily leave
		if(nodeOwn->activeRunning == false){
			pthread_exit(NULL);
		}

		//add failure detection in between listening and sending out heartbeats
		nodeOwn->failureDetection();
		nodeOwn->masterDetection();

		// keep heartbeating
		nodeOwn->localTimestamp++;
		nodeOwn->heartbeatCounter++;
		nodeOwn->updateNodeHeartbeatAndTime();
		nodeOwn->updateDirIntoFileSystem();
		if (nodeOwn->nodeInformation.ip.compare(nodeOwn->masterInformation.ip) == 0) nodeOwn->orderReplication();

		// 3. prepare to send heartbeating, and
		// 4. do gossiping
		nodeOwn->heartbeatToNode();
		nodeOwn->directoryToNode();

		time_t endTimestamp;
		time(&endTimestamp);
		double diff = difftime(endTimestamp, nodeOwn->startTimestamp);
		nodeOwn->computeAndPrintBandwidth(diff);
		if (nodeOwn->prepareToSwitch) {
			cout << "[SWITCH] I am going to swtich my mode in " << T_switch << "s" << endl;
			nodeOwn->SwitchMyMode();
		} else {
			usleep(T_period);
		}
	}

	pthread_exit(NULL);
}
