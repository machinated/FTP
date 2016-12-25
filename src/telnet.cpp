#include <unistd.h>
#include <stdio.h>
#include <sys/select.h>
#include <iostream>
#include <cassert>
#include <string.h>
#include "telnet.h"
#include "net.h"

#define AYT 246
#define GA 249
#define WILL 251
#define WONT 252
#define DO 253
#define DONT 254
#define IAC 255

#define ERR_SOCK_CLOSED 1
#define ERR_IO 2
#define ERR_COMMAND 3

using namespace std;


Telnet::Telnet(int socket)
: socketDescriptor(socket)
{
    writeMutex = PTHREAD_MUTEX_INITIALIZER;
}


void Telnet::sendGA()
{
    char msg[2];
    msg[0] = IAC;
    msg[1] = GA;

    pthread_mutex_lock(&writeMutex);
    int nBytes = write(socketDescriptor, msg, 2);
    pthread_mutex_unlock(&writeMutex);

    if (nBytes < 2)
    {
        if (nBytes == 0)
            throw SocketClosedError();
        if (nBytes == -1)
            throw SocketError("Error while sending GA");
        // TODO if(nBytes == 1) -- unlikely
    }
}


void Telnet::respond(unsigned char command, unsigned char option)
{
    char response[3] = "\x00\x00";
    int resLen = 3;
    response[0] = IAC;

    switch (command)
    {
        case AYT:
        {
            string resp("eftp telenet");
            writeLine(&resp);
            return;
        }
        case WILL:
        case WONT:
        {
            response[1] = DONT;
            response[2] = option;
            break;
        }
        case DO:
        case DONT:
        {
            response[1] = WONT;
            response[2] = option;
            break;
        }
        default:
            assert(false);
    }

    pthread_mutex_lock(&writeMutex);
    int nBytes = write(socketDescriptor, response, resLen);
    pthread_mutex_unlock(&writeMutex);

    if (nBytes < resLen)
    {
        if (nBytes == 0)
            throw SocketClosedError();
        if (nBytes == -1)
            throw SocketError("Error while sending Telnet response");
    }
}


void Telnet::readLine(string* line)
// read exactly one line
{
    line->clear();
    unsigned char newChar;
    bool interpretCommand = false;
    bool interpretOption = false;
    bool subnegotiation = false;
    bool cr = false;
    unsigned char command = 0;

    while(true)
    {
        int nBytes = read(socketDescriptor, &newChar, 1);
        if (nBytes == 0)
        {
            throw SocketClosedError();
        }
        if (nBytes == -1)
        {
            throw SocketError("Error while reading from control connection");
        }
        // #ifdef DEBUG
        //     cout << "Received: " << (int) newChar << "\n";
        // #endif

        if (interpretOption)   // interpret this byte as option
        {
            #ifdef DEBUG
                cout << "Telnet - request ";
                switch (command)
                {
                    case WILL:
                    cout << "WILL "; break;
                    case WONT:
                    cout << "WON'T "; break;
                    case DO:
                    cout << "DO "; break;
                    case DONT:
                    cout << "DON'T"; break;
                }
                cout << (int) newChar << "\n";
            #endif
            // send negative response
            respond(command, newChar);
            interpretOption = false;
        }
        else if (newChar == IAC and !interpretCommand)
        // interpret next byte as command
        {
            interpretCommand = true;
        }
        else if (interpretCommand and newChar != IAC)
        // interpret this byte as command
        {
            interpretCommand = false;
            command = newChar;
            #ifdef DEBUG
                cout << "Command: " << (int) command << "\n";
            #endif

            if (command == WILL or command == WONT or
                command == DO or command == DONT)
            {
                interpretOption = true;
            }
            else if (command == 248)    // erase line
            {
                line->clear();
            }
            else if (command == 247 and line->length() > 0)
            {
                // erase character
                line->erase(line->length() - 1, 1);
            }
            else if (command == AYT)    // Are you there?
            {
                respond(command, (unsigned char) 0);
            }
            else if (command == 250)
            {
                subnegotiation = true;
            }
            else if (command == 240)
            {
                subnegotiation = false;
            }
            else if (command < 240)
            {
                char errMsg[50];
                sprintf(errMsg, "Invalid telnet command: %d", command);
                throw CommandError(errMsg);
            }
        }
        else    // regular character
        {
            interpretCommand = false;
            if (newChar != '\0' and !subnegotiation)
                (*line) += newChar;
            // #ifdef DEBUG
            //     cout << "So far: " << *line << "\n";
            // #endif

            if (cr and newChar == '\n')   // CR LF
            {
                // #ifdef DEBUG
                //     cout << "EOL\n";
                // #endif
                break;
            }
            cr = (newChar == '\r');       // CR
        }
    }
    #ifdef DEBUG
        cout << "Telnet - received line: " << *line;
    #endif
}

void Telnet::writeLine(const char* line)
{
    size_t len = strlen(line);
    char* terminated;
    size_t lenTerminated;

    const char* crlfPtr = strstr(line, "\r\n");   // find Telnet new line sequence
    if (crlfPtr == NULL)                    // not found => add
    {
        terminated = new char[len + 3];
        // strcpy(terminated, line);
        memcpy(terminated, line, len);
        // strcat(terminated, "\r\n")
        terminated[len] = '\r';
        terminated[len+1] = '\n';
        terminated[len+2] = '\0';
        lenTerminated = len + 2;
    }
    else
    {
        assert(crlfPtr == &line[len-2]);
        terminated = (char*) line;
        lenTerminated = len;
    }

    int writeResult;

    pthread_mutex_lock(&writeMutex);
    #ifdef DEBUG
        cout << "Telnet - writing: " << terminated;
    #endif

    size_t i = 0;
    while (i < lenTerminated)
    {
        writeResult = write(socketDescriptor, terminated + i, 1);
        if (writeResult == 1 && (unsigned char) terminated[i] == IAC)
        {   // double IAC bytes
            writeResult = write(socketDescriptor, terminated + i, 1);
        }
        if (writeResult == 0)
            throw SocketClosedError();
        else if (writeResult == -1)
            throw SocketError("Error while writing to control connection");
        i += writeResult;
    }
    pthread_mutex_unlock(&writeMutex);

    if (!options.supressGA)
        sendGA();

    if (terminated != line)
        delete[] terminated;
}

void Telnet::writeLine(string* line)
// append CR LF to line and write to connected socket
{
    writeLine(line->c_str());
}
