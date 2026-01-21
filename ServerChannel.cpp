#include "Server.hpp"

// JOIN / PRVMSG / WHO

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
    std::string providedKey = (msg.params.size() >= 2) ? msg.params[1] : "";

    if (chanName.size() < 2 || chanName[0] != '#') {
        sendLine(fd, ":" + _serverName + " 479 " + c.nick + " " + chanName + " :Illegal channel name");
        return;
    }

    // check if channel is new
    bool isNew = (_channels.find(chanName) == _channels.end());

    Channel& ch = _channels[chanName];
    ch.name = chanName;

    // If already in channel, do nothing
    if (ch.members.find(fd) != ch.members.end())
        return;

    // Enforce +i (invite-only) for existing channels
    if (!isNew && ch.inviteOnly) {
        if (ch.invited.find(fd) == ch.invited.end()) {
            sendLine(fd, ":" + _serverName + " 473 " + c.nick + " " + chanName + " :Cannot join channel (+i)");
            return;
        }
    }

    // Enforce +k (key) for existing channels
    if (!isNew && ch.hasKey) {
        if (providedKey != ch.key) {
            sendLine(fd, ":" + _serverName + " 475 " + c.nick + " " + chanName + " :Cannot join channel (+k)");
            return;
        }
    }

    // Enforce +l (limit) for existing channels
    if (!isNew && ch.hasLimit) {
        if (ch.members.size() >= ch.userLimit) {
            sendLine(fd, ":" + _serverName + " 471 " + c.nick + " " + chanName + " :Cannot join channel (+l)");
            return;
        }
    }

    ch.members.insert(fd);
    ch.invited.erase(fd); // consume invite if any

    // First member becomes operator
    if (isNew)
        ch.operators.insert(fd);

    std::string joinLine = userPrefix(c) + " JOIN " + chanName;
    sendLine(fd, joinLine);

    // Broadcast join to others
    for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); it++) {
        int toFd = *it;
        if (toFd == fd) continue; //skip joining user to send everyone else but them
        sendLine(toFd, joinLine);
    }

    // Topic replies (helps real clients)
    if (ch.topic.empty())
        sendLine(fd, ":" + _serverName + " 331 " + c.nick + " " + chanName + " :No topic is set");
    else
        sendLine(fd, ":" + _serverName + " 332 " + c.nick + " " + chanName + " :" + ch.topic);


    // NAMES list
    std::string names;
    for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); it++) {
        Client& m = _clients[*it];
        if (!names.empty())
            names += " ";
        if (ch.operators.find(*it) != ch.operators.end())
            names += "@";
        names += m.nick;
    }

    sendLine(fd, ":" + _serverName + " 353 " + c.nick + " = " + chanName + " :" + names);
    sendLine(fd, ":" + _serverName + " 366 " + c.nick + " " + chanName + " :End of /NAMES list.");
}


void Server::handlePRIVMSG(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];

    if (!c.registered) {
        sendLine(fd, ":" + _serverName + " 451 * :You have not registered");
        return;
    }

    if (msg.params.empty()) {
        sendLine(fd, ":" + _serverName + " 461 " + c.nick + " PRIVMSG :Not enough parameters");
        return;
    }

    // One param -> we have something (often trailing text) but no target
    if (msg.params.size() == 1) {
        sendLine(fd, ":" + _serverName + " 411 " + c.nick + " :No recipient given (PRIVMSG)");
        return;
    }

    std::string target = msg.params[0];
    std::string text = msg.params[1];

    if (target.empty()) {
        sendLine(fd, ":" + _serverName + " 411 " + c.nick + " :No recipient given (PRIVMSG)");
        return;
    }

    if (text.empty()) {
        sendLine(fd, ":" + _serverName + " 412 " + c.nick + " :No text to send");
        return;
    }

    // Channel message
    if (target[0] == '#') {
        std::map<std::string, Channel>::iterator chit = _channels.find(target);

        // a channel with that name doesnt exist
        if (chit == _channels.end()) {
            sendLine(fd, ":" + _serverName + " 403 " + c.nick + " " + target + " :No such channel");
            return;
        }

        // a user isn't a memeber of that channel
        Channel& ch = chit->second;
        if (ch.members.find(fd) == ch.members.end()) {
            sendLine(fd, ":" + _serverName + " 404 " + c.nick + " " + target + " :Cannot send to channel");
            return;
        }

        std::string line = userPrefix(c) + " PRIVMSG " + target + " :" + text;

        for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); it++) {
            int toFd = *it;
            if (toFd == fd) continue; // Halloy shows own message locally
            sendLine(toFd, line);
        }
        return;
    }

    // Direct message to nick
    std::map<std::string, int>::iterator it = _nickToFd.find(target);
    if (it == _nickToFd.end()) {
        // a user with that nick doesnt exist
        sendLine(fd, ":" + _serverName + " 401 " + c.nick + " " + target + " :No such nick");
        return;
    }

    int toFd = it->second;
    sendLine(toFd, userPrefix(c) + " PRIVMSG " + target + " :" + text);
}

void Server::handleWHO(int fd, const ParsedMessage& msg) {
    Client& c = _clients[fd];
    std::string mask = msg.params.empty() ? "*" : msg.params[0];

    // If WHO is for a channel, send 352 for each member (useful for clients)
    if (!mask.empty() && mask[0] == '#') {
        std::map<std::string, Channel>::iterator chit = _channels.find(mask);
        if (chit != _channels.end()) {
            Channel& ch = chit->second;
            for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it) {
                Client& m = _clients[*it];
                std::string mu = m.user.empty() ? "user" : m.user;
                std::string rn = m.realname.empty() ? m.nick : m.realname;

                // 352 <me> <channel> <user> <host> <server> <nick> <flags> :<hopcount> <realname>
                sendLine(fd, ":" + _serverName + " 352 " + c.nick + " " + mask + " " + mu + " localhost " + _serverName + " " + m.nick + " H :0 " + rn);
            }
        }
    }
    // End of WHO
    sendLine(fd, ":" + _serverName + " 315 " + c.nick + " " + mask + " :End of /WHO list.");
}