#ifndef DATA_H
#define DATA_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include "telnet.h"


typedef struct dataConnSettings
{
    bool passive;
    bool ascii;
    int mode;
    #define MODE_STREAM 4
    #define MODE_BLOCK 8
    int structure;
    #define STRU_FILE 15
    #define STRU_REC 16
    struct sockaddr_in addrLocal;
    struct sockaddr_in addrRemote;
} dataConnSettings;

void* store(void* DC_pointer);
void* retrieve(void* DC_pointer);

class DataConnection
{
    dataConnSettings settings;
    Telnet* telnet;
    int connDesc;
    int fileDesc;
    bool abort;

public:
    DataConnection(pthread_t parent, Telnet* telnet);
    ~DataConnection();
    void sendResponse(const char response[]);
    void SetSettings(dataConnSettings* conn_settings);
    void SetFile(int fdesc);
    void Open();
    void Connect();
    void Close();
    void Store();
    void Retrieve();
    void Abort();

    bool isConnected()
    {
        return connDesc > 0;
    }

    int serverSocket;
    bool active;
    pthread_t thread;
    pthread_t parent;
    std::exception_ptr excP;
};

#endif
