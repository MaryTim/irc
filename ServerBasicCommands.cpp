#include "Server.hpp"

// PING / CAP / NICK / USER / PASS / QUIT

void Server::handlePING(int fd, const ParsedMessage& msg) {
    if (!msg.params.empty())
        sendLine(fd, "PONG :" + msg.params[0]);
    else
        sendLine(fd, "PONG");
}

// capabilities
void Server::handleCAP(int fd, const ParsedMessage& msg) {
    // "CAP LS 302", "CAP END"
    if (msg.params.empty())
        return;

    std::string sub = toUpper(msg.params[0]);
    if (sub == "LS") {
        sendLine(fd, ":" + _serverName + " CAP * LS :"); //empty capability list
    }
    // CAP END: no reply needed
}

void Server::handleNICK(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];
    c.fd = fd;

    if (msg.params.empty()) {
        sendLine(fd, ":" + _serverName + " 431 * :No nickname given");
        return;
    }

    std::string newNick = msg.params[0];

    std::map<std::string, int>::iterator it = _nickToFd.find(newNick);
    if (it != _nickToFd.end() && it->second != fd) {
        sendLine(fd, ":" + _serverName + " 433 * " + newNick + " :Nickname is already in use");
        return;
    }

    if (c.hasNick)
        _nickToFd.erase(c.nick);

    c.nick = newNick;
    c.hasNick = true;
    _nickToFd[newNick] = fd;

    tryRegister(fd);
}

// USER <username> <mode> <unused> :<realname>
void Server::handleUSER(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];
    c.fd = fd;

    if (c.registered)
        return;

    if (msg.params.size() < 4) {
        sendLine(fd, ":" + _serverName + " 461 * USER :Not enough parameters");
        return;
    }

    c.user = msg.params[0];
    c.realname = msg.params[3];
    c.hasUser = true;

    tryRegister(fd);
}

void Server::handlePASS(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];
    c.fd = fd;

    if (c.registered)
        return;

    if (msg.params.empty()) {
        sendLine(fd, ":" + _serverName + " 461 * PASS :Not enough parameters");
        return;
    }

    if (msg.params[0] != _password) {
        sendLine(fd, ":" + _serverName + " 464 * :Password incorrect");
        disconnectClientByFd(fd);
        return;
    }

    c.passOk = true;
    tryRegister(fd);
}

void Server::handleQUIT(int pollInd) {
    disconnectClient(pollInd);
}

void Server::tryRegister(int fd) {
    Client& c = _clients[fd];

    if (c.registered) return;
    if (!c.passOk) return;
    if (!c.hasNick) return;
    if (!c.hasUser) return;

    c.registered = true;

    sendLine(fd, ":" + _serverName + " 001 " + c.nick + " :Welcome to the IRC server");
    sendLine(fd, ":" + _serverName + " 002 " + c.nick + " :Your host is " + _serverName);
    sendLine(fd, ":" + _serverName + " 003 " + c.nick + " :This server was created today");
    // include user modes/channel modes for compatibility
    sendLine(fd, ":" + _serverName + " 004 " + c.nick + " " + _serverName + " 0.1");
}