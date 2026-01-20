#include "Server.hpp"

// MODE / INVITE / KICK / TOPIC

// for normal user this implementation is enough (not for operators)
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
