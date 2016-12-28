#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <errno.h>

#include <string>
#include <regex>
#include <iostream>

#include "net.h"
#include "telnet.h"
#include "manager.h"
using namespace std;

list<ControlConnection*> ControlConnection::List = list<ControlConnection*>();
pthread_mutex_t ControlConnection::listMutex = PTHREAD_MUTEX_INITIALIZER;

enum Commands
{
    USER, PASS, ACCT, CWD, CDUP, SMNT, QUIT, REIN, PORT, PASV, TYPE, STRU, MODE,
    RETR, STOR, STOU, APPE, ALLO, REST, RNFR, RNTO, ABOR, DELE, RMD, MKD, PWD,
    LIST, NLST, SITE, SYST, STAT, HELP, NOOP
};

#define NCOMMANDS 33
const char* CommandStrings[] = {
    "USER", "PASS", "ACCT", "CWD", "CDUP", "SMNT", "QUIT", "REIN", "PORT",
    "PASV", "TYPE", "STRU", "MODE", "RETR", "STOR", "STOU", "APPE", "ALLO",
    "REST", "RNFR", "RNTO", "ABOR", "DELE", "RMD", "MKD", "PWD", "LIST", "NLST",
    "SITE", "SYST", "STAT", "HELP", "NOOP"
};

typedef struct Command
{
    int type;
    int argc;
    string* args;
}Command;

bool checkName(string* arg)
{
    static const char pattern[] = "[[:print:]]+";
    static const regex re(pattern, regex_constants::icase);

    string fname;
    smatch match;

    return (regex_match(*arg, match, re) && match.size() == 1);
}

int parseLine(const string* line, string* args)
{   // split line into command and arguments
    // return command as int, save the rest of line to string pointed by args
    // if command was not recognized return -1
    // line must end with CR LF (Telnet new line sequence)
    size_t len = line->length();
    assert((*line)[len-2] == '\r');
    assert((*line)[len-1] == '\n');

    char cPtr[len+1];
    line->copy(cPtr, len, 0);           // copy line to C-style string cPtr
    cPtr[len-2] = '\0';                 // resize string to omit CRLF

    char* argsPtr = index(cPtr, ' ');   // find first space character
    if (argsPtr == NULL)                    // no space in string
    {
        *args = string();               // argument part = empty string
    }
    else
    {
        *args = string(argsPtr+1);        // copy argument part to args
        *argsPtr = '\0';                // end string on space character
    }
    // cPtr now contains just the command

    // try to match cPtr with any string in CommandStrings
    for (int i = 0; i<NCOMMANDS; i++)
    {
        size_t cmdLen = strlen(CommandStrings[i]);
        if (strncasecmp(cPtr, CommandStrings[i], cmdLen) == 0)
        {
            char end = cPtr[cmdLen];
            if (end == '\0' || end == '\r')
                return i;
        }
    }
    return -1;
}

void* runCC(void* cc_p)
{
    ControlConnection* cc = (ControlConnection*)cc_p;
    cc->Run();
    return 0;
}

ControlConnection::ControlConnection(int socketDescriptor)
    : socket(socketDescriptor), telnet(socketDescriptor),
      dataConnection(&telnet), user("")
{
    pthread_mutex_lock(&listMutex);
    List.push_back(this);
    listIterator = prev(List.end());
    pthread_mutex_unlock(&listMutex);

    // find string representetion of client IP addrress and port
    struct sockaddr_in addr;
    unsigned int len = sizeof(addr);
    if (getpeername(socket, (struct sockaddr*) &addr, &len))
    {
        cerr << "Error looking up peer address.\n";
        exit(1);
    }
    char tempBuf[16];
    inet_ntop(AF_INET, (const void*) &(addr.sin_addr), tempBuf, 16);
    if (tempBuf == nullptr)
    {
        cerr << "Error converting client address to string.\n";
        exit(1);
    }
    peerAddrStr = string(tempBuf) + string(":")
                    + to_string((unsigned short) addr.sin_port);
    #ifdef DEBUG
        cout << "Initialized connection with client: " << peerAddrStr << "\n";
    #endif

    settings.passive = false;
    settings.ascii = true;
    settings.mode = MODE_STREAM;
    settings.structure = STRU_FILE;

    settings.addrLocal.sin_family = AF_INET;
    settings.addrLocal.sin_addr.s_addr = htonl(INADDR_ANY);
    settings.addrLocal.sin_port = htons(options.port - 1);

    settings.addrRemote.sin_family = AF_INET;
    settings.addrRemote.sin_addr.s_addr = addr.sin_addr.s_addr;
    settings.addrRemote.sin_port = addr.sin_port;

    dataConnection.SetSettings(&settings);
}

inline void ControlConnection::sendResponse(const char response[])
{
    telnet.writeLine(response);
    #ifdef DEBUG
        cout << peerAddrStr << " << " << response << '\n';
    #endif
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
        cerr << "Error while closing connection\n";
    }
    #ifdef DEBUG
        cout << "Terminated connection with client: " << peerAddrStr << "\n";
    #endif
}

void ControlConnection::Run()
{
    run = true;

    CmdHandlerP commandHandlers[NCOMMANDS];
    for (int i=0; i<NCOMMANDS; i++)
    {
        commandHandlers[i] = &ControlConnection::CmdNotImplemented;  // default
    }
    commandHandlers[USER] = &ControlConnection::CmdUser;
    commandHandlers[CWD] = &ControlConnection::CmdCwd;
    commandHandlers[CDUP] = &ControlConnection::CmdCdup;
    commandHandlers[QUIT] = &ControlConnection::CmdQuit;
    commandHandlers[PORT] = &ControlConnection::CmdPort;
    commandHandlers[TYPE] = &ControlConnection::CmdType;
    commandHandlers[MODE] = &ControlConnection::CmdMode;
    commandHandlers[STRU] = &ControlConnection::CmdStru;
    commandHandlers[RETR] = &ControlConnection::CmdRetr;
    commandHandlers[STOR] = &ControlConnection::CmdStor;
    commandHandlers[ABOR] = &ControlConnection::CmdAbor;
    commandHandlers[DELE] = &ControlConnection::CmdDele;
    commandHandlers[NOOP] = &ControlConnection::CmdNoop;
    commandHandlers[MKD] = &ControlConnection::CmdMkd;
    commandHandlers[NLST] = &ControlConnection::CmdNlst;
    commandHandlers[LIST] = &ControlConnection::CmdNlst;    // TODO check OK ?!
    commandHandlers[RMD] = &ControlConnection::CmdRmd;
    commandHandlers[PWD] = &ControlConnection::CmdPwd;
    commandHandlers[PASV] = &ControlConnection::CmdPasv;


    if (options.local)
    {
        if (settings.addrRemote.sin_addr.s_addr != htonl(INADDR_LOOPBACK))
        {
            sendResponse("521 Service not available for remote clients.");
            delete this;
            return;
        }
    }

    sendResponse("220 xmftp avaiting input");

    while(run)
    {
        try
        {
            fd_set watchedFiles;
            FD_ZERO(&watchedFiles);
            FD_SET(telnet.socketDescriptor, &watchedFiles);
            FD_SET(dataConnection.pipe.descR, &watchedFiles);
            int selectResult = select(FD_SETSIZE, &watchedFiles, NULL, NULL, NULL);
            if (selectResult == -1)
            {
                throw SystemError("'select' call error");
            }

            if (FD_ISSET(telnet.socketDescriptor, &watchedFiles))
            {
                string line;
                telnet.readLine(&line);

                string args;
                int command = parseLine(&line, &args);
                if (command == -1)
                {
                    sendResponse("500 Syntax error, command unrecognized.");
                }
                else
                {
                    (this->*commandHandlers[command])(&args);
                }
            }

            if (FD_ISSET(dataConnection.pipe.descR, &watchedFiles))
            {
                run = false;
            }
        }
        catch (SocketClosedError& e)
        {
            cout << "Socket closed by other party\n";
            break;
        }
        catch (SocketError& e)
        {
            cerr << "Network error " << e.code() << " " << e.what() << "\n";
            break;
        }
    }
    delete this;
}

// #############################################################################
// COMMAND HANDLERS
// argument 'args' is a pointer to string containing command arguments without
// leading space character or terminating CR LF
// #############################################################################

void ControlConnection::CmdUser(string* args)
{
    user = *args;
    sendResponse("230 User logged in, proceed.");
}

void ControlConnection::CmdCwd(string* args)
{
    int chdirResult = chdir(args->c_str());

    if (chdirResult == 0)
    {
        sendResponse("250 Requested file action okay, comleted.");
    }
    else
    {
        if (errno == ENOTDIR)
        {
            sendResponse(
                "550 Requested action not taken: "
                "not a directory.");
        }
        else if (errno == ENOENT)
        {
            sendResponse(
                "550 Requested action not taken: "
                "directory does not exist.");
        }
        else
        {
            cerr << "Error changing to directory: " << *args << "\n";
            sendResponse("451 Requested action aborted: "
                          "local error in processing.");
        }
    }
}

void ControlConnection::CmdCdup(string* args)
{
    if (args->length() == 0)
    {
        string up("..");
        CmdCwd(&up);
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdQuit(string* args)
{
    if (args->length() == 0)
    {
        run = false;
        sendResponse(
            "221 Service closing control connection. "
            "Logged out if appropriate.");
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdPort(string* args)
{
    static const char pattern[] = "(\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)";
    static const regex re(pattern, regex_constants::icase);

    smatch match;

    if (regex_match(*args, match, re) && match.size() == 7)
    {
        char addrStr[17];
        int formatResult;
        formatResult = snprintf(addrStr, 17, "%s.%s.%s.%s",
                       match.str(1).c_str(), match.str(2).c_str(),
                       match.str(3).c_str(), match.str(4).c_str());

        if (formatResult < 0)
        {
            sendResponse("451 Requested action aborted: "
                          "local error in processing.");
            return;
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;

        #ifdef DEBUG
            cout << "Setting IP address to: " << addrStr << "\n";
        #endif

        int atonResult;
        atonResult = inet_aton(addrStr, &(addr.sin_addr));

        if (atonResult == 0)
        {
            sendResponse("501 Syntax error in parameters or arguments; "
                          "invalid IP address.");
            return;
        }

        in_port_t portNumber;
        int p1, p2;
        p1 = atoi(match.str(5).c_str());
        p2 = atoi(match.str(6).c_str());
        if (p1 < 0 || p1 > 255 || p2 < 0 || p2 > 255)
        {
            sendResponse("501 Syntax error in parameters or arguments: "
                          "invalid port number");
            return;
        }

        portNumber = p1;
        portNumber *= 256;
        portNumber += p2;
        addr.sin_port = portNumber;
        #ifdef DEBUG
            cout << "Setting port number to: " << portNumber << "\n";
        #endif

        memcpy(&(settings.addrRemote), &addr, sizeof(struct sockaddr_in));
        dataConnection.SetSettings(&settings);
        sendResponse("200 Command okay.");
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdType(string* args)
{
    static const char pattern[] = "([AE])( ([NTC]))?|(I)|(L) (\\d+)";
    static const regex re(pattern, regex_constants::icase);

    smatch match;

    if (regex_match(*args, match, re))
    {
        string arg1 = match.str(1) + match.str(4) + match.str(5);
        string arg2 = match.str(3) + match.str(6);

        char c1, c2;
        c1 = arg1[0];
        if (c1 > 90)
        {
            c1 -= 32;    // uppercase
        }

        if (arg2.length() > 0)
        {
            c2 = arg2[0];
        }
        else
        {
            c2 = '\0';
        }
        if (c2 > 90)
        {
            c2 -= 32;    // uppercase
        }

        if (c1 == 'A' && (c2 == '\0' || c2 == 'N'))     // ascii non-print
        {
            settings.ascii = true;
        }
        else if (c1 == 'I')                             // image (binary)
        {
            settings.ascii = false;
        }
        else
        {
            sendResponse("504 Command not implemented for that parameter.");
            return;
        }
        dataConnection.SetSettings(&settings);
        sendResponse("200 Command okay.");
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdMode(string *args)
{
    if (args->length() != 1)
    {
        sendResponse("501 Syntax error in parameters or arguments.");
        return;
    }

    char c = (*args)[0];
    if (c > 90)
    {
        c -= 32;    // uppercase
    }
    if (c == 'S')   // stream mode
    {
        sendResponse("200 Command okay.");
    }
    else if (c == 'B' || c == 'C')  // block mode, compressed mode
    {
        sendResponse("504 Command not implemented for that parameter.");
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdStru(string* args)
{
    if (args->length() != 1)
    {
        sendResponse("501 Syntax error in parameters or arguments.");
        return;
    }

    char c = (*args)[0];
    if (c > 90)
    {
        c -= 32;    // uppercase
    }
    if (c == 'F' || c == 'R')   // file structure, record structure
    {
        sendResponse("200 Command okay.");
    }
    else if (c == 'P')          // page structure
    {
        sendResponse("504 Command not implemented for that parameter.");
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdRetr(string* args)
{
    if (dataConnection.active)
    {
        sendResponse("400 Processing previous command");
        return;
    }

    int fileDesc = open(args->c_str(), O_RDONLY);
    if (fileDesc == -1)
    {
        if (errno == ENOENT)
        {
            sendResponse(
                "550 Requested action not taken. "
                "File does not exist");
        }
        else
        {
            sendResponse(
                "451 Requested action aborted: "
                "local error in processing.");
        }

        return;
    }

    dataConnection.ThreadRetrieve(fileDesc);
}

void ControlConnection::CmdStor(string* args)
{
    if (!checkName(args))
    {
        sendResponse(
            "553 Requested file action not taken. "
            "File name not allowed.");
        return;
    }

    if (dataConnection.active)
    {
        sendResponse("400 Processing previous command");
        return;
    }
    // open file for writing, create it if it doesn't exist, truncate if it does
    // read and write permission for user, group and others
    int fileDesc = open(args->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fileDesc == -1)
    {
        if (errno == ENOSPC || errno == EDQUOT)
        {
            sendResponse(
                "452 Requested file action not taken. "
                "Insufficient storage space in system.");
        }
        else if (errno == EISDIR)
        {
            sendResponse(
                "550 Requested file action not taken."
                "File name refers to a directory.");
        }
        else if (errno == ENAMETOOLONG)
        {
            sendResponse(
                "553 Requested file action not taken."
                "File name too long.");
        }
        else
        {
            sendResponse(
                "451 Requested action aborted: "
                "local error in processing.");
        }

        return;
    }

    dataConnection.ThreadStore(fileDesc);
}

void ControlConnection::CmdAbor(string* args)
{
    if (args->length() == 0)
    {
        if (dataConnection.active)
        {
            dataConnection.Abort();
        }
        else
        {
            dataConnection.Close();
        }
        sendResponse(
            "226 Closing data connection; "
            "action successfully aborted.");
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdDele(string* args)
{
    int unlinkResult = unlink(args->c_str());

    if (unlinkResult == 0)
    {
        sendResponse("250 Requested file action okay, completed.");
    }
    else
    {
        if (errno == EBUSY)
        {
            sendResponse(
                "450 Requested file action not taken: "
                "file busy.");
        }
        if (errno == ENOENT)
        {
            sendResponse(
                "550 Requested action not taken: "
                "file does not exist");
        }
        else if (errno == EISDIR)
        {
            sendResponse(
                "550 Requested file action not taken: "
                "file name refers to a directory.");
        }
        else if (errno == ENAMETOOLONG)
        {
            sendResponse(
                "553 Requested file action not taken: "
                "file name too long.");
        }
        else
        {
            sendResponse(
                "451 Requested action aborted: "
                "local error in processing.");
        }
    }
}

void ControlConnection::CmdNoop(string* args)
{
    if (args->length() == 0)
    {
        sendResponse("200 Command okay");
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdMkd(string* args)
{
    if (!checkName(args))
    {
        sendResponse(
            "553 Requested file action not taken. "
            "File name not allowed.");
        return;
    }

    int mkdirResult = mkdir(args->c_str(), 0777);

    if (mkdirResult == 0)
    {
        char msg[1024];
        snprintf(msg, 1024, "257 \"%s\" created.", args->c_str());
        sendResponse(msg);
    }
    else
    {
        if (errno == ENOSPC || errno == EDQUOT)
        {
            sendResponse(
                "452 Requested directory action not taken: "
                "insufficient storage space in system.");
        }
        else if (errno == ENOTDIR)
        {
            sendResponse("553 Invalid directory name");
        }
        else if (errno == EEXIST)
        {
            sendResponse(
                "550 Requested directory action not taken: "
                "file or directory with the same name already exists");
        }
        else if (errno == ENAMETOOLONG)
        {
            sendResponse(
                "553 Requested directory action not taken: "
                "file name too long.");
        }
        else
        {
            sendResponse(
                "451 Requested action aborted: "
                "local error in processing.");
        }
    }
}

void ControlConnection::CmdRmd(string* args)
{
    int rmdirResult = rmdir(args->c_str());

    if (rmdirResult == 0)
    {
        sendResponse("250 Requested file action okay, completed.");
    }
    else
    {
        if (errno == EBUSY)
        {
            sendResponse(
                "450 Requested directory action not taken: "
                "file busy.");
        }
        if (errno == ENOENT)
        {
            sendResponse(
                "550 Requested action not taken: "
                "directory does not exist");
        }
        else if (errno == ENOTDIR)
        {
            sendResponse(
                "550 Requested directory action not taken: "
                "name does not refer to directory.");
        }
        else if (errno == ENOTEMPTY || errno == EEXIST)
        {
            sendResponse(
                "550 Requested directory action not taken: "
                "Directory is not empty.");
        }
        else if (errno == ENAMETOOLONG)
        {
            sendResponse(
                "553 Requested directory action not taken: "
                "path is too long.");
        }
        else
        {
            sendResponse(
                "451 Requested action aborted: "
                "local error in processing.");
        }
    }
}

void ControlConnection::CmdPwd(string* args)
{
    if (args->length() == 0)
    {
        char* dirname = getcwd(NULL, 0);
        if (dirname == NULL)
        {
            if (errno == ENOENT)
            {
                sendResponse(
                    "550 Requested action not taken: "
                    "directory does not exist");
            }
            else if (errno == ENAMETOOLONG)
            {
                sendResponse(
                    "553 Requested directory action not taken: "
                    "path is too long.");
            }
            else
            {
                sendResponse(
                    "451 Requested action aborted: "
                    "local error in processing.");
            }
            return;
        }
        char msg[1024];
        snprintf(msg, 1024, "257 %s", dirname);
        sendResponse(msg);
        free(dirname);
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdNlst(string* args)
{
    DIR* dirStream;

    if (args->length() == 0)
    {
        char* dirname = getcwd(NULL, 0);
        if (dirname == NULL)
        {
            sendResponse("451 Requested action aborted: "
                          "local error in processing.");
            return;
        }
        cout << "Opening directory: " << dirname << "\n";
        dirStream = opendir(dirname);
        free(dirname);
    }
    else
    {
        dirStream = opendir(args->c_str());
    }



    if (dirStream == NULL)
    {
        if (errno == ENOTDIR)
        {
            sendResponse(
                "550 Requested action not taken: "
                "not a directory.");
        }
        else if (errno == ENOENT)
        {
            sendResponse(
                "550 Requested action not taken: "
                "directory does not exist.");
        }
        else
        {
            cerr << "Error opening directory." << "\n";
            sendResponse("451 Requested action aborted: "
                          "local error in processing.");
        }
        return;
    }

    dataConnection.ThreadNlist(dirStream);
}

void ControlConnection::CmdPasv(string *args)
{
    #ifdef DEBUG
        cout << peerAddrStr << ": processing PASV command\n";
    #endif

    if (args->length() == 0)
    {
        settings.passive = true;
        dataConnection.SetSettings(&settings);
        try
        {
            dataConnection.Open();
        }
        catch (system_error &e)
        {
            dataConnection.HandleException(e);
        }

        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        if (getsockname(dataConnection.serverSocket, (struct sockaddr*) &addr,
                        &len))
        {
            cerr << "Error looking up own IP address";
            sendResponse("451 Requested action aborted: "
                          "local error in processing.");
            return;
        }
        uint32_t a = ntohl(addr.sin_addr.s_addr);
        uint16_t p = ntohs(addr.sin_port);
        uint8_t a1, a2, a3, a4, p1, p2;
        a1 = a>>24;
        a2 = (a>>16) & 0xFF;
        a3 = (a>>8) & 0xFF;
        a4 = a & 0xFF;

        p1 = p>>8;
        p2 = p & 0xFF;

        char response[100];
        int formatResult = snprintf(response, 100,
            "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).", a1, a2, a3, a4,
            p1, p2);
        if (formatResult < 0)
        {
            cerr << "snprintf error\n";
            sendResponse("451 Requested action aborted: "
                          "local error in processing.");
            return;
        }
        sendResponse(response);
    }
    else
    {
        sendResponse("501 Syntax error in parameters or arguments.");
    }
}

void ControlConnection::CmdNotImplemented(string*)
{
    sendResponse("502 Command not implemented.");
}
