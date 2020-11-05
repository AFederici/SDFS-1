#include "../inc/Node.h"

void *runUdpServer(void *udpSocket)
{
	int id = new_thread_id();
	cout << "THREAD " << to_string(id) << "RUNNING UDP SERVER" << endl;
	UdpSocket* udp;
	udp = (UdpSocket*) udpSocket;
	udp->bindServer();
	pthread_exit(NULL);
}

void *runTcpServer(void* tcpSocket)
{
	int id = new_thread_id();
	cout << "THREAD " << to_string(id) << "RUNNING TCP SERVER" << endl;
	TcpSocket * tcp = (TcpSocket *) tcpSocket;
	tcp->setupServer();
	tcp->runServer();
	pthread_exit(NULL);
}

void *runRepairThread(void* node){
	int id = new_thread_id();
	cout << "THREAD " << to_string(id) << "RUNNING REPAIR" << endl;
	pthread_detach(pthread_self());
	Node * n = (Node *) node;
    int fd;
	auto targets = n->getTcpTargets(n->tcpServent->repairReq.payload);
    if (get<0>(targets[0]).compare(n->nodeInformation.ip) == 0){
        if (targets.size() == 1) return NULL;
        fd = n->tcpServent->outgoingConnection(get<0>(targets[1]), get<1>(targets[1]));
    }
	else fd = n->tcpServent->outgoingConnection(get<0>(targets[0]), get<1>(targets[0]));
	n->tcpServent->sendPutRequest(fd, n->tcpServent->repairReq.payload, n->tcpServent->repairReq.payload, 1);
	n->tcpServent->repairReq = Messages(FILEPUT, "");
	return NULL;
}

void *processTcpRequests(void *tcpSocket) {
	pthread_detach(pthread_self());
	TcpSocket* tcp = (TcpSocket*) tcpSocket;
	pthread_mutex_lock(&process_q_mutex);
	tuple<int, int> process = tcp->process_q.front();
	tcp->process_q.pop();
	pthread_mutex_unlock(&process_q_mutex);
	cout << "THREAD " << to_string(get<0>(process)) << "PROCESSING CLIENT # " << to_string(get<1>(process)) << endl;
	tcp->receiveMessage(tcp->clients[get<1>(process)]);
	cout << "MESSAGE RECEIVED" << endl;
    close(tcp->clients[get<1>(process)]);
    pthread_mutex_lock(&clients_mutex);
    tcp->clients[get<1>(process)] = -1;
    tcp->clientsCount--;
    pthread_mutex_unlock(&clients_mutex);
	cout << "THREAD " << to_string(get<0>(process)) << "FINISHED. " << to_string(tcp->clientsCount) << " left on server."<< endl;
	return NULL;
}

//return 0 = fail
void *runTcpClient(void* tcpSocket)
{
	TcpSocket * tcp = (TcpSocket *) tcpSocket;
	pthread_mutex_lock(&runner_q_mutex);
	tuple<string, string, string, int> runner = tcp->runner_q.front();
	tcp->runner_q.pop();
	pthread_mutex_unlock(&runner_q_mutex);
	int fd = tcp->outgoingConnection(make_tuple(get<0>(runner), get<1>(runner), get<2>(runner)));
	cout << "THREAD " << to_string(get<3>(runner)) << " CONNECTED TO " << get<0>(runner) << " FOR " << messageTypes[tcp->outgoingReq.type] << endl;
	char * buffer = (char*)calloc(1,MAXBUFLEN+1);
	int fileBytes = 0;
	if (tcp->outgoingReq.type == FILEDEL){
		if (tcp->sendMessage(fd, FILEDEL, tcp->outgoingReq.payload.c_str())) free(buffer);close(fd);return (void*)0;
		if ((fileBytes = read(fd, buffer, MAXBUFLEN)) == -1){
			perror("write_server_put: read");free(buffer);close(fd);return (void*)0;
		}
		buffer[fileBytes] = '\0';
		Messages msg = Messages(buffer);
		if (strcmp(msg.payload.c_str(), OK)){
			cout << "DELETION FAILED " << " | " << get<0>(runner) <<
				"::" << get<1>(runner) << " | " <<
				tcp->outgoingReq.payload << " | " << msg.payload << endl;
			free(buffer);close(fd);return (void*)0;
		}
	} else if (tcp->outgoingReq.type == FILEPUT){
		vector<string> ss = splitString(tcp->outgoingReq.payload, ",");
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
	for (int j = 0; j < REP; j++) {
		udp->sendMessage("127.0.0.1", UDP_PORT, "test message "+to_string(j));
	}
	sleep(1);
}

void *testMessages(void* tcp)
{
	TcpSocket * t = (TcpSocket *) tcp;
	sleep(2);
	cout << "p1" << endl;
	for (int j = 0; j < REP; j++) {
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

	// step: joining to the group -> just heartbeating to introducer
	Member introducer(nodeOwn->introducerIP, UDP_PORT);
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
