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
int serverSocket = -1;

void clean()
{
    for (list<ControlConnection*>::iterator
         iter = ControlConnection::List.begin();
         iter != ControlConnection::List.end(); iter++)
    {
        delete *iter;
    }

    if (serverSocket != -1)
    {
        int closeResult = close(serverSocket);
        if (closeResult)
        {
            cerr << "Błąd podczas zamykania gniazda\n";
        }
    }
}

int main(int argc, char const *argv[])
{
    int atexitResult = atexit(clean);
    if (atexitResult)
    {
        cerr << "Błąd rejestracji procedury atexit.\n";
        exit(1);
    }

    if (argc != 2)
    {
        cerr << "Podaj dokładnie jeden argument: numer portu.\n";
        exit(1);
    }
    int port = atoi(argv[1]);

    try
    {
        serverSocket = openServerSocket(port);
    }
    catch (SocketError& e)
    {
        cerr << "Network error " << e.code() << e.what() << "\n";
        exit(1);
    }

    while(true)
    {
        int connectionDesc = accept(serverSocket, NULL, NULL);
        if (connectionDesc < 0)
        {
            cerr << " Błąd przy próbie utworzenia gniazda dla połączenia.\n";
            exit(1);
        }

        ControlConnection* cc = new ControlConnection(connectionDesc);

        int createResult;
        createResult = pthread_create(&(cc->thread), NULL, &run, (void*)cc);
        if (createResult)
        {
            cerr << "Błąd przy próbie utworzenia wątku.\n";
            exit(1);
        }
    }
}
