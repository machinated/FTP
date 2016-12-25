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
#include <signal.h>
#include <string>
#include <iostream>

#include "net.h"
#include "manager.h"


#define QUEUE_SIZE 5
int serverSocket = -1;
struct options_t options;

using namespace std;

void cleanup()
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
            cerr << "Failed to close server socket.\n";
        }
    }
}

void signalHandler(int)
{
    exit(0);
}

int main()          //(int argc, char const *argv[])
{
    int atexitResult = atexit(cleanup);
    if (atexitResult)
    {
        cerr << "Failed to register 'atexit' function.\n";
        exit(1);
    }

    sighandler_t signalResult = signal(SIGINT, &signalHandler);
    if (signalResult == SIG_ERR)
    {
        cerr << "Failed to register signal handler.\n";
        exit(1);
    }

    // char* endPointer;
    // int port = strtol(argv[1], &endPointer, 10);
    // if (*endPointer != '\0')    // non-digit characters in argument
    // {
    //     cerr << "Cannot interpret " << argv[1] << " as port number.\n";
    //     cerr << helpMsg;
    //     exit(1);
    // }

    options.port = PORT_L;
    options.supressGA = true;
    options.local = true;

    try
    {
        serverSocket = openServerSocket(options.port);
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
            cerr << "Fatal: cannot create connection socket\n";
            exit(1);
        }

        ControlConnection* cc = new ControlConnection(connectionDesc);

        int createResult;
        createResult = pthread_create(&(cc->thread), NULL, &runCC, (void*)cc);
        if (createResult)
        {
            cerr << "Failed to start new thread.\n";
            exit(1);
        }
    }
}
