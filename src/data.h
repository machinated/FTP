#ifndef DATA_H
#define DATA_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>

#include "telnet.h"
#include "net.h"


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
void* nlist(void* DC_pointer);

class DataConnection
{
    dataConnSettings settings;
    Telnet* telnet;
    int connDesc;
    int fileDesc;
    DIR* dirDesc;
    bool abort;
    void SetFile(int fdesc);
    void SetDir(DIR* dDesc);

public:
    DataConnection(Telnet* telnet);
    ~DataConnection();
    void sendResponse(const char response[]);
    void SetSettings(dataConnSettings* conn_settings);
    void Open();
    void Connect();
    void Close();
    void Abort();
    void Store();
    void Retrieve();
    void Nlist();
    void HandleException(std::system_error& e);
    void ThreadStore(int fdesc);
    void ThreadRetrieve(int fdesc);
    void ThreadNlist(DIR* dDesc);

    bool isConnected()
    {
        return connDesc > 0;
    }

    int serverSocket;
    bool active;
    pthread_t thread;
    std::exception_ptr excP;
    MutexPipe pipe;
};

#endif
