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
int TcpSocket::outgoingConnection(tuple<string,string> address){
	return outgoingConnection(get<0>(address), get<1>(address));
}

int TcpSocket::outgoingConnection(string host, string port){
	struct addrinfo hints;
	struct addrinfo * res = NULL;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	cout << "CONNECTING TO " << host.c_str() << "::" << port.c_str() << endl;
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
    Messages msg;
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
	cout << " PUT COMPLETE " << msg.toString() << endl;
	fflush(stdout);
	return status;
}

int TcpSocket::receiveGetRequest(int fd, string target){
	Messages msg;
	int result = 0;
	bool cond = true;
	pthread_mutex_lock(&dir_mutex);
	if (dir->file_status.count(target)) result = dir->file_status[target];
	else {
		pthread_mutex_unlock(&dir_mutex);
		sendMessage(fd, MISSING, "");
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
			sendMessage(fd, MISSING, "");
			return -1;
		}
		cond = (result == WRITELOCK);
		if (!cond) dir->file_status[target] = READLOCK;
		pthread_mutex_unlock(&dir_mutex);
	}
	return sendFile(fd, dir->get_path(target), NULL);
}

int TcpSocket::sendGetRequest(int fd, string filename, string local_file){
	int numBytes = 0;
	if ((numBytes = sendMessage(fd, FILEGET, filename.c_str()))) {
        perror("sendGet: send");
        return -1;
    }
	return receiveFile(fd, local_file);
}

int TcpSocket::sendPutRequest(int fd, string local, string target, int node_initiated){
	char * buffer = (char*)calloc(1,MAXBUFLEN);
	int fileBytes = 0;
	if (node_initiated) local = dir->get_path(local);
	if (sendFile(fd, local, target)) return -1;;
	if ((fileBytes = recv(fd, buffer, MAXBUFLEN, 0)) == -1){
		perror("write_server_put: recv");
		return -1;
	}
	buffer[fileBytes] = '\0';
	Messages msg = Messages(buffer);
	if (strcmp(msg.payload.c_str(), OK)){
		perror("write_server_put: recv");
		free(buffer);
		return -2;
	}
	free(buffer);
	return 0;
}

int TcpSocket::sendMessage(int fd, MessageType mt, const char * buffer){
    int numBytes = 0;
    Messages msg = Messages(mt, buffer).toString();
    if ((numBytes = send(fd, msg.toString().c_str(), msg.toString().size(), 0)) == -1) {
        perror("sendOK: send");
        return -1;
    }
	pthread_mutex_lock(&gen_mutex);
	byteSent += numBytes;
	pthread_mutex_unlock(&gen_mutex);
    return 0;
}

int TcpSocket::sendOK(int fd){
    return sendMessage(fd, ACK, OK);
}

int TcpSocket::receiveMessage(int fd){
	char * buffer = (char*)calloc(1,MAXBUFLEN);
	if (recv(fd, buffer, MAXBUFLEN, 0) == -1){
        perror("receiveMessage: recv");
		free(buffer);
        return -1;
    }
    Messages msg = Messages(buffer);
	cout << " RECEIVED REQUEST " << msg.toString() << endl;
	fflush(stdout);
	if (msg.type == FILEDATA){
		if (receivePutRequest(fd, msg.payload) == 0) sendOK(fd);
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
	FILE * fr = fopen(filename.c_str(), "r");
	target = (target.size()) ? target : OK;
	char * buffer = (char*) calloc(1, MAXBUFLEN);
	if (!fr){
		fprintf(stderr, "can't open file %s\n", filename.c_str());
		free(buffer);
		return -1;
	}
	while (!feof(fr)){
		size_t partialR = fread(buffer, 1, MAXBUFLEN, fr);
		if (!partialR) break;
		if (sendMessage(fd, FILEDATA, buffer)) {
			perror("write_server_put: send");
			free(buffer);
			return -1;
		}
		pthread_mutex_lock(&gen_mutex);
		byteSent += partialR;
		pthread_mutex_unlock(&gen_mutex);
	}
	free(buffer);
	if (sendMessage(fd, FILEEND, target.c_str())) {
		perror("write_server_put: send");
		return -1;
	}
	if (shutdown(serverSocket, SHUT_WR)) {perror("shutdown"); exit(1);}
	cout << " GET COMPLETE " << target << endl;
	fflush(stdout);
	return 0;
}

int TcpSocket::receiveFile(int fd, string local_file){
	int numBytes = 0;
	string tmp = tmpnam (NULL);
	FILE * f = fopen(tmp.c_str(), "w+");
	char * buffer = (char*)calloc(1,MAXBUFLEN);
	while (((numBytes = recv(fd, buffer, MAXBUFLEN, 0)) != -1)){
		Messages msg = Messages(buffer);
		if (msg.type == FILEEND){
			local_file = (local_file.size()) ? local_file : msg.payload;
			fclose(f);
			remove(local_file.c_str());
			rename(tmp.c_str(), local_file.c_str());
			free(buffer);
			return 0;
		}
		if (msg.type == MISSING){
			fclose(f);
			remove(local_file.c_str());
			free(buffer);
			return -2;
		}
        if (dir->write_file(f, buffer + msg.fillerLength(), numBytes - msg.fillerLength())) break;
    }
	fclose(f);
	remove(tmp.c_str());
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
    		size_t j;
    		for (size_t i = 0; i < MAX_CLIENTS; i++){
    			if (clients[i] == -1) {
    				clients[i] = accept_fd;
    				j = i;
					int result = 0;
					pthread_mutex_lock(&id_mutex);
    				result = pthread_create(tids + j, NULL, processTcpRequests, (void *)this);
					if (!result) { thread_to_ind[tids[j]] = j; }
					pthread_mutex_unlock(&id_mutex);
					if (result) break;
    			}
    		}
    	}
    	pthread_mutex_unlock(&clients_mutex);
    }
}
