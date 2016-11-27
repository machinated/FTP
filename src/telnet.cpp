#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <cassert>
#include <string.h>
#include "telnet.h"

#define CRLF "\x0d\x0a"
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
: socketDescriptor(socket), error(0)
{
    writeMutex = PTHREAD_MUTEX_INITIALIZER;
}


int Telnet::sendGA()
{
    char msg[3];
    msg[0] = IAC;
    msg[1] = GA;
    msg[2] = '\0';

    pthread_mutex_lock(&(this->writeMutex));
    int nBytes = write(this->socketDescriptor, msg, 3);
    pthread_mutex_unlock(&(this->writeMutex));

    if (nBytes < 3)
    {
        if (nBytes == 0)
            this->error = ERR_SOCK_CLOSED;
        if (nBytes == -1)
            this->error = ERR_IO;
        return 1;
    }
    return 0;
}


int Telnet::respond(unsigned char command, unsigned char option)
{
    char response[4] = "\x00\x00\x00";
    int resLen = 4;
    response[0] = IAC;

    switch (command)
    {
        case AYT:
            strcpy(response, "...");
            break;
        case WILL:
        case WONT:
            response[1] = DONT;
            response[2] = option;
            break;
        case DO:
        case DONT:
            response[1] = WONT;
            response[2] = option;
            break;
        default:
            assert(false);
    }

    pthread_mutex_lock(&(this->writeMutex));
    int nBytes = write(this->socketDescriptor, response, resLen);
    pthread_mutex_unlock(&(this->writeMutex));

    if (nBytes < resLen)
    {
        if (nBytes == 0)
            this->error = ERR_SOCK_CLOSED;
        if (nBytes == -1)
            this->error = ERR_IO;
        return 1;
    }
    return 0;
}


int Telnet::readLine(string* line)
// read exactly one line
{
    line->clear();
    unsigned char newChar;
    bool interpretCommand = false;
    bool interpretOption = false;
    bool cr = false;
    unsigned char command = 0;

    while(true)
    {
        int nBytes = read(this->socketDescriptor, &newChar, 1);
        if (nBytes == 0)
        {
            this->error = ERR_SOCK_CLOSED;
            return 1;
        }
        if (nBytes == -1)
        {
            this->error = ERR_IO;
            return 1;
        }
        // #ifdef DEBUG
        //     cout << "Received: " << (int) newChar << "\n";
        // #endif

        if (newChar == IAC and !interpretCommand)
        // interpret next byte as command
        {
            interpretCommand = true;
        }
        else
        {
            if (interpretCommand and newChar != IAC)
            // interpret this byte as command
            {
                command = newChar;
                #ifdef DEBUG
                    cout << "Command: " << (int) command << "\n";
                #endif

                // TODO respond to SB, SE commands

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
                    int result = this->respond(command, (unsigned char) 0);
                    if (result)
                        return 1;
                }
                else if (command < 240)
                {
                    this->error = ERR_COMMAND;
                    return 1;
                }
                interpretCommand = false;
            }
            else if (interpretOption)   // interpret this byte as option
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
                int result = this->respond(command, newChar);
                if (result)
                    return 1;
                interpretOption = false;
            }
            else    // regular character
            {
                (*line) += newChar;
                // #ifdef DEBUG
                //     cout << "So far: " << *line << "\n";
                // #endif

                if (cr and newChar == 10)   // CR LF
                {
                    // #ifdef DEBUG
                    //     cout << "EOL\n";
                    // #endif
                    return 0;
                }
                cr = (newChar == 13);       // CR
            }
        }
    }
}


int Telnet::writeLine(string* line)
// append CR LF to line and write to connected socket
{
    int lineLen = line->length();
    int finalLen = lineLen + 3;
    char chLine[finalLen];
    strcpy(chLine, line->c_str());
    chLine[finalLen-3] = 13;      // CR
    chLine[finalLen-2] = 10;      // LF
    chLine[finalLen-1] = '\0';

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
            this->error = ERR_SOCK_CLOSED;
        if (nBytes == -1)
            this->error = ERR_IO;
        return 1;
    }
    return 0;
}
