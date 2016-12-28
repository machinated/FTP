#ifndef NET_H
#define NET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <system_error>
#include <stdexcept>
#include <errno.h>
#include <string.h>
#define PORT_L 21

typedef struct sockaddr_in* sockaddrp;
int openServerSocket(sockaddrp serverAddress);
int openServerSocket(int port);

struct options_t
{
    unsigned int port;
    bool supressGA;
    bool local;
};

extern struct options_t options;

class MutexPipe
{
    // POSIX pipe with exclusive access
    pthread_mutex_t pipeLock = PTHREAD_MUTEX_INITIALIZER;
public:
    MutexPipe();
    int descW, descR;
    void readMutex(std::string* line);
    void writeMutex(const char line[], size_t len);
    void writeMutex(std::string* line);
};

class SocketError : public std::system_error
{
public:
    SocketError(const char* what_arg)
        : std::system_error(errno, std::system_category(), what_arg) {}
};

class RefusedError : public std::system_error
{
public:
    RefusedError(const char* what_arg)
        : std::system_error(errno, std::system_category(), what_arg) {}
};

class SocketClosedError : public std::exception
{
public:
    SocketClosedError() : std::exception() {}
};

class CommandError : public std::runtime_error
{
public:
    CommandError(const char* what_arg)
        : std::runtime_error(what_arg) {}
};

class SystemError : public std::system_error
{
public:
    SystemError(const char* what_arg)
        : std::system_error(errno, std::system_category(), what_arg) {}
};

class PipeError : public std::runtime_error
{
public:
    PipeError(const char* what_arg)
        : runtime_error(what_arg) {}
};

#endif
