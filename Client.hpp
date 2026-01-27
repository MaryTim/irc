#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

struct Client {
    int fd;

    bool passOk;
    bool hasNick;
    bool hasUser;
    bool registered;
    bool shouldClose;

    std::string nick;
    std::string user;
    std::string realname;

    Client() : fd(-1), passOk(false), hasNick(false), hasUser(false), registered(false) {}
};

#endif
