#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <cassert>
#include <system_error>
#include <exception>
#include <string.h>
#include <signal.h>
#include "data.h"
#include "net.h"


#define RW_BUFFER 1024
using namespace std;

void* store(void* DC_pointer)
{
    DataConnection* dc = (DataConnection*) DC_pointer;
    try
    {
        dc->Store();
    }
    catch (exception &e)
    {
        dc->HandleException(e);
    }
    return nullptr;
}

void* retrieve(void* DC_pointer)
{
    DataConnection* dc = (DataConnection*) DC_pointer;
    try
    {
        dc->Retrieve();
    }
    catch (exception &e)
    {
        dc->HandleException(e);
    }
    return nullptr;
}

void DataConnection::HandleException(exception& e)
{
    cerr << "Exception in data transfer thread: " << e.what() << "\n";
    excP = current_exception();
    try
    {
        sendResponse(
            "451 Requested action aborted: "
            "local error in processing.");
    }
    catch (...)
    {
        cerr << "Failed to send negative response after exception\n";
    }
    if (pthread_kill(parent, SIGTERM))
    {
        cerr << "Failed to send termination signal to parent thread\n";
    }
}

void DataConnection::sendResponse(const char response[])
{
    telnet->writeLine(response);
    #ifdef DEBUG
        cout << response << '\n';
    #endif
}

DataConnection::DataConnection(pthread_t parent, Telnet* telnet)
: telnet(telnet), connDesc(0), fileDesc(0), abort(false), serverSocket(0),
  active(false), parent(parent)
{
    settings.mode = 0;
    #define SETTINGS_SET settings.mode
}

void DataConnection::Open()
{
    if (settings.passive && serverSocket == 0)
    {
        serverSocket = openServerSocket(&(settings.addrLocal));
    }
}

void DataConnection::Connect()
{
    assert(connDesc == 0);
    assert(SETTINGS_SET);

    if (settings.passive)    // wait for connection
    {
        Open();
        connDesc = accept(serverSocket, NULL, NULL);
        if (connDesc < 0)
        {
            throw SocketError("System error while accepting connection");
        }
    }
    else                    // connect to client
    {
        connDesc = socket(PF_INET, SOCK_STREAM, 0);
        if (connDesc < 0)
        {
            throw SocketError("Failed to create socket for data connection");
        }

        int reuse_addr_val = 1;
        setsockopt(connDesc, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_addr_val, sizeof(reuse_addr_val));

        setsockopt(connDesc, SOL_SOCKET, SO_REUSEPORT,
                   &reuse_addr_val, sizeof(reuse_addr_val));

        int bindResult = bind(connDesc,
                              (struct sockaddr*) &(settings.addrLocal),
                              sizeof(struct sockaddr));
        if (bindResult)
        {
            throw SocketError("Failed to bind port number to local socket");
        }

        int connectRes;
        connectRes = connect(connDesc, (struct sockaddr*) &(settings.addrRemote),
                             sizeof(struct sockaddr));
        if (connectRes < 0)
        {
            if (errno == ECONNREFUSED)
            {
                sendResponse("425 Cannot open data connection: "
                             "connection refused");
            }
            else
            {
                throw SocketError(
                    "Couldn't connect to client for data transfer");
            }
        }
    }
}

void DataConnection::Close()
{
    assert(isConnected());
    close(connDesc);
    connDesc = 0;

    if (settings.passive)
    {
        assert(serverSocket > 0);
        close(serverSocket);
        serverSocket = 0;
    }
}

void DataConnection::SetSettings(dataConnSettings* conn_settings)
{
    if (connDesc > 0)
        Close();
    memcpy(&(settings), conn_settings, sizeof(settings));
}

void DataConnection::SetFile(int fdesc)
{
    assert(!active);
    fileDesc = fdesc;
}

DataConnection::~DataConnection()
{
    close(connDesc);
    if (settings.passive)
        close(serverSocket);
}

void DataConnection::Abort()
{
    abort = true;
}

void DataConnection::Store()
{
    if (isConnected())
    {
        sendResponse(
            "125 Data connection already open; "
            "transfer starting");
    }
    else
    {
        sendResponse(
            "150  File status okay; "
            "about to open data connection.");
        Connect();
    }

    int nBytesR, nBytesW, writeRes;
    char buffer[RW_BUFFER];
    bool cr = false;    // for ASCII type
    char c;

    while (true)
    {
        if (abort)
        {
            sendResponse(
                "426 Transfer aborted."
            );
            abort = false;
            Close();
            break;
        }
        if (settings.ascii)
        {
            nBytesR = read(connDesc, &c, 1);
        }
        else
        {
            nBytesR = read(connDesc, buffer, RW_BUFFER);
        }
        if (nBytesR == -1)
        {
            throw SocketError("Error receiving data");
        }
        if (nBytesR == 0)
        {
            sendResponse(
                "226 Closing data connection. "
                "File transfer successful.");
            Close();
            break;
        }
        if (settings.ascii)
        {
            if (cr && c != '\n')
            {
                buffer[1] = c;
                buffer[0] = '\r';
                nBytesR = 2;        // write 2 bytes: '\r' and newly receided
                                    // byte
            }
            else
            {
                buffer[0] = c;
            }
            if (c == '\r')
            {
                cr = true;
                nBytesR = 0;        // don't write anything yet
            }
            else
            {
                cr = false;
            }
        }
        nBytesW = 0;
        while (nBytesW < nBytesR)
        {
            writeRes = write(fileDesc, buffer + nBytesW, nBytesR);
            if (writeRes == -1)
            {
                throw SystemError("Error while writing file");
            }
            nBytesW += writeRes;
        }
    }
    active = false;
}

void DataConnection::Retrieve()
{
    if (isConnected())
    {
        sendResponse(
            "125 Data connection already open; "
            "transfer starting");
    }
    else
    {
        sendResponse(
            "150  File status okay; "
            "about to open data connection.");
        Connect();
    }

    int nBytesR, nBytesW, writeRes;
    char buffer[RW_BUFFER];

    while (true)
    {
        if (abort)
        {
            sendResponse("426 Transfer aborted.");
            abort = false;
            Close();
            break;
        }
        if (settings.ascii)
        {
            nBytesR = read(fileDesc, buffer, 1);
        }
        else
        {
            nBytesR = read(fileDesc, buffer, RW_BUFFER);
        }
        if (nBytesR == -1)
        {
            throw SystemError("Error while reading file");
        }
        if (nBytesR == 0)
        {
            sendResponse(
                "226 Closing data connection. "
                "File transfer successful.");
            Close();
            break;
        }
        if (settings.ascii)
        {
            if (buffer[0] == '\n')
            {
                buffer[0] = '\r';
                buffer[1] = '\n';
                nBytesR = 2;
            }
        }
        nBytesW = 0;
        while (nBytesW < nBytesR)
        {
            writeRes = write(connDesc, buffer + nBytesW, nBytesR);
            if (writeRes == -1)
            {
                if (errno == EPIPE)
                {
                    sendResponse("426 Connection closed; transfer aborted.");
                    Close();
                    break;
                }
                else
                {
                    throw SocketError("Error sending data");
                }
            }
            nBytesW += writeRes;
        }
    }
    active = false;
}
