#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <cassert>
#include <system_error>
#include <exception>
#include <string.h>
#include "data.h"
#include "net.h"


#define RW_BUFFER 1024
using namespace std;

DataConnection::DataConnection(bool passive, struct sockaddr* addr, int port)
{
    if (this->passive)    // wait for connection
    {
        this->serverSocket = openServerSocket(port);
        this->connDesc = accept(serverSocket, NULL, NULL);
        if (this->connDesc < 0)
        {
            throw SocketError("System error while accepting connection");
        }
    }
    else            // connect to client
    {
        this->connDesc = socket(PF_INET, SOCK_STREAM, 0);
        if (this->connDesc < 0)
        {
            cerr << "Can't create a socket.\n";
            exit(1);
        }
        int connectRes;
        connectRes = connect(this->connDesc, (struct sockaddr*)addr,
                             sizeof(struct sockaddr));
        if (connectRes < 0)
        {
            throw SocketError("Cannot establish connection");
        }
    }

}

DataConnection::~DataConnection()
{
    close(this->connDesc);
    if (this->passive)
        close(this->serverSocket);
}

void DataConnection::Receive(int fileDesc)
{
    int nBytesR, nBytesW, writeRes;
    char buffer[RW_BUFFER];

    while (1)
    {
        nBytesR = read(connDesc, buffer, RW_BUFFER);
        if (nBytesR == -1)
            throw SocketError("Error receiving data");
        if (nBytesR == 0)
            break;
        nBytesW = 0;
        while (nBytesW < nBytesR)
        {
            writeRes = write(fileDesc, buffer, nBytesR);
            if (writeRes == -1)
                throw system_error(errno, system_category(),
                                   "Error while writing file");
            nBytesW += writeRes;
        }
    }
}

void DataConnection::Send(int fileDesc)
{
    int nBytesR, nBytesW, writeRes;
    char buffer[RW_BUFFER];

    while (1)
    {
        nBytesR = read(fileDesc, buffer, RW_BUFFER);
        if (nBytesR == -1)
            throw system_error(errno, system_category(),
                               "Error while reading file");
        if (nBytesR == 0)
            break;
        nBytesW = 0;
        while (nBytesW < nBytesR)
        {
            writeRes = write(connDesc, buffer, nBytesR);
            if (writeRes == -1)
                throw SocketError("Error sending data");
            nBytesW += writeRes;
        }
    }
}
