#ifndef TELNET_H
#define TELNET_H
#include <string>
#include <pthread.h>
using namespace std;

class Telnet
{
    int socketDescriptor;
    pthread_mutex_t writeMutex;
    int respond(unsigned char command, unsigned char option);
    int sendGA();
public:
    int error;
    Telnet(int socket);
    int readLine(string* line);
    int writeLine(string* line);
};


#endif
