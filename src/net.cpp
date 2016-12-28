#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"

#define QUEUE_SIZE 5

typedef struct sockaddr_in* sockaddrp;
int openServerSocket(sockaddrp serverAddress)
{
    int serverDesc;
    int bindResult;
    int listenResult;
    int reuse_addr_val = 1;

    serverDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (serverDesc < 0)
    {
        throw SocketError("Failed to create network socket");
    }
    setsockopt(serverDesc, SOL_SOCKET, SO_REUSEADDR,
               &reuse_addr_val, sizeof(reuse_addr_val));

    setsockopt(serverDesc, SOL_SOCKET, SO_REUSEPORT,
               &reuse_addr_val, sizeof(reuse_addr_val));

    bindResult = bind(serverDesc,
                       (struct sockaddr*)serverAddress,
                       sizeof(struct sockaddr));
    if (bindResult < 0)
    {
        throw SocketError("Failed to bind IP address and port number "
                          "to network socket");
    }

    listenResult = listen(serverDesc, QUEUE_SIZE);
    if (listenResult < 0)
    {
        throw SocketError("Failed to set queue size");
    }

    return serverDesc;
}

int openServerSocket(int port)
{
    struct sockaddr_in serverAddress;

    memset(&serverAddress, 0, sizeof(struct sockaddr));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(port);

    return openServerSocket(&serverAddress);
}

MutexPipe::MutexPipe()
{
    #define PIPE_BUF_S 100
    int pipefd[2];
    int pipeResult = pipe(pipefd);
    if (pipeResult == -1)
    {
        throw SystemError("Error creating pipe");
    }
    descR = pipefd[0];
    descW = pipefd[1];
}

void MutexPipe::readMutex(std::string* line)
{
    int nBytes = PIPE_BUF_S;
    char buffer[PIPE_BUF_S];
    line->clear();

    pthread_mutex_lock(&pipeLock);
    while (nBytes == PIPE_BUF_S)
    {
        nBytes = read(descR, buffer, PIPE_BUF_S);
        if (nBytes == -1)
        {
            throw SystemError("Error reading from pipe");
        }
        line->append(buffer, nBytes);
    }
    pthread_mutex_unlock(&pipeLock);
}

void MutexPipe::writeMutex(const char line[], size_t len)
{
    unsigned int nBytesW = 0;
    int nBytes;

    pthread_mutex_lock(&pipeLock);
    while (nBytesW < len)
    {
        nBytes = write(descW, &line[nBytesW], len - nBytesW);
        if (nBytes == -1)
        {
            throw SystemError("Error writing to pipe");
        }
        else if (nBytes == 0)
        {
            throw PipeError("Cannot write more data to pipe");
        }
        nBytesW += nBytes;
    }
    pthread_mutex_unlock(&pipeLock);
}

void MutexPipe::writeMutex(std::string* line)
{
    writeMutex(line->data(), line->length());
}
