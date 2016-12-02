#ifndef MANAGER_H
#define MANAGER_H
#include <string>
#include <pthread.h>
#include "telnet.h"

class ControlConnection
{
    int socket;
    pthread_t thread;
    Telnet telnet;
    string username;
public:
    ControlConnection(int socketDescriptor);
    void* Run(void* a);
    ~ControlConnection();
};

#endif
