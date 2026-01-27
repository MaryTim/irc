#include "Server.hpp"
#include "ModeResult.hpp"
#include <sstream>

// MODE / INVITE / KICK / TOPIC

// i, t, k, o, l
// MODE <target> [modestring] [params...]
// e.g.: MODE #general +i
void Server::handleMODE(int fd, const ParsedMessage& msg) {
    // we get just MODE, we still need to reply
    if (msg.params.empty()) {
        sendLine(fd, ":" + _serverName + " 461 " + nickOf(fd) + " MODE :Not enough parameters");
        return;
    }

    const std::string& target = msg.params[0];

    // MODE #channel
    // check we work with channel, it should start with #
    if (target.empty() || target[0] != '#') {
        sendLine(fd, ":" + _serverName + " 403 " + nickOf(fd) + " " + target + " :No such channel");
        return;
    }

    // Find channel if it exists
    std::map<std::string, Channel>::iterator chit = _channels.find(target);
    if (chit == _channels.end()) {
        sendLine(fd, ":" + _serverName + " 403 " + nickOf(fd) + " " + target + " :No such channel");
        return;
    }

    Channel& ch = chit->second;

    // get current modes for a specific channel
    if (msg.params.size() == 1) { // MODE #channel - no modes
        std::string nick = nickOf(fd);
        // build mode string, it should start with +
        std::string modes = "+";
        std::vector<std::string> modeParams;

        if (ch.inviteOnly)
            modes += "i";
        if (ch.topicOpsOnly)
            modes += "t";
        if (ch.hasLimit) {
            modes += "l"; 
            std::ostringstream stringStream; // safely convert integer to string
            stringStream << ch.userLimit;
            modeParams.push_back(stringStream.str());
        }

        std::string line = ":" + _serverName + " 324 " + nick + " " + ch.name + " " + modes;
        for (size_t i = 0; i < modeParams.size(); i++)
            line += " " + modeParams[i];

        sendLine(fd, line);
        return;
    }

    // Must be operator to change modes
    if (ch.operators.count(fd) == 0) {
        std::string nick = nickOf(fd);
        sendLine(fd, ":" + _serverName + " 482 " + nick + " " + ch.name + " :You're not channel operator");
        return;
    }

    // Apply changes (+itkol and friends)
    ModeResult r = applyChannelModeChanges(fd, ch, msg);

    if (r.anyChange) {
        broadcastToChannel(ch, r.broadcastLine, -1);
    }
}


void Server::handleTOPIC(int fd, const ParsedMessage& msg) {
    std::map<int, Client>::iterator cit = _clients.find(fd);
    if (cit == _clients.end())
        return;
    Client& c = cit->second;

    if (!c.registered) {
        sendLine(fd, ":" + _serverName + " 451 * :You have not registered");
        return;
    }
    if (msg.params.empty()) {
        sendLine(fd, ":" + _serverName + " 461 " + c.nick + " TOPIC :Not enough parameters");
        return;
    }

    std::string chanName = msg.params[0];
    std::map<std::string, Channel>::iterator it = _channels.find(chanName);
    if (it == _channels.end()) {
        sendLine(fd, ":" + _serverName + " 403 " + c.nick + " " + chanName + " :No such channel");
        return;
    }
    Channel& ch = it->second;

    if (ch.members.find(fd) == ch.members.end()) {
        sendLine(fd, ":" + _serverName + " 442 " + c.nick + " " + chanName + " :You're not on that channel");
        return;
    }

    // Query topic
    if (msg.params.size() == 1) {
        if (ch.topic.empty())
            sendLine(fd, ":" + _serverName + " 331 " + c.nick + " " + chanName + " :No topic is set");
        else
            sendLine(fd, ":" + _serverName + " 332 " + c.nick + " " + chanName + " :" + ch.topic);
        return;
    }

    // Set topic
    if (ch.topicOpsOnly && !isChannelOperator(ch, fd)) {
        sendLine(fd, ":" + _serverName + " 482 " + c.nick + " " + chanName + " :You're not channel operator");
        return;
    }

    ch.topic = msg.params[1];
    std::string line = ":" + userPrefix(c) + " TOPIC " + chanName + " :" + ch.topic;
    broadcastToChannel(ch, line, -1);
}


void Server::handleINVITE(int fd, const ParsedMessage& msg)
{
    std::map<int, Client>::iterator cit = _clients.find(fd);
    if (cit == _clients.end())
        return;
    Client& inviter = cit->second;

    if (!inviter.registered) {
        sendLine(fd, ":" + _serverName + " 451 * :You have not registered");
        return;
    }

    // INVITE <nick> <#channel>
    if (msg.params.size() < 2) {
        sendLine(fd, ":" + _serverName + " 461 " + inviter.nick + " INVITE :Not enough parameters");
        return;
    }

    std::string targetNick = msg.params[0];
    std::string chanName = msg.params[1];

    // channel exists?
    std::map<std::string, Channel>::iterator chit = _channels.find(chanName);
    if (chit == _channels.end()) {
        sendLine(fd, ":" + _serverName + " 403 " + inviter.nick + " " + chanName + " :No such channel");
        return;
    }

    Channel& ch = chit->second;

    // inviter is on channel?
    if (ch.members.count(fd) == 0) {
        sendLine(fd, ":" + _serverName + " 442 " + inviter.nick + " " + chanName + " :You're not on that channel");
        return;
    }

    // in this project INVITE is operator command => require operator
    if (ch.operators.count(fd) == 0) {
        sendLine(fd, ":" + _serverName + " 482 " + inviter.nick + " " + chanName + " :You're not channel operator");
        return;
    }

    // target exists?
    int targetFd = findFdByNick(targetNick);
    if (targetFd == -1) {
        sendLine(fd, ":" + _serverName + " 401 " + inviter.nick + " " + targetNick + " :No such nick");
        return;
    }

    // target already in channel?
    if (ch.members.count(targetFd) != 0) {
        sendLine(fd, ":" + _serverName + " 443 " + inviter.nick + " " + targetNick + " " + chanName + " :is already on channel");
        return;
    }

    // store invite (by fd)
    ch.invited.insert(targetFd);

    // notify target
    sendLine(targetFd, ":" +userPrefix(inviter) + " INVITE " + targetNick + " " + chanName);

    // notify inviter (341)
    sendLine(fd, ":" + _serverName + " 341 " + inviter.nick + " " + targetNick + " " + chanName);
}

void Server::handleKICK(int fd, const ParsedMessage& msg)
{
    std::map<int, Client>::iterator cit = _clients.find(fd);
    if (cit == _clients.end())
        return;
    Client& kicker = cit->second;

    if (!kicker.registered) {
        sendLine(fd, ":" + _serverName + " 451 * :You have not registered");
        return;
    }

    // KICK <#channel> <nick> [reason]
    if (msg.params.size() < 2) {
        sendLine(fd, ":" + _serverName + " 461 " + kicker.nick + " KICK :Not enough parameters");
        return;
    }

    std::string chanName = msg.params[0];
    std::string targetNick = msg.params[1];

    std::string reason = "Kicked";
    if (msg.params.size() >= 3)
        reason = msg.params[2];

    // channel exists?
    std::map<std::string, Channel>::iterator chit = _channels.find(chanName);
    if (chit == _channels.end()) {
        sendLine(fd, ":" + _serverName + " 403 " + kicker.nick + " " + chanName + " :No such channel");
        return;
    }

    Channel& ch = chit->second;

    // kicker is on channel?
    if (ch.members.count(fd) == 0) {
        sendLine(fd, ":" + _serverName + " 442 " + kicker.nick + " " + chanName + " :You're not on that channel");
        return;
    }

    // operator only
    if (ch.operators.count(fd) == 0) {
        sendLine(fd, ":" + _serverName + " 482 " + kicker.nick + " " + chanName + " :You're not channel operator");
        return;
    }

    // target exists?
    int targetFd = findFdByNick(targetNick);
    if (targetFd == -1) {
        sendLine(fd, ":" + _serverName + " 401 " + kicker.nick + " " + targetNick + " :No such nick");
        return;
    }

    // target is on channel?
    if (ch.members.count(targetFd) == 0) {
        sendLine(fd, ":" + _serverName + " 441 " + kicker.nick + " " + targetNick + " " + chanName + " :They aren't on that channel");
        return;
    }

    // build KICK message
    std::string kickLine = ":" + userPrefix(kicker) + " KICK " + chanName + " " + targetNick + " :" + reason;

    // broadcast to channel (including target)
    std::set<int>::iterator it = ch.members.begin();
    while (it != ch.members.end()) {
        sendLine(*it, kickLine);
        ++it;
    }

    // remove target from channel
    ch.members.erase(targetFd);
    ch.operators.erase(targetFd);
    ch.invited.erase(targetFd);

    // optional: delete empty channel
    if (ch.members.empty())
        _channels.erase(chit);
}