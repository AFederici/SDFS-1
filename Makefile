GCC := g++
RM := rm
APP := Node
INC_DIR := ./inc
CFLAGS := -g -w -Wall -std=c++11 -I$(INC_DIR)
LIBS := -lpthread
SRC_FILES := src/Node.cpp src/Messages.cpp src/Member.cpp src/UdpSocket.cpp src/Threads.cpp src/Sdfs.cpp src/Logger.cpp src/TcpSocket.cpp src/Utils.cpp src/Directory.cpp

.PHONY: clean

all: clean app

app:
	$(GCC) -o $(APP) $(SRC_FILES) $(CFLAGS) $(LIBS)

clean:
	$(RM) -f $(APP) *.o
