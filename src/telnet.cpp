#include <unistd.h>
#include <stdio.h>
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

    pthread_mutex_lock(&(this->writeMutex));
    int nBytes = write(this->socketDescriptor, msg, 2);
    pthread_mutex_unlock(&(this->writeMutex));

    if (nBytes < 2)
    {
        if (nBytes == 0)
            throw SocketClosedError();
        if (nBytes == -1)
            throw SocketError("Error while sending GA");
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
            this->writeLine(&resp);
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

    pthread_mutex_lock(&(this->writeMutex));
    int nBytes = write(this->socketDescriptor, response, resLen);
    pthread_mutex_unlock(&(this->writeMutex));

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
        int nBytes = read(this->socketDescriptor, &newChar, 1);
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
                cout << "request ";
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
            this->respond(command, newChar);
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
                this->respond(command, (unsigned char) 0);
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
                return;
            }
            cr = (newChar == '\r');       // CR
        }
    }
}


void Telnet::writeLine(string* line)
// append CR LF to line and write to connected socket
{
    int lineLen = line->length();
    int finalLen = lineLen;
    char chLine[lineLen+2];
    assert(line->find("\r\n") >= lineLen-2);    // no extra lines
    //if (line->find("\r\n") == string::npos)
    if (not ((*line)[lineLen-2] == '\r' and (*line)[lineLen-1] == '\n'))
    {
        finalLen += 2;
        chLine[finalLen-2] = '\r';      // CR
        chLine[finalLen-1] = '\n';      // LF
    }
    strcpy(chLine, line->c_str());

    pthread_mutex_lock(&(this->writeMutex));
    #ifdef DEBUG
        cout << "Writing: " << chLine << "\n";
    #endif
    int nBytes = write(this->socketDescriptor, chLine, finalLen);
    pthread_mutex_unlock(&(this->writeMutex));

    this->sendGA();

    if (nBytes != finalLen)
    {
        if (nBytes == 0)
            throw SocketClosedError();
        if (nBytes == -1)
            throw SocketError("Error while writing to control connection");
    }
}
