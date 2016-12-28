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
#include <getopt.h>
#include <string>
#include <iostream>

#include "net.h"
#include "manager.h"


#define QUEUE_SIZE 5
int serverSocket = -1;
struct options_t options;

using namespace std;

const char* helpMsg =
"Usage: xmftp [OPTIONS]"                                                    "\n"
"   -p PORT, --port         specify port number, default is 21"             "\n"
"   -l, --local             accept only connections from localhost"         "\n"
"   -g, --sendGA            send Go Ahead commands via Telnet, defult=no"   "\n"
"   -r, --readonly          make all sessions read only"                    "\n"
"   -h, --help              show this message"                              "\n";

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

int parseOptions(int argc, char *argv[])
{
    static struct option long_options[] =
    {
        {"port",        required_argument,  0, 'p'},
        {"local",       no_argument,        0, 'l'},
        {"sendga",      no_argument,        0, 'g'},
        {"readonly",    no_argument,        0, 'r'},
        {"help",        no_argument,        0, 'h'},
        {0,             0,                  0,  0 }
    };
    int c;

    while (1)
    {
        int option_index;
        c = getopt_long(argc, argv, "p:lgrh", long_options, &option_index);
        if (c == -1)
            break;

        switch(c)
        {
        case 'p':
        {
            unsigned int newport = (unsigned int) atoi(optarg);
            if (newport == 21 || (newport > 1024 && newport < 256*256))
                options.port = newport;
            else
                cerr << "Invalid port: " << optarg << "\n";
            break;
        }

        case 'l':
            options.local = true;
            break;

        case 'g':
            options.supressGA = false;
            break;

        case 'r':
            break;

        case 'h':
            cout << helpMsg;
            return 1;

        case '?':
            return 1;

        default:
            cerr << "Getopt returned character code " << c << "\n";
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    // int atexitResult = atexit(cleanup);
    // if (atexitResult)
    // {
    //     cerr << "Failed to register 'atexit' function.\n";
    //     exit(1);
    // }

    sighandler_t signalResult = signal(SIGINT, &signalHandler);
    if (signalResult == SIG_ERR)
    {
        cerr << "Failed to register signal handler.\n";
        exit(1);
    }

    options.port = PORT_L;
    options.supressGA = true;
    options.local = false;

    if (parseOptions(argc, argv))
    {
        exit(1);
    }

    // chroot jail
    if (chdir("ftproot"))
    {
        cerr << "Failed to change to ftproot directory\n";
        exit(1);
    }

    char* dirname = getcwd(NULL, 0);
    if (dirname == NULL)
    {
        exit(1);
    }
    if (chroot(dirname))
    {
        cerr << "Failed to create chroot jail\n";
        free(dirname);
        exit(1);
    }
    free(dirname);

    try
    {
        serverSocket = openServerSocket(options.port);
    }
    catch (SocketError& e)
    {
        cerr << "Network error " << e.code() << e.what() << "\n";
        exit(1);
    }
    cout << "Service started.\n";

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
