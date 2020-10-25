#include "../inc/Node.h"

int Node::directoryToNode()
{
	string to_send = populateFileLocationMessage();
	vector<tuple<string,string,string>> targetNodes = getRandomNodesToGossipTo();
	for (uint i=0; i<targetNodes.size(); i++) {
		Messages msg(FILESYSTEM, to_send);
		udpServent->sendMessage(get<0>(targetNodes[i]), get<0>(targetNodes[i]), msg.toString());
	}
	return 0;
}

map<tuple<string, string, string>, map<string, tuple<int, int>>> file_system;
string Node::populateFileLocationMessage(){
	string to_send = "";
	for (auto& element: this->file_system) {
		map<string, tuple<int, int>> valueTuple = get<1>(element.second);
		to_send += this->nodeInformation.toString() + "," + valueTuple.first;
		to_send += to_string(get<0>(valueTuple.second)) + "," + to_string(get<1>(valueTuple.second)) + "\n";
	}
	return to_send;
}

void Node::updateDirIntoFileSystem(){
	tuple<int, map<string, tuple<int, int>>> items = file_system[nodeInformation.identity()];
	get<1>(items) = tcpSocket->dir.file_heartbeats;
	for (auto &el : get<1>(items)){
		get<0>(replicas_list[el.first]) = get<1>(el.second);
		get<1>(replicas_list[el.first]).insert(nodeInformation.identity());
	}
}

//first arg is node info in string form, not comma seperated
void Node::readSdfsMessage(string m){
    Messages msg(message);
	switch (msg.type) {
        case VOTE: {
            if (masterInformation) return;
            if (nodeInformation.ip != maxIP) return;
            vector<string> address = string_split(msg.payload, "::");
            votes.add(make_tuple(address[0], address[1], address[2]));
        }
        case REPLICATE: {
			//either file is removed or theres now 4 copies
			if (get<0>val == 0 || (get<1>val).size() >= 4) return;
			pthread_mutex_lock(&repair_mutex);
			if (threads[7] != -1) { pthread_mutex_unlock(&repair_mutex); return;} //repair thread is busy
			string s = msg.payload + "," + msg.payload;
			tcpServent.repairReq = Messages(FILEDATA, s);
            pthread_create(threads + 7, NULL, runRepairThread, (void *)tcpServent);
			pthread_mutex_lock(&repair_mutex);
        }
        case FILESYSTEM: {
            mergeFileSystem(msg.payload);
        }
        case VOTEACK: {
            if (masterInformation && (masterInformation.ip) == (nodeInformation.ip)) return;
            vector<string> address = string_split(msg.payload, "::");
            masterInformation = Member(address[0], address[1], address[2]);
        }
		/*
		case REPLICACK: {
            if (masterInformation && (masterInformation.ip) != (nodeInformation.ip)) return;
            vector<string> ss = string_split(msg.payload, ",");
            Messages resp(REPLICATE, ss[1]);
            vector<string> address = string_split(ss[0], "::");
            udpServent->sendMessage(address[0], address[1], resp.toString());
        }
        case INITREPLICATE: {
            if (masterInformation && (masterInformation.ip) == (nodeInformation.ip)) return;
			pthread_mutex_lock(&repair_mutex);
			int val = threads[7];
			pthread_mutex_unlock(&repair_mutex);
			if (val != -1) return; //repair thread is busy
            tuple<int, set<tuple<string, string, string>> val = replicas_list[msg.payload];
            //either file is removed, theres now 4 copies, or we already have it
            if (get<0>val == 0 || (get<1>val).size() >= 4 || (get<1>val).count(nodeInformation.identity())) return;
            string payload = nodeInformation.toString() + "," + msg.payload;
            Messages resp(REPLICACK, payload);
            udpServent->sendMessage(fields[0], fields[1], resp.toString());
        }
		*/
		case default : {}
}



vector<tuple<string, string>> Node::getTcpTargets(){
	vector<string, string, string, int> v;
	for(auto& element: file_system){
		tuple<string, string, string> keyPair = element.first;
		if (get<0>(keyPair).compare(nodeInformation.ip)) continue;
		v.push_back(make_tuple(get<0>keyPair, get<1>keyPair, get<2>keyPair, get<0>(element.second)));
	}
	std::sort(v.begin(), v.end(), TupleCompare());
	vector<tuple<string, string>> targets;
	for (int i = 0; i < 4; i++) targets.push_back(make_tuple(get<0>(v[i]), get<1>(v[i])));
	int index = 4;
	int target_num = 0;
	while (target_num < 4 && index < v.size()){
		//if fail state find a new option
		if (get<2>membershipList[make_tuple(get<0>targets[target_num], get<1>targets[target_num], get<2>targets[target_num])])
		{
			while (index < v.size()){
				if (!get<2>membershipList[make_tuple(get<0>v[index], get<1>v[index], get<2>v[index])])
				{
				targets[target_num] = make_tuple(get<0>v[index], get<1>v[index]);
				}
				index++;
			}
		}
		target_num++
	}
	return targets;
}

void Node::threadConsistency(){
	set<tuple<string, string>> sent;
	int **t = calloc(1, sizeof(int*));
	while (sent.size() < 4){
		tcpServent->request_targets = getTcpTargets();
		int index = 0;
		for (int i = 0; i < (4 - sent.size()); i++){
			while (send.count(tcpServent->request_targets[index])) index++;
			pthread_mutex_lock(&id_mutex);
			if ((rc = pthread_create(&threads[3+i], NULL, runTcpClient, (void *)tcpServent)) != 0) {
				cout << "Error:unable to create thread," << rc << endl; exit(-1);
			}
			tcpServent->thread_to_ind[threads[3+i]] = index;
			pthread_mutex_unlock(&id_mutex);
		}
		for (int i = 0; i < (4 - sent.size()); i++){
			*t = 0;
			pthread_join(threads[3+i], t);
			if (*t) sent.insert(tcpServent->request_targets[tcpServent->thread_to_ind[threads[3+i]]]);
		}
	}
	free(t);
}

void Node::mergeFileSystem(string m){
	vector<string> incomingUpdates = splitString(m, "\n");
	vector<string> entry;
	for(string list_entry: incomingUpdates) {
		if (list_entry.size() == 0) continue;
		entry.clear();
		entry = splitString(list_entry, ",");
		if (entry.size() < 4) continue;
		vector<string> address = string_split(entry[0]);
		string file = entry[1];
		int status = stoi(entry[2]);
		int hb = stoi(entry[3]);
		tuple<string,string,string> mapKey(address[0], address[1], address[2]);
		if ((get<0>(mapKey).compare(nodeInformation.ip) == 0) continue;
		map<string, tuple<int,int>> val = get<1>(file_system[mapKey]);
		if (!val.count(file) || hb > get<0>(val[file])){ //doesnt exist or is being updated
			val[file] = make_tuple(hb, status);
			get<0>(replicas_list[file]) = status;
			get<1>(replicas_list[file]).insert(mapKey);
		}
	}
}

void Node::handlePut(string s1, string s2){
	string fileinfo = s1 + "," + s2;
	tcpServent->outgoingReq = Messages(DATA, fileinfo);
	threadConsistency();
}

void Node::handleGet(string s1, string s2){
	tcpServent->request_targets.clear();
	string fileinfo = s1 + "," + s2;
	tcpServent->outgoingReq = Messages(DATA, fileinfo);
	void ** t = calloc(sizeof(int*));
	*t = 0;
	while (!(*t)){
		if (!replicas_list.count(s1)){
			cout << "[GET] Error: " << s1 << " does not exist";
			free(t);
			return;
		}
		for (auto &el : replicas_list[s1]){
			tcpServent->request_targets.push_back(el);
			pthread_mutex_lock(&id_mutex);
			if ((rc = pthread_create(&threads[3], NULL, runTcpClient, (void *)tcpServent)) != 0) {
				cout << "Error:unable to create thread," << rc << endl; exit(-1);
			}
			tcpServent->thread_to_ind[threads[3]] = tcpServent->request_targets.size()-1;
			pthread_mutex_unlock(&id_mutex);

			pthread_join(threads[3+i], t);
			if (*t) free(t); return;
		}
	}
}

void Node::handleDelete(string s1){
	tcpServent->outgoingReq = Messages(FILEDEL, s1);
	threadConsistency();
}
