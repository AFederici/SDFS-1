#ifndef MESSAGES_H
#define MESSAGES_H

#include <iostream>
#include <string>
#include "MessageTypes.h"
using namespace std;
using std::string;
using std::to_string;

class Messages {
public:
	MessageType type;
	string payload;
	Messages(string payloadMessage); //split message into type and payload, delimeted by ::
	Messages(MessageType messageType, string payloadMessage);
	Messages();
	string toString();
	int fillerLength();
};

#endif //MESSAGES_H
