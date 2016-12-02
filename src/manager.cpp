#include <string>
#include <regex>
#include <iostream>
#include <unistd.h>
#include "net.h"
#include "telnet.h"
#include "manager.h"
using namespace std;

list<ControlConnection*> ControlConnection::List = list<ControlConnection*>();
pthread_mutex_t ControlConnection::listMutex = PTHREAD_MUTEX_INITIALIZER;

enum Commands
{
    USER, QUIT, PORT, TYPE, MODE, STRU,
    RETR, STOR, NOOP, MKD, RMD
};

#define NCOMMANDS 11
const char* CommandStrings[] = {"USER", "QUIT", "PORT", "TYPE", "MODE", "STRU",
                               "RETR", "STOR", "NOOP", "MKD", "RMD"};

const char* commandPatterns[] = {
    "USER ([[:print:]]+)\\r\\n",
    "QUIT\\r\\n",
    "PORT (\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\r\\n",
    // "TYPE ([AE] [NTC]|I|L \\d+)\\r\\n",
    "TYPE (([AE]) ([NTC])|I|(L) (\\d+))\\r\\n",
    "MODE ([SBC])\\r\\n",
    "STRU ([FRP])\\r\\n",
    "RETR ([[:print:]]+)\\r\\n",
    "STOR ([[:print:]]+)\\r\\n",
    "NOOP\\r\\n",
    "MKD ([[:print:]]+)\\r\\n",
    "RMD ([[:print:]]+)\\r\\n"};

typedef struct Command
{
    int type;
    int argc;
    string* args;
}Command;

Command* parseLine(string* line)
{
    for (int i = 0; i<NCOMMANDS; i++)
    {
        regex re(commandPatterns[i], regex_constants::icase);
        smatch match;

        if (regex_match(*line, match, re))
        {
            Command* command = new Command;
            command->type = i;
            command->argc = match.size()-1;
            command->args = new string[command->argc];
            for (int j=0; j<command->argc; j++)
            {
                command->args[j] = match.str(j+1);
            }

            return command;
        }
    }
    return nullptr;
}

void* run(void* cc_p)
{
    ControlConnection* cc = (ControlConnection*)cc_p;
    cc->Run();
    return 0;
}

ControlConnection::ControlConnection(int socketDescriptor)
    : socket(socketDescriptor), telnet(socketDescriptor)
{
    pthread_mutex_lock(&listMutex);
    List.push_back(this);
    listIterator = prev(List.end());
    pthread_mutex_unlock(&listMutex);
}

void ControlConnection::Run()
{
    while(1)
    {
        try
        {
            string line;
            telnet.readLine(&line);

            Command* command = parseLine(&line);
            if (command == nullptr)
            {
                cout << "Received invalid command: ";
                cout << line;
                string response("Invalid command: ");
                response += line;
                telnet.writeLine(&response);
            }
            else
            {
                cout << "FTP command: ";
                cout << CommandStrings[command->type] << "\n";
                cout << "Arguments: ";
                for (int j = 0; j<command->argc; j++)
                {
                    cout << command->args[j];
                    if (j != (command->argc)-1)
                        cout << ", ";
                }
                cout << "\n";
                if (command->type == QUIT)
                {
                    break;
                }
                delete[] command->args;
                delete command;
            }
        }
        catch (SocketClosedError& e)
        {
            cout << "Socket closed by other party\n";
            break;
        }
        catch (SocketError& e)
        {
            cerr << "Network error " << e.code() << e.what() << "\n";
            break;
        }
    }
    delete this;
}

ControlConnection::~ControlConnection()
{
    pthread_mutex_lock(&listMutex);
    List.erase(listIterator);
    // List.remove(this);
    pthread_mutex_unlock(&listMutex);

    int closeResult;
    closeResult = close(socket);
    if (closeResult)
    {
        cerr << "Błąd podczas zamykania połączenia\n";
    }
}
