#include "Server.hpp"

#include <cctype>
#include <cerrno>
#include <cstring>
#include <iostream>

static std::string toUpper(std::string s) {
    for (size_t i = 0; i < s.size(); i++)
        s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
    return s;
}

// Build a user prefix like ":nick!user@localhost"
static std::string userPrefix(const Client& c) {
    std::string u = c.user.empty() ? "user" : c.user;
    return ":" + c.nick + "!" + u + "@localhost";
}

void Server::sendLine(int fd, const std::string& line) {
    std::string out = line + "\r\n";
    ::send(fd, out.c_str(), out.size(), 0); //.c_str() points to the string's internal buffer
}

void Server::disconnectClientByFd(int fd) {
    for (size_t i = 1; i < _pollFDs.size(); i++) {
        if (_pollFDs[i].fd == fd) {
            disconnectClient(static_cast<int>(i));
            return;
        }
    }
}

void Server::onMessage(int fd, const ParsedMessage& msg) {
    std::string cmd = toUpper(msg.command);
    if (cmd == "PING") { 
        handlePING(fd, msg); return;
    }
    if (cmd == "CAP") {
        handleCAP(fd, msg); return;
    }
    if (cmd == "PASS") {
        handlePASS(fd, msg); return;
    }
    if (cmd == "NICK") {
        handleNICK(fd, msg); return;
    }
    if (cmd == "USER") {
        handleUSER(fd, msg); return;
    }

    if ((cmd == "JOIN" || cmd == "PRIVMSG" || cmd == "MODE" || cmd == "WHO") && !_clients[fd].registered) {
        sendLine(fd, ":" + _serverName + " 451 * :You have not registered"); return;
    }

    if (cmd == "JOIN") {
        handleJOIN(fd, msg); return;
    }
    if (cmd == "PRIVMSG") {
        handlePRIVMSG(fd, msg); return;
    }
    if (cmd == "MODE") {
        handleMODE(fd, msg); return;
    }
    if (cmd == "WHO") {
        handleWHO(fd, msg); return;
    }

    Client& c = _clients[fd];
    std::string target = (c.hasNick ? c.nick : "*");
    sendLine(fd, ":" + _serverName + " 421 " + target + " " + msg.command + " :Unknown command");
}

// FUNCTIONS FOR HANDSHAKE

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
    // include usermodes/channelmodes for compatibility
    sendLine(fd, ":" + _serverName + " 004 " + c.nick + " " + _serverName + " 0.1");
}

// JOIN & PRVMSG

void Server::handleJOIN(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];

    if (!c.registered) {
        sendLine(fd, ":" + _serverName + " 451 * :You have not registered");
        return;
    }

    if (msg.params.empty()) {
        sendLine(fd, ":" + _serverName + " 461 " + c.nick + " JOIN :Not enough parameters");
        return;
    }

    std::string chanName = msg.params[0];

    if (chanName.empty() || chanName[0] != '#') {
        sendLine(fd, ":" + _serverName + " 479 " + c.nick + " " + chanName + " :Illegal channel name");
        return;
    }

    Channel& ch = _channels[chanName];
    ch.name = chanName;
    ch.members.insert(fd);

    // Echo JOIN (without ':' before channel is most compatible)
    std::string joinLine = userPrefix(c) + " JOIN " + chanName;
    std::cout << "OUT -> [" << joinLine << "]\n";
    sendLine(fd, joinLine);

    // Broadcast join to others
    for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it) {
        int toFd = *it;
        if (toFd == fd) continue;
        sendLine(toFd, joinLine);
    }

    // NAMES list
    std::string names;
    for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it) {
        Client& m = _clients[*it];
        if (!names.empty()) names += " ";
        names += m.nick;
    }

    sendLine(fd, ":" + _serverName + " 353 " + c.nick + " = " + chanName + " :" + names);
    sendLine(fd, ":" + _serverName + " 366 " + c.nick + " " + chanName + " :End of /NAMES list.");
}

void Server::handlePRIVMSG(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];

    if (msg.params.size() < 2) {
        sendLine(fd, ":" + _serverName + " 461 " + c.nick + " PRIVMSG :Not enough parameters");
        return;
    }

    std::string target = msg.params[0];
    std::string text   = msg.params[1];

    if (text.empty()) {
        sendLine(fd, ":" + _serverName + " 412 " + c.nick + " :No text to send");
        return;
    }

    // Channel message
    if (!target.empty() && target[0] == '#') {
        std::map<std::string, Channel>::iterator chit = _channels.find(target);
        if (chit == _channels.end()) {
            sendLine(fd, ":" + _serverName + " 403 " + c.nick + " " + target + " :No such channel");
            return;
        }

        Channel& ch = chit->second;
        if (ch.members.find(fd) == ch.members.end()) {
            sendLine(fd, ":" + _serverName + " 404 " + c.nick + " " + target + " :Cannot send to channel");
            return;
        }

        std::string line = userPrefix(c) + " PRIVMSG " + target + " :" + text;

        for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it) {
            int toFd = *it;
            if (toFd == fd) continue; // Halloy shows own message locally
            sendLine(toFd, line);
        }
        return;
    }

    // Direct message to nick
    std::map<std::string, int>::iterator it = _nickToFd.find(target);
    if (it == _nickToFd.end()) {
        sendLine(fd, ":" + _serverName + " 401 " + c.nick + " " + target + " :No such nick");
        return;
    }

    int toFd = it->second;
    sendLine(toFd, userPrefix(c) + " PRIVMSG " + target + " :" + text);
}

// -------------------- MODE / WHO (minimal, Halloy-friendly) --------------------

void Server::handleMODE(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];

    if (msg.params.empty())
        return;

    std::string target = msg.params[0];

    // Channel mode query: reply with "no modes" (+)
    if (!target.empty() && target[0] == '#') {
        sendLine(fd, ":" + _serverName + " 324 " + c.nick + " " + target + " +");
        return;
    }

    // User mode query: ignore for now
}

void Server::handleWHO(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];
    std::string mask = msg.params.empty() ? "*" : msg.params[0];

    // If WHO is for a channel, send 352 for each member (minimal but useful for clients)
    if (!mask.empty() && mask[0] == '#') {
        std::map<std::string, Channel>::iterator chit = _channels.find(mask);
        if (chit != _channels.end()) {
            Channel& ch = chit->second;
            for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it) {
                Client& m = _clients[*it];
                std::string mu = m.user.empty() ? "user" : m.user;
                std::string rn = m.realname.empty() ? m.nick : m.realname;

                // 352 <me> <channel> <user> <host> <server> <nick> <flags> :<hopcount> <realname>
                sendLine(fd, ":" + _serverName + " 352 " + c.nick + " " + mask + " " +
                              mu + " localhost " + _serverName + " " + m.nick +
                              " H :0 " + rn);
            }
        }
    }

    // End of WHO
    sendLine(fd, ":" + _serverName + " 315 " + c.nick + " " + mask + " :End of /WHO list.");
}
