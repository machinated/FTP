#ifndef MANAGER_H
#define MANAGER_H
#include <string>
#include <pthread.h>
#include <list>
#include "telnet.h"

void* run(void* CC_pointer);

class ControlConnection
{
    std::list<ControlConnection*>::iterator listIterator;
    int socket;
    Telnet telnet;
    string username;
public:
    pthread_t thread;
    static std::list<ControlConnection*> List;
    static pthread_mutex_t listMutex;

    ControlConnection(int socketDescriptor);
    void Run();
    ~ControlConnection();
};

#endif
