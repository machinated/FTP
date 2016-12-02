#ifndef NET_H
#define NET_H
#include <system_error>
#include <stdexcept>
#include <errno.h>
#include <string.h>

using namespace std;

int openServerSocket(int port);

class SocketError : public system_error
{
public:
    SocketError(const char* what_arg)
        : system_error(errno, system_category(), what_arg) {}
};

class SocketClosedError : public exception
{
public:
    SocketClosedError() : exception() {}
};

class CommandError : public runtime_error
{
public:
    CommandError(const char* what_arg)
        : runtime_error(what_arg) {}
};

#endif
