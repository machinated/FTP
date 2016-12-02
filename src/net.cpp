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

int openServerSocket(int port)
{
    int server_socket_descriptor;
    int bind_result;
    int listen_result;
    char reuse_addr_val = 1;
    struct sockaddr_in server_address;

    memset(&server_address, 0, sizeof(struct sockaddr));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    server_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_descriptor < 0)
    {
        throw SocketError("Błąd przy próbie utworzenia gniazda");
    }
    setsockopt(server_socket_descriptor, SOL_SOCKET, SO_REUSEADDR,
               (char*)&reuse_addr_val, sizeof(reuse_addr_val));

    bind_result = bind(server_socket_descriptor,
                       (struct sockaddr*)&server_address,
                       sizeof(struct sockaddr));
    if (bind_result < 0)
    {
        throw SocketError("Błąd przy próbie dowiązania adresu IP i numeru "
                          "portu do gniazda");
    }

    listen_result = listen(server_socket_descriptor, QUEUE_SIZE);
    if (listen_result < 0)
    {
        throw SocketError("Błąd przy próbie ustawienia wielkości kolejki");
    }

    return server_socket_descriptor;
}
