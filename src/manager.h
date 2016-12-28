#ifndef MANAGER_H
#define MANAGER_H
#include <string>
#include <pthread.h>
#include <list>
#include "telnet.h"
#include "data.h"

void* runCC(void* CC_pointer);

class ControlConnection
{
    std::list<ControlConnection*>::iterator listIterator;
    int socket;
    Telnet telnet;
    DataConnection dataConnection;
    dataConnSettings settings;
    std::string peerAddrStr;
    std::string user;
    bool run;
public:
    pthread_t thread;
    static std::list<ControlConnection*> List;
    static pthread_mutex_t listMutex;

    ControlConnection(int socketDescriptor);
    void Run();
    void sendResponse(const char response[]);

    void CmdUser(std::string* args);
    void CmdCwd(std::string* args);
    void CmdCdup(std::string* args);
    void CmdQuit(std::string* args);
    void CmdPort(std::string* args);
    void CmdPasv(std::string* args);
    void CmdType(std::string* args);
    void CmdMode(std::string* args);
    void CmdStru(std::string* args);
    void CmdRetr(std::string* args);
    void CmdStor(std::string* args);
    void CmdAbor(std::string* args);
    void CmdDele(std::string* args);
    void CmdMkd(std::string* args);
    void CmdRmd(std::string* args);
    void CmdNlst(std::string* args);
    void CmdNoop(std::string* args);
    void CmdNotImplemented(std::string* args);

    // typedef void (*CmdHandlerP)(string* args);

    ~ControlConnection();
};

typedef void (ControlConnection::*CmdHandlerP)(std::string* args);

#endif
