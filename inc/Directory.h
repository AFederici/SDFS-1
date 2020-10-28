#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <utility>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "Modes.h"
#ifdef __linux__
#include <bits/stdc++.h>
#endif

using std::string;
using std::get;
using std::map;
using std::vector;
using std::cout;
using std::endl;
using std::tuple;
using std::ostringstream;

static pthread_mutex_t directory_mutex = PTHREAD_MUTEX_INITIALIZER;

class Directory{
public:
	char * dir; //need to handle sigkill and remove this when node fails
    map<string,tuple<int,int>> file_heartbeats; //file -> (hearbeat,alive or dead)
    map<string,int> file_status; //file -> operation status
	int size;
    Directory();
    void store();
	int write_file(FILE * f, char * buf, uint size);
	void remove_file(string filename);
    string get_path(string filename);
	void clear();
};

#endif //DIRECTORY_H
