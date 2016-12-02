#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <string>
#include <iostream>

#include "net.h"
#include "manager.h"


#define QUEUE_SIZE 5


int main(int argc, char const *argv[])
{
    int server_socket_descriptor;
    int connection_socket_descriptor;

    if (argc != 2)
    {
        cerr << "Podaj dokładnie jeden argument: numer portu.\n";
        exit(1);
    }
    int port = atoi(argv[1]);

    try
    {
        server_socket_descriptor = openServerSocket(port);
    }
    catch (SocketError& e)
    {
        cerr << "Network error " << e.code() << e.what() << "\n";
        exit(1);
    }
    connection_socket_descriptor = accept(server_socket_descriptor, NULL, NULL);
    if (connection_socket_descriptor < 0)
    {
        cerr << argv[0];
        cerr << " Błąd przy próbie utworzenia gniazda dla połączenia.\n";
        exit(1);
    }

    // 'real' stuff
    ControlConnection cc(connection_socket_descriptor);

    try
    {
        cc.Run();
    }
    catch (SocketClosedError& e)
    {
        cout << "Socket closed by other party\n";
    }
    catch (SocketError& e)
    {
        cerr << "Network error " << e.code() << e.what() << "\n";
    }


    int closeResult;
    closeResult = close(connection_socket_descriptor);
    if (closeResult)
    {
        cerr << "Błąd podczas zamykania połączenia\n";
    }
    closeResult = close(server_socket_descriptor);
    if (closeResult)
    {
        cerr << "Błąd podczas zamykania gniazda\n";
    }
    return 0;
}
