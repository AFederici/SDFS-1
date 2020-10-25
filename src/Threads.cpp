#include "../inc/Node.h"
#include "../inc/TcpSocket.h"

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

void *runRepairThread(void* tcpSocket){
	pthread_detach(pthread_self());
	TcpSocket * tcp = (TcpSocket *) tcpSocket;
	auto targets = getTcpTargets();
	int fd = tcpSocket->outgoingConnection(get<0>(target[0]), get<1>(target[1]));
	tcpSocket->sendPutRequest(fd, tcpSocket->repairReq.payload, tcpSocket->repairReq.payload);
	close(fd);
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
		if (sendMessage(fd, FILEDEL, tcp->outgoingReq.payload)) free(buffer);close(fd);return 0;
		if ((fileBytes = recv(fd, buffer, 1024, 0)) == -1){
			perror("write_server_put: recv");free(buffer);close(fd);return 0;
		}
		buffer[fileBytes] = '\0';
		Messages msg = Messages(buffer);
		if (strcmp(msg.payload.c_str(), OK)){
			cout << "DELETION FAILED " << " | " << get<0>(tcp->request_targets[id]) <<
				"::" << get<1>(tcp->request_targets[id]) << " | " <<
				tcp->outgoingReq.payload << " | " << msg.payload << endl;
			free(buffer);close(fd);return 0;
		}
	} else if (tcp->outgoingReq.type == DATA){
		vector<string> ss = string_split(tcp->outgoingReq.payload);
		if (tcp->sendPutRequest(fd, ss[0], ss[1])) { free(buffer);close(fd);return 0; }
	} else if (tcp->outgoingReq.type == FILEGET){
		vector<string> ss = string_split(tcp->outgoingReq.payload);
		if (tcp->sendGetRequest(fd, ss[0], ss[1]), 0) { free(buffer);close(fd);return 0; }
	}
	free(buffer);close(fd);return 1;
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
		nodeOwn->updateDirIntoFileSystem(); //TODO
		nodeOwn->orderReplication();

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
