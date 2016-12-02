#ifndef DATA_H
#define DATA_H

#include <sys/types.h>
#include <sys/socket.h>

class DataConnection
{
    bool passive;
    bool ascii;
    int serverSocket;
    int connDesc;
public:
    DataConnection(bool passive, struct sockaddr* addr, int port);
    ~DataConnection();
    void Receive(int fileDesc);
    void Send(int fileDesc);
};

#endif
