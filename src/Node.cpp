#include "../inc/Node.h"
#include "../inc/Utils.h"

Node::Node() : Node(ALL2ALL) {}

//TEST MODE
Node::Node(char * p1, char * p2, ModeType mode){
	Directory * d = new Directory();
	udpServent = new UdpSocket(p1);
	tcpServent = new TcpSocket(p2, d);
	udpPort = p1;
	tcpPort = p2;
	localTimestamp = 0;
	heartbeatCounter = 0;
	time(&startTimestamp);
	runningMode = mode;
	activeRunning = false;
	introducerIP = "127.0.0.1";
	prepareToSwitch = false;
	logWriter = new Logger(LOGGING_FILE_NAME);
	masterInformation = Member();
	tcpServent->repairReq = Messages(FILEDATA, "");
}

Node::Node(ModeType mode)
{
	Directory * d = new Directory();
	tcpServent = new TcpSocket(TCP_PORT, d);
	udpServent = new UdpSocket(UDP_PORT);
	udpPort = UDP_PORT;
	tcpPort = TCP_PORT;
	localTimestamp = 0;
	heartbeatCounter = 0;
	time(&startTimestamp);
	runningMode = mode;
	introducerIP = getIP(INTRODUCER);
	activeRunning = false;
	prepareToSwitch = false;
	logWriter = new Logger(LOGGING_FILE_NAME);
	masterInformation = Member();
	tcpServent->repairReq = Messages(FILEDATA, "");
}

void Node::computeAndPrintBandwidth(double diff)
{
#ifdef LOG_VERBOSE
	cout << "total " << udpServent->byteSent << " bytes sent" << endl;
	cout << "total " << udpServent->byteReceived << " bytes received" << endl;
	printf("elasped time is %.2f s\n", diff);
#endif
	if (diff > 0) {
		double bandwidth = udpServent->byteSent/diff;
		string message = "["+to_string(this->localTimestamp)+"] B/W usage: "+to_string(bandwidth)+" bytes/s";
#ifdef LOG_VERBOSE
		printf("%s\n", message.c_str());
#endif
		this->logWriter->printTheLog(BANDWIDTH, message);
	}
}

vector<tuple<string,string, string>> Node::getRandomNodesToGossipTo()
{
    vector<tuple<string, string, string>> availableNodesInfo;
    vector<tuple<string, string, string>> selectedNodesInfo;
    vector<int> indexList;
    int availableNodes = 0;
    for(auto& element: this->membershipList){
        tuple<string, string, string> keyPair = element.first;
        tuple<int, int, int> valueTuple = element.second;
        //dont gossip to self or failed nodes
        if(get<0>(keyPair).compare(this->nodeInformation.ip) && (get<2>(valueTuple) != 1)){
            availableNodesInfo.push_back(keyPair);
            indexList.push_back(availableNodes++);
        }
    }
    switch (this->runningMode) {
        case GOSSIP: {
            srand(time(NULL));
            // N_b is a predefined number
            if (availableNodes <= N_b) return availableNodesInfo;
            int nodeCount = 0;
            while (nodeCount < N_b) {
                int randomNum = rand() % availableNodes;
                selectedNodesInfo.push_back(availableNodesInfo[indexList[randomNum]]);
                indexList.erase(indexList.begin() + randomNum);
                availableNodes--;
                nodeCount++;
            }
            return selectedNodesInfo;
        }
        //ALL2ALL
        default: {
            return availableNodesInfo;
        }
    }
}

void Node::updateNodeHeartbeatAndTime()
{
	string startTime = ctime(&startTimestamp);
	startTime = startTime.substr(0, startTime.find("\n"));
	tuple<string, string, string> keyTuple(nodeInformation.ip, nodeInformation.udpPort,startTime);
	tuple<int, int, int> valueTuple(heartbeatCounter, localTimestamp, 0);
	this->membershipList[keyTuple] = valueTuple;
}

string Node::populateMembershipMessage()
{
	//The string we send will be seperated line by line --> IP,UDP_PORT,HeartbeatCounter,FailFlag
	string mem_list_to_send = "";
	//Assume destination already exists in the membership list of this node, just a normal heartbeat
	switch (this->runningMode)
	{
		case GOSSIP:
			mem_list_to_send = populateIntroducerMembershipMessage(); //same functionality
			break;

		default:
			string startTime = ctime(&startTimestamp);
			startTime = startTime.substr(0, startTime.find("\n"));
			mem_list_to_send += nodeInformation.ip + "," + nodeInformation.udpPort + "," + startTime + ",";
			tuple<string, string, string> mapKey(nodeInformation.ip, nodeInformation.udpPort, startTime); // member_id
			mem_list_to_send += to_string(heartbeatCounter) + "," + to_string(0) + "," + to_string(get<0>(this->file_system[mapKey])) + "\n";
			break;
	}
	return mem_list_to_send;
}

string Node::populateIntroducerMembershipMessage(){
	string mem_list_to_send = "";
	for (auto& element: this->membershipList) {
		tuple<string, string, string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;
		mem_list_to_send += get<0>(keyTuple) + "," + get<1>(keyTuple) + "," + get<2>(keyTuple) + ",";
		mem_list_to_send += to_string(get<0>(valueTuple)) + "," + to_string(get<2>(valueTuple));
		mem_list_to_send += to_string(get<0>(this->file_system[keyTuple])) + "\n";
	}
	return mem_list_to_send;
}

/**
 *
 * HeartbeatToNode: Sends a string version of the membership list to the receiving node.
 * The receiving node will convert the string to a <string, long> map where
 * the key is the Addresss (IP + UDP_PORT) and value is the heartbeat counter. We then compare the Member.
 *
 **/
int Node::heartbeatToNode()
{
	// 3. pick nodes from membership list
	string mem_list_to_send = populateMembershipMessage();
	vector<tuple<string,string,string>> targetNodes = getRandomNodesToGossipTo();

	// 4. do gossiping
	for (uint i=0; i<targetNodes.size(); i++) {
		Member destination(get<0>(targetNodes[i]), get<1>(targetNodes[i]));
		string message = "["+to_string(this->localTimestamp)+"] node "+destination.ip+"/"+destination.udpPort+"/"+get<2>(targetNodes[i]);
		this->logWriter->printTheLog(GOSSIPTO, message);
		Messages msg(HEARTBEAT, mem_list_to_send);
		udpServent->sendMessage(destination.ip, destination.udpPort, msg.toString());
	}
	return 0;
}

void Node::masterDetection(){
	string potential = "";
	string port = "";
	if (masterInformation.ip.size() == 0){
		for (auto &el : membershipList){
			if (get<0>(el.first) > potential){
				potential = get<0>(el.first);
				port = get<1>(el.first);
			}
		}
	}
	else { votes.clear(); }
	if (port.size() > 0) {
		if (strcmp(potential.c_str(), nodeInformation.ip.c_str())){ // fixed typo
			Messages msg(VOTE, nodeInformation.toString());
			udpServent->sendMessage(potential, port, msg.toString());
		}
		else{
			votes.insert(nodeInformation.identity()); //add ==> insert
			int alive_nodes = membershipList.size();
			if (alive_nodes >= 4 && votes.size() > ((alive_nodes / 2) + 1)){
				masterInformation = nodeInformation;
				Messages msg(VOTEACK, nodeInformation.toString()); //identity ==> toString
				for (auto &el : votes){
					if (get<0>(el).compare(nodeInformation.ip)){
						udpServent->sendMessage(get<0>(el), get<1>(el), msg.toString());
					}
				}
			}
		}
	}
}
/*
void Node::orderReplication(){
	vector<tuple<string, string>> targets = getTcpTargets();
	int ind = 0;
	for (auto &el : replicas_list){
		if (get<0>(el.second) == 0) continue;
		if ((get<1>el.second).size() != 4) {
			while ((get<1>el.second).count())
			Messages msg(INITREPLICATE, el.first);
			udpServent->sendMessage(get<0>(targets[ind]), get<1>(targets[ind]), msg);
			udpServent->sendMessage(get<0>(targets[(ind + 1) % targets]), get<1>(targets[(ind + 1) % targets]), msg);
			udpServent->sendMessage(get<0>(targets[(ind + 2) % targets]), get<1>(targets[(ind + 2) % targets]), msg);
			ind = (ind + 1) % targets;
		}
	}
}
*/

void Node::orderReplication(){
	for (auto &el : replicas_list){
		if (get<0>(el.second) == 0) continue; //bad file status
		if (get<1>(el.second).size() < 4) {
			auto it = (get<1>(el.second)).cbegin();
			int random = rand() % replicas_list.size(); // what is v?
			while (random--) {
				++it;
			}
			Messages msg(REPLICATE, el.first);
			udpServent->sendMessage(get<0>(*it), get<1>(*it), msg.toString()); // added toString
		}
	}
}

int Node::failureDetection(){
	//1. check local membership list for any timestamps whose curr_time - timestamp > T_fail
	//2. If yes, mark node as local failure, update fail flag to 1 and update timestamp to current time
	//3. for already failed nodes, check to see if curr_time - time stamp > T_cleanup
	//4. If yes, remove node from membership list
	vector<tuple<string,string,string>> removedVec;
	for(auto& element: this->membershipList){
		tuple<string,string,string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;
#ifdef LOG_VERBOSE
		cout << "checking " << get<0>(keyTuple) << "/" << get<1>(keyTuple) << "/" << get<2>(keyTuple) << endl;
#endif
		if ((get<0>(keyTuple).compare(nodeInformation.ip) == 0) && (get<1>(keyTuple).compare(nodeInformation.udpPort) == 0)) {
			// do not check itself
#ifdef LOG_VERBOSE
			cout << "skip it" << endl;
#endif
			continue;
		}
		if(get<2>(valueTuple) == 0){
			if(localTimestamp - get<1>(valueTuple) > T_timeout){
				get<1>(this->membershipList[keyTuple]) = localTimestamp;
				get<2>(this->membershipList[keyTuple]) = 1;
				string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(keyTuple)+"/"+get<1>(keyTuple)+"/"+get<2>(keyTuple)+": Local Failure";
				cout << "[FAIL]" << message.c_str() << endl;
				this->logWriter->printTheLog(FAIL, message);
			}
		}
		else{
			if(localTimestamp - get<1>(valueTuple) > T_cleanup){
				auto iter = this->membershipList.find(keyTuple);
				if (iter != this->membershipList.end()) {
					removedVec.push_back(keyTuple);
				}
			}
		}
	}

	// O(c*n) operation, but it ensures safety
	for (uint i=0; i<removedVec.size(); i++) {
		auto iter = this->membershipList.find(removedVec[i]);
		if (iter != this->membershipList.end()) {
			this->membershipList.erase(iter);
			string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(removedVec[i])+"/"+get<1>(removedVec[i])+"/"+get<2>(removedVec[i])+": REMOVED FROM LOCAL MEMBERSHIP LIST";
			cout << "[REMOVE]" << message.c_str() << endl;
			this->logWriter->printTheLog(REMOVE, message);
		}
		for (auto &el : this->replicas_list){
			get<1>(el.second).erase(removedVec[i]); // added get<1>
		}
		this->file_system.erase(removedVec[i]);
		if (get<0>(removedVec[i]).compare(masterInformation.ip) == 0) masterInformation.ip = "";
	}
	return 0;
}


int Node::joinSystem(Member introducer)
{
	string mem_list_to_send = populateMembershipMessage();
	Messages msg(JOIN, mem_list_to_send);
	string message = "["+to_string(this->localTimestamp)+"] sent a request to "+introducer.ip+"/"+introducer.udpPort+", I am "+nodeInformation.ip+"/"+nodeInformation.udpPort;
	cout << "[JOIN]" << message.c_str() << endl;
	this->logWriter->printTheLog(JOINGROUP, message);
	udpServent->sendMessage(introducer.ip, introducer.udpPort, msg.toString());
	return 0;
}

int Node::requestSwitchingMode()
{
	string message = nodeInformation.ip+","+nodeInformation.udpPort;
	Messages msg(SWREQ, message);
	for(auto& element: this->membershipList) {
		tuple<string,string,string> keyTuple = element.first;
		cout << "[SWITCH] sent a request to " << get<0>(keyTuple) << "/" << get<1>(keyTuple) << endl;
		udpServent->sendMessage(get<0>(keyTuple), get<1>(keyTuple), msg.toString());
	}
	return 0;
}

int Node::SwitchMyMode()
{
	sleep(T_switch); // wait for a while
	udpServent->qMessages = queue<string>(); // empty all messages
	switch (this->runningMode) {
		case GOSSIP: {
			this->runningMode = ALL2ALL;
			cout << "[SWITCH] === from gossip to all-to-all ===" << endl;
			break;
		}
		case ALL2ALL: {
			this->runningMode = GOSSIP;
			cout << "[SWITCH] === from all-to-all to gossip ===" << endl;
			break;
		}
		default:
			break;
	}
	prepareToSwitch = false;
	return 0;
}

int Node::listen()
{
	//look in queue for any strings --> if non empty, we have received a message and need to check the membership list
	// 1. deepcopy and handle queue
	queue<string> qCopy(udpServent->qMessages);
	udpServent->qMessages = queue<string>();

	int size = qCopy.size();

	// 2. merge membership list
	for (int j = 0; j < size; j++) {
		readMessage(qCopy.front());
		// Volunteerily leave
		if(this->activeRunning == false){
			return 0;
		}
		qCopy.pop();
	}
	return 0;
}

void Node::debugMembershipList()
{
	cout << "Membership list [" << this->membershipList.size() << "]:" << endl;
	cout << "IP/Port/JoinedTime:Heartbeat/LocalTimestamp/FailFlag" << endl;
	string message = "";
	for (auto& element: this->membershipList) {
		tuple<string,string,string> keyTuple = element.first;
		tuple<int, int, int> valueTuple = element.second;
		message += get<0>(keyTuple)+"/"+get<1>(keyTuple)+"/"+get<2>(keyTuple);
		message += ": "+to_string(get<0>(valueTuple))+"/"+to_string(get<1>(valueTuple))+"/"+to_string(get<2>(valueTuple))+"\n";
	}
	cout << message.c_str() << endl;
	this->logWriter->printTheLog(MEMBERS, message);
}

void Node::processHeartbeat(string message) {
	bool changed = false;
	vector<string> incomingMembershipList = splitString(message, "\n");
	vector<string> membershipListEntry;
	for(string list_entry: incomingMembershipList){
		cout << "handling with " << list_entry << endl;
		if (list_entry.size() == 0) {
			continue;
		}
		membershipListEntry.clear();
		membershipListEntry = splitString(list_entry, ",");
		if (membershipListEntry.size() < 6) { cout << "ERRRORRRRR" << endl; fflush(stdout); continue; }
		int incomingHeartbeatCounter = stoi(membershipListEntry[3]);
		int failFlag = stoi(membershipListEntry[4]);
		tuple<string,string,string> mapKey(membershipListEntry[0], membershipListEntry[1], membershipListEntry[2]);

		if ((get<0>(mapKey).compare(nodeInformation.ip) == 0) && (get<1>(mapKey).compare(nodeInformation.udpPort) == 0)) {
			// Volunteerily leave if you are marked as failed
			if(failFlag == 1){
				this->activeRunning = false;
				string message = "["+to_string(this->localTimestamp)+"] node "+this->nodeInformation.ip+"/"+this->nodeInformation.udpPort+" is left";
				cout << "[VOLUNTARY LEAVE]" << message.c_str() << endl;
				this->logWriter->printTheLog(LEAVE, message);
				return;
			}

			// do not check own heartbeat
#ifdef LOG_VERBOSE
			cout << "skip it" << endl;
#endif
			continue;
		}
		auto it = this->membershipList.find(mapKey);
		if (it == this->membershipList.end() && failFlag == 0) {
			tuple<int, int, int> valueTuple(incomingHeartbeatCounter, localTimestamp, failFlag);
			this->membershipList[mapKey] = valueTuple;

			string message = "["+to_string(this->localTimestamp)+"] new node "+get<0>(mapKey)+"/"+get<1>(mapKey)+"/"+get<2>(mapKey)+" is joined";
			cout << "[JOIN]" << message.c_str() << endl;
			this->logWriter->printTheLog(JOINGROUP, message);
			changed = true;
		} else if(it != this->membershipList.end()) {
			// update heartbeat count and local timestamp if fail flag of node is not equal to 1. If it equals 1, we ignore it.
			if(get<2>(this->membershipList[mapKey]) != 1){
				//if incoming membership list has node with fail flag = 1, but fail flag in local membership list = 0, we have to set fail flag = 1 in local
				if(failFlag == 1){
					get<2>(this->membershipList[mapKey]) = 1;
					get<1>(this->membershipList[mapKey]) = localTimestamp;
					string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(mapKey)+"/"+get<1>(mapKey)+"/"+get<2>(mapKey)+": Disseminated Failure";
					cout << "[FAIL]" << message.c_str() << endl;
					this->logWriter->printTheLog(FAIL, message);
				}
				else{
					int currentHeartbeatCounter = get<0>(this->membershipList[mapKey]);
					if(incomingHeartbeatCounter > currentHeartbeatCounter){
						get<0>(this->membershipList[mapKey]) = incomingHeartbeatCounter;
						get<1>(this->membershipList[mapKey]) = localTimestamp;
						// Need to split the string and make it into a tuple
						if (this->file_system.count(mapKey)) get<0>(this->file_system[mapKey]) = stoi(membershipListEntry[5]); //update nodes byte info in terms of files


						string message = "["+to_string(this->localTimestamp)+"] node "+get<0>(mapKey)+"/"+get<1>(mapKey)+"/"+get<2>(mapKey)+" from "+to_string(currentHeartbeatCounter)+" to "+to_string(incomingHeartbeatCounter);
#ifdef LOG_VERBOSE
						cout << "[UPDATE]" << message.c_str() << endl;
#endif
						this->logWriter->printTheLog(UPDATE, message);
					}
				}
				break;
			}
		}
	}

	// If membership list changed in all-to-all, full membership list will be sent
	if(changed && this->runningMode == ALL2ALL){
		string mem_list_to_send = populateIntroducerMembershipMessage();
		vector<tuple<string,string,string>> targetNodes = getRandomNodesToGossipTo();

		for (uint i=0; i<targetNodes.size(); i++) {
			Member destination(get<0>(targetNodes[i]), get<1>(targetNodes[i]));

			string message = "["+to_string(this->localTimestamp)+"] node "+destination.ip+"/"+destination.udpPort+"/"+get<2>(targetNodes[i]);
#ifdef LOG_VERBOSE
			cout << "[Gossip]" << message.c_str() << endl;
#endif
			this->logWriter->printTheLog(GOSSIPTO, message);

			Messages msg(HEARTBEAT, mem_list_to_send);
			udpServent->sendMessage(destination.ip, destination.udpPort, msg.toString());

		}
	}
}

/**
 * given a string message which contains a membership list, we will take the string,
 * split it by returns, and then split it by commas, to then compare the heartbeat counters
 * of each IP,UDP_PORT,timestamp tuple with the membership list of the receiving Node.
 *
 * Found help on how to do string processing part of this at
 * https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
 */
void Node::readMessage(string message){
	Messages msg(message);
	switch (msg.type) {
		case HEARTBEAT:
		case JOINRESPONSE:{
			processHeartbeat(msg.payload);
			break;
		}
		case SWREQ: {
			// got a request, send an ack back
			vector<string> fields = splitString(msg.payload, ",");
			if (fields.size() == 2) {
				cout << "[SWITCH] got a request from "+fields[0]+"/"+fields[1] << endl;
				string messageReply = nodeInformation.ip+","+nodeInformation.udpPort;
				Messages msgReply(SWRESP, messageReply);
				udpServent->sendMessage(fields[0], fields[1], msgReply.toString());
				prepareToSwitch = true;
			}
			break;
		}
		case SWRESP: {
			// got an ack
			vector<string> fields = splitString(msg.payload, ",");
			if (fields.size() == 2) {
				cout << "[SWITCH] got an ack from "+fields[0]+"/"+fields[1] << endl;
			}
			break;
		}

		case JOIN:{
			string introducerMembershipList;
			introducerMembershipList = populateIntroducerMembershipMessage();
			Messages response(JOINRESPONSE, introducerMembershipList);
			vector<string> fields = splitString(msg.payload, ",");
			cout << fields[0]<< "::" << fields[1] << " " << response.toString() << endl;
			udpServent->sendMessage(fields[0], fields[1], response.toString());
			break;
		}

		default:
			readSdfsMessage(message);

	}
	//debugMembershipList();
}

int main(int argc, char **argv)
{
	ModeType mode = ALL2ALL;
	int rc;
	Node * node;
	if (argc == 3){
		cout << "---------------- LOCAL MODE -----------------" << endl;
		node = new Node(argv[1],argv[2], mode);
	} else {node = new Node(mode);}
	Member own(getIP(), node->udpPort, node->tcpPort, node->localTimestamp, node->heartbeatCounter);
	node->nodeInformation = own;
	cout << "Starting Node at " << node->nodeInformation.ip << "/";
	cout << node->nodeInformation.udpPort << "..." << endl;
	string startTime = ctime(& node -> startTimestamp);
	startTime = startTime.substr(0, startTime.find("\n"));
	tuple<string,string,string> mapKey(own.ip, own.udpPort, startTime);
	tuple<int, int, int> valueTuple(own.timestamp, own.heartbeatCounter, 0);
	node->membershipList[mapKey] = valueTuple;
	node->debugMembershipList();
	get<0>(node->file_system[own.identity()]) = 0;
	int *ret;
	string cmd;
	bool joined = false;
	if ((rc = pthread_create(&(node->thread_arr[0]), NULL, runUdpServer, (void *)node->udpServent)) != 0) {
		cout << "Error:unable to create thread," << rc << endl; exit(-1);
	}

	if ((rc = pthread_create(&(node->thread_arr[1]), NULL, runTcpServer, (void *)node->tcpServent)) != 0) {
		cout << "Error:unable to create thread," << rc << endl; exit(-1);
	}
	while(1){
		cin >> cmd;
		if(cmd == "join"){
			if (node->activeRunning) continue;
			node->activeRunning = true;
			if ((rc = pthread_create(&node->thread_arr[2], NULL, runSenderThread, (void *)node)) != 0) {
				cout << "Error:unable to create thread," << rc << endl;
				exit(-1);
			}
			joined = true;
		} else if(cmd == "leave"){
			if(joined){
				node->activeRunning = false;
				node->tcpServent->endSession[MAX_CLIENTS] = 1; // fixed typo
				node->tcpServent->dir->clear();
				closeFd(node->tcpServent->serverSocket); //  removed caller and fixed typo
				pthread_join(node->thread_arr[2], (void **)&ret);
				pthread_join(node->thread_arr[1], (void **)&ret);
				string message = "["+to_string(node->localTimestamp)+"] node "+node->nodeInformation.ip+"/"+node->nodeInformation.udpPort+" is left";
				cout << "[LEAVE]" << message.c_str() << endl;
				node->logWriter->printTheLog(LEAVE, message);
				sleep(2); // wait for logging
				joined = false;
			}
		} else if(cmd == "id"){
			cout << "ID: (" << node->nodeInformation.ip << ", " << node->nodeInformation.udpPort << ")" << endl;
		} else if(cmd == "member"){
			node->debugMembershipList();
		} else if(cmd == "exit"){
			cout << "exiting..." << endl;
			break;
		} else if(cmd == "switch") {
			if(joined){
				node->requestSwitchingMode();
			}
		} else if(cmd == "mode") {
			cout << "In " << node->runningMode << " mode" << endl;
		} else if(cmd == "store"){
			node->tcpServent->dir->store(); // added node->
			break;
		} else {
			vector<string> ss = splitString(cmd, " ");
			if (ss[0] == "put"){
				node->handlePut(ss[1], ss[2]);
			} else if (ss[0] == "get"){
				node->handleGet(ss[1], ss[2]);
			} else if (ss[0] == "delete"){
				node->handleDelete(ss[1]);
			} else if (ss[0] == "ls"){
				cout << "---- ls ----" << endl;
				if (node->replicas_list.count(ss[1])){
					if (get<0>(node->replicas_list[ss[1]])){
						for (auto &el : get<1>(node->replicas_list[ss[1]])){ // added node-> Not sure what's going on here...
							cout << Member(get<0>(el), get<1>(el)).toString() << endl;
						}
					}
				}
			} else{

				cout << "[join] join to a group via fixed introducer" << endl;
				cout << "[leave] leave the group" << endl;
				cout << "[id] print id (IP/UDP_PORT)" << endl;
				cout << "[member] print all membership list" << endl;
				cout << "[exit] terminate process" << endl;
				cout << "[get] [sdfsname] [localfile] retrive data from file [sdfsname] into [localfile]" << endl;
				cout << "[delete] [sdfsname] remove file [sdfsname] from the system" << endl;
				cout << "[put] [localfile] [sdfsname] store [localfile] in the file [sdfsname]" << endl;
				cout << "[ls] [filename] list all machines storing [filename]" << endl;
				cout << "[store] list all files on this machine" << endl;

			}
		}
	}

	pthread_kill(node->thread_arr[0], SIGUSR1);
	pthread_kill(node->thread_arr[1], SIGUSR1);
	if(joined) pthread_kill(node->thread_arr[2], SIGUSR1);
	pthread_exit(NULL);
	return 1;
}





//// MP2 API
