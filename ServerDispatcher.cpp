#include "Server.hpp"

// command dispatcher + small helper commands

std::string Server::toUpper(std::string s) {
    for (size_t i = 0; i < s.size(); i++)
        s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
    return s;
}

// Build a user prefix like ":nick!user@localhost"
// It’s mandated by the IRC protocol
std::string Server::userPrefix(const Client& c) {
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

bool Server::isChannelOperator(const Channel& ch, int fd) const {
    return ch.operators.find(fd) != ch.operators.end();
}

void Server::broadcastToChannel(const Channel& ch, const std::string& line, int exceptFd) {
    for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it) {
        int toFd = *it;
        if (toFd == exceptFd) continue;
        sendLine(toFd, line);
    }
}

// DISPATCH MESSAGES

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