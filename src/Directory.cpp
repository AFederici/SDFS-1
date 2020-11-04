#include "../inc/Directory.h"

Directory::Directory(){
    if (mkdir("sdfs", 0777) == -1) { perror("bad dir"); exit(1);}
}

void Directory::store(){
    cout << "[STORE] " << endl;
    for(auto& element: file_heartbeats){
        if (get<1>(element.second) == 0) continue;
        cout << "FILE - " << element.first.c_str() << endl;
    }
}

int Directory::write_file(FILE * f, char * buf, uint size){
	size_t written = 0;
	while (written < size){
		int actuallyWritten = fwrite(buf + written, 1, size - written, f);
		if (actuallyWritten <= 0){
			fprintf(stderr, "coudln't write to local file\n");
        	return -1;
		}
		else written += actuallyWritten;
	}
    pthread_mutex_lock(&directory_mutex);
    size += written;
    pthread_mutex_unlock(&directory_mutex);
    return 0;
}

string Directory::get_path(string filename){
    string s = "sdfs/" + filename;
    return s;
}

void Directory::remove_file(string filename){
    FILE *p_file = fopen(get_path(filename).c_str(),"rb");
    fseek(p_file,0,SEEK_END);
    int to_remove = ftell(p_file);
    fclose(p_file);
    remove(get_path(filename).c_str());
    pthread_mutex_lock(&directory_mutex);
    size += to_remove;
    pthread_mutex_unlock(&directory_mutex);
}

void Directory::clear(){
    remove(dir);
}
