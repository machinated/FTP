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
    char reuse_addr_val = 1;

    serverDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (serverDesc < 0)
    {
        throw SocketError("Failed to create network socket");
    }
    setsockopt(serverDesc, SOL_SOCKET, SO_REUSEADDR,
               (char*)&reuse_addr_val, sizeof(reuse_addr_val));

    setsockopt(serverDesc, SOL_SOCKET, SO_REUSEPORT,
               (char*)&reuse_addr_val, sizeof(reuse_addr_val));

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
