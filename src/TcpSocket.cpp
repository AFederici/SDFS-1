#include "../inc/TcpSocket.h"

TcpSocket::TcpSocket(char* port, Directory * direct){
	for (int i = 0; i < MAX_CLIENTS; i++){ clients[i] = -1; endSession[i] = 0;}
	endSession[MAX_CLIENTS] = 0;
	byteSent = 0;
	serverPort = port;
	dir = direct;
	clientsCount = 0;
}

void closeFdAfterWrite(int fd){
    int stillReading = shutdown(fd, SHUT_RD);
    if (stillReading){ perror("shutdown"); exit(1);}
    close(fd);
}

void closeFd(int fd){
    if (shutdown(fd, SHUT_RDWR) != 0) {
        perror("shutdown():");
    }
    close(fd);
}

int TcpSocket::outgoingConnection(tuple<string,string, string> address){
	return outgoingConnection(get<0>(address), get<1>(address));
}


int TcpSocket::outgoingConnection(string host, string port){
	struct addrinfo hints;
	struct addrinfo * res = NULL;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int fail = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
	if (fail) {
		gai_strerror(fail);
		exit(1);
	}
	if (!res) exit(1);
	int socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (socket_fd == -1) {
		perror("socket");
		exit(1);
	}
	if (connect(socket_fd, res->ai_addr, res->ai_addrlen) == -1){
		perror("connect");
		exit(1);
	}
	freeaddrinfo(res);
	return socket_fd;
}

int TcpSocket::receivePutRequest(int fd, string target){
	int result = 0;
	int status = 0;
	bool cond = true;
	get<0>(dir->file_heartbeats[target])++;
	get<1>((dir->file_heartbeats[target])) = 1;
	pthread_mutex_lock(&dir_mutex);
	auto it = dir->file_status.find(target);
	if (it != dir->file_status.end()) result = it->second;
	cond = ((result == READLOCK) || (result == WRITELOCK));
	if (cond){
		pthread_mutex_unlock(&dir_mutex);
		while (cond){
			sleep(1);
			pthread_mutex_lock(&dir_mutex);
			auto it = dir->file_status.find(target);
			if (it != dir->file_status.end()) result = it->second;
			cond = (result == READLOCK) || (result == WRITELOCK);
			if (!cond) dir->file_status[target] = WRITELOCK;
			pthread_mutex_unlock(&dir_mutex);
		}
	}
	else{
		dir->file_status[target] = WRITELOCK;
		pthread_mutex_unlock(&dir_mutex);
	}
	status = receiveFile(fd, dir->get_path(target));
	pthread_mutex_lock(&dir_mutex);
	dir->file_status[target] = OPEN;
	pthread_mutex_unlock(&dir_mutex);
	if (status == -2) { cout << "MISSING FILE " << target << endl; fflush(stdout);}
	cout << " PUT RECEIVED FOR " << target << endl;
	return status;
}

int TcpSocket::receiveGetRequest(int fd, string target){
	char buffer[16]; buffer[0] = '\0';
	Messages msg;
	int result = 0;
	bool cond = true;
	pthread_mutex_lock(&dir_mutex);
	if (dir->file_status.count(target)) result = dir->file_status[target];
	else {
		pthread_mutex_unlock(&dir_mutex);
		sendMessage(fd, MISSING, buffer);
		return -1;
	}
	cond = (result == WRITELOCK);
	if (!cond) dir->file_status[target] = READLOCK;
	pthread_mutex_unlock(&dir_mutex);
	while (cond){
		sleep(1);
		pthread_mutex_lock(&dir_mutex);
		if (dir->file_status.count(target)) result = dir->file_status[target];
		else {
			pthread_mutex_unlock(&dir_mutex);
			sendMessage(fd, MISSING, buffer);
			return -1;
		}
		cond = (result == WRITELOCK);
		if (!cond) dir->file_status[target] = READLOCK;
		pthread_mutex_unlock(&dir_mutex);
	}
	cout << " GET RECEIVED FOR " << target << endl;
	return sendFile(fd, dir->get_path(target), NULL);
}

int TcpSocket::sendGetRequest(int fd, string filename, string local_file){
	int numBytes = 0;
	if ((numBytes = sendMessage(fd, FILEGET, filename.c_str()))) {
        perror("sendGet: send");
        return -1;
    }
	cout << " GET SENT FOR " << filename << endl;
	return receiveFile(fd, local_file);
}

int TcpSocket::sendPutRequest(int fd, string local, string target, int node_initiated){
	char * buffer = (char*)calloc(1,MAXBUFLEN+1);
	int fileBytes = 0;
	if (node_initiated) local = dir->get_path(local);
	cout << "SENDING PUT REQUEST " << endl;
	if (sendMessage(fd, FILEPUT, target.c_str())) return -1;
	cout << "WAITING FOR PUT ACK " << endl;
	if ((fileBytes = read(fd, buffer, MAXBUFLEN)) == -1){ perror("sendPutRequest: wait for put ack"); return -1; }
	buffer[fileBytes] = '\0';
	Messages msg = Messages(buffer);
	if (strcmp(msg.payload.c_str(), OK)){ perror("sendPutRequest: NON OK RESP"); free(buffer); return -2; }
	cout << "PUT ACK RECEIVED " << endl;
	if (sendFile(fd, local, target)) return -1;
	cout << "FILE RECEIVED" << endl;
	if ((fileBytes = read(fd, buffer, MAXBUFLEN)) == -1){ perror("sendPutRequest: wait for put ack2"); return -1; }
	buffer[fileBytes] = '\0';
	msg = Messages(buffer);
	if (strcmp(msg.payload.c_str(), OK)){ perror("sendPutRequest: NON OK RESP2"); free(buffer); return -2; }
	free(buffer);
	cout << " PUT SENT " << target << endl;
	return 0;
}

int TcpSocket::sendMessage(int fd, MessageType mt, const char * buffer){
    int numBytes = 0;
	string str(buffer);
    string msg = Messages(mt, str).toString();
    if ((numBytes = send(fd, msg.c_str(), msg.size(), 0)) == -1) {
		string s = "sendMessage: fail to send to fd " + to_string(fd) + " with type " + messageTypes[mt];
        perror(s.c_str());
        return -1;
    }
	pthread_mutex_lock(&traffic_mutex);
	byteSent += numBytes;
	pthread_mutex_unlock(&traffic_mutex);
	cout << numBytes << " were sent!" << endl;
    return 0;
}

int TcpSocket::sendOK(int fd){
    return sendMessage(fd, ACK, OK);
}

int TcpSocket::receiveMessage(int fd){
	char * buffer = (char*)malloc(MAXBUFLEN+1);
	int numBytes = 0;
	if ((numBytes = read(fd, buffer, MAXBUFLEN)) <= 0){
		string s = "receiveMessage: message not read from fd " + to_string(fd);
        perror(s.c_str());
		free(buffer);
        return -1;
    }
	buffer[numBytes] = '\0';
	string str(buffer);
    Messages msg = Messages(str);
	cout << " RECEIVED REQUEST " << messageTypes[msg.type] << endl;
	if (msg.type == FILEPUT){
		if (sendOK(fd) == 0) cout << "ACK PUT, READ TO RECEIVE" << endl;
		if (receivePutRequest(fd, msg.payload) == 0) cout << "PUT COMPLETE, SENDING OK" << endl; sendOK(fd);
	}
	else if (msg.type == FILEDEL){
		pthread_mutex_lock(&dir_mutex);
		dir->file_status.erase(dir->file_status.find(msg.payload));
		pthread_mutex_unlock(&dir_mutex);
		dir->remove_file(msg.payload);
		get<1>((dir->file_heartbeats[msg.payload])) = 0;
		sendOK(fd);
		cout << " DELETION COMPLETE " << msg.toString() << endl;
		fflush(stdout);
	}
	else if (msg.type == FILEGET){
		receiveGetRequest(fd, msg.payload);
	}
	free(buffer);
	return 0;
}

int TcpSocket::sendFile(int fd, string filename, string target){
	int bytes = 0;
	cout << "SENDING FILE " << filename << " to " << target << endl;
	FILE * fr = fopen(filename.c_str(), "r");
	target = (target.size()) ? target : OK;
	char * buffer = (char*) calloc(1, MAXBUFLEN+1);
	if (!fr){
		fprintf(stderr, "can't open file %s\n", filename.c_str());
		free(buffer);
		return -1;
	}
	while (!feof(fr)){
		size_t partialR = fread(buffer, 1, MAXBUFLEN, fr);
		if (!partialR) break;
		if (partialR == -1){
			perror("file read failed");
			free(buffer);
			return -1;
		}
		buffer[partialR] = '\0';
		if (sendMessage(fd, FILEDATA, buffer)) {
			perror("sendMessage: FILEDATA COULDNT BE SENT");
			free(buffer);
			return -1;
		}
		bytes += partialR;
	}
	free(buffer);
	if (sendMessage(fd, FILEEND, target.c_str())) {
		perror("write_server_put: send");
		return -1;
	}
	if (shutdown(serverSocket, SHUT_WR)) {perror("shutdown"); exit(1);}
	cout << to_string(bytes) << " bytes were sent from file " << filename;
	pthread_mutex_lock(&traffic_mutex);
	byteSent += bytes;
	pthread_mutex_unlock(&traffic_mutex);
	return 0;
}

int TcpSocket::receiveFile(int fd, string local_file){
	int totalNumBytes = 0;
	int numBytes = 0;
	srand (time(NULL));
  	int iSecret = rand() % 10000 + 1;
	string tempFile = local_file + to_string(iSecret);
	FILE * f = fopen(tempFile.c_str(), "w+");
	char * buffer = (char*)malloc(MAXBUFLEN+1);
	while (((numBytes = read(fd, buffer, MAXBUFLEN)) > 0)){
		totalNumBytes += numBytes;
		buffer[numBytes] = '\0';
		string str(buffer);
		Messages msg = Messages(str);
		if (msg.type == FILEEND){
			cout << "FILEEND WITH TOTAL " << to_string(totalNumBytes) << endl;
			fclose(f);
			remove(local_file.c_str());
			rename(tempFile.c_str(), local_file.c_str());
			free(buffer);
			cout << "directory now cotains: " << endl;
			dir->printer();
		}
		if (msg.type == MISSING){
			fclose(f);
			remove(tempFile.c_str());
			free(buffer);
			cout << "FILEMISSING " << local_file << endl;
			return -2;
		}
        if (dir->write_file(f, buffer + msg.fillerLength(), numBytes - msg.fillerLength())) break;
    }
	cout << "SOMETHING WRONG IN SENDING??" << endl;
	fclose(f);
	remove(tempFile.c_str());
	free(buffer);
	return -1;
}

void TcpSocket::cleanup() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
		endSession[MAX_CLIENTS - i] = 1;
        if (clients[i] != -1) closeFd(clients[i]);
    }
	endSession[0] = 1;
	closeFd(serverSocket);
}

void TcpSocket::setupServer() {
	struct addrinfo hints;
	struct addrinfo * res = NULL;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	int fail = 0;
	fail = getaddrinfo(NULL, serverPort, &hints, &res);
	if (fail) {
		gai_strerror(fail);
		exit(1);
	}
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		perror("socket");
		exit(1);
	}
	serverSocket = socket_fd;
    int boolVal = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT), &boolVal, sizeof(int)) == -1){
        perror("setsockopt"); exit(1);
    }
	if (::bind(socket_fd, res->ai_addr, res->ai_addrlen) == -1){ perror("bind"); exit(1); }
	if (listen(socket_fd, MAX_CLIENTS) == -1){ perror("listen"); exit(1); }
	freeaddrinfo(res);
}

void TcpSocket::runServer(){
    pthread_t tids[MAX_CLIENTS];
    while (endSession[MAX_CLIENTS] == 0){
    	int accept_fd = accept(serverSocket, NULL, NULL);
    	if (accept_fd == -1){
    		perror("accept");
    		exit(1);
    	}
    	pthread_mutex_lock(&clients_mutex);
    	if (clientsCount >= MAX_CLIENTS){
    		shutdown(accept_fd,SHUT_RDWR);
    		close(accept_fd);
    	}
    	else{
    		clientsCount++;
			int id = new_thread_id();
    		for (size_t i = 0; i < MAX_CLIENTS; i++){
    			if (clients[i] == -1) {
    				clients[i] = accept_fd;
					int result = 0;
					pthread_mutex_lock(&process_q_mutex);
					tuple<int, int> process = make_tuple(id, i);
					process_q.push(process);
					pthread_mutex_unlock(&process_q_mutex);
    				pthread_create(tids + i, NULL, processTcpRequests, (void *)this);
					cout << "CLIENT # " << to_string(clientsCount) << " assigned to index " << to_string(i) << endl;
					break;
    			}
    		}
    	}
    	pthread_mutex_unlock(&clients_mutex);
    }
}
