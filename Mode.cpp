#include "ModeResult.hpp"
#include "Server.hpp"

std::string Server::makeModeBroadcastLine(int fd,
                                         const std::string& chan,
                                         const std::string& modeStr,
                                         const std::vector<std::string>& modeParams) {
    if (modeStr.empty())
        return "";
    std::string prefix = "*";
    std::map<int, Client>::const_iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        prefix = userPrefix(it->second);
    }
    std::string line = ":" + prefix + " MODE " + chan + " " + modeStr;
    for (size_t i = 0; i < modeParams.size(); i++)
        line += " " + modeParams[i];
    return line;
}

bool Server::parsePositiveSizeT(const std::string& s, size_t& out) {
    if (s.empty())
        return false;

    // Reject leading '+' or '-' (we only want digits)
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9')
            return false;
    }
    // Convert with overflow check
    // size_t max = (size_t)-1;  // max value of size_t
    size_t value = 0;
    for (size_t i = 0; i < s.size(); i++) {
        size_t digit = static_cast<size_t>(s[i] - '0');

        // overflow check: value*10 + digit <= max
        if (value > ((size_t)-1 - digit) / 10)
            return false;
        value = value * 10 + digit;
    }

    out = value;
    return true;
}

ModeResult Server::applyChannelModeChanges(int fd, Channel& ch, const ParsedMessage& msg) {
    ModeResult res;

    // msg.params: [0]=#chan, [1] = mode string, [2..] = mode params
    if (msg.params.size() < 2) {
        // Not enough params for a "change" call (MODE #channel)
        std::string nick = nickOf(fd);
        sendLine(fd, ":" + _serverName + " 461 " + nick + " MODE :Not enough parameters");
        return res;
    }

    char currentOutSign = 0;
    const std::string& modeStr = msg.params[1]; // e.g "+i"

    bool adding = true; // current sign (+ or -)
    size_t argi = 2; // index of next extra parameter in msg.params (e.g "i")

    for (size_t i = 0; i < modeStr.size(); i++) {
        char m = modeStr[i];
        if (m == '+') { 
            adding = true;
            continue;
        }
        if (m == '-') {
            adding = false;
            continue;
        }
        // i: invite-only
        if (m == 'i') {
            if (ch.inviteOnly != adding) {
                ch.inviteOnly = adding;
                appendModeChar(res.appliedModes, currentOutSign, adding, 'i');
                res.anyChange = true;
            }
        }
        // t: topic restricted to ops
        else if (m == 't') {
            if (ch.topicOpsOnly != adding) {
                ch.topicOpsOnly = adding;
                appendModeChar(res.appliedModes, currentOutSign, adding, 't');
                res.anyChange = true;
            }
        }
        // k: key 
        else if (m == 'k') {
            // +k
            if (adding) {
                // +k requires key afterwards, just "+k" is unacceptable
                if (argi >= msg.params.size()) {
                    std::string nick = nickOf(fd);
                    sendLine(fd, ":" + _serverName + " 461 " + nick + " MODE :Not enough parameters");
                    break;
                }

                const std::string& newKey = msg.params[argi++];
                bool changed = (!ch.hasKey || ch.key != newKey);
                ch.hasKey = true;
                ch.key = newKey;

                if (changed) {
                    appendModeChar(res.appliedModes, currentOutSign, true, 'k');
                    res.anyChange = true;
                }
            // -k
            } else {
                if (ch.hasKey) {
                    ch.hasKey = false;
                    ch.key.clear();
                    appendModeChar(res.appliedModes, currentOutSign, false, 'k');
                    res.anyChange = true;
                }
            }
        }
        // l: user limit
        else if (m == 'l') {
            // +l needs a param
            if (adding) {
                if (argi >= msg.params.size()) {
                    std::string nick = nickOf(fd);
                    sendLine(fd, ":" + _serverName + " 461 " + nick + " MODE :Not enough parameters");
                    break;
                }

                const std::string& limStr = msg.params[argi++];
                size_t lim = 0;
                if (!parsePositiveSizeT(limStr, lim)) {
                    std::string nick = nickOf(fd);
                    sendLine(fd, ":" + _serverName + " 461 " + nick + " MODE :Invalid limit");
                    continue;
                }

                bool changed = (!ch.hasLimit || ch.userLimit != lim);
                ch.hasLimit = true;
                ch.userLimit = lim;
                if (changed) {
                    appendModeChar(res.appliedModes, currentOutSign, true, 'l');
                    res.modeParams.push_back(limStr);
                    res.anyChange = true;
                }
            // l-
            } else {
                if (ch.hasLimit) {
                    ch.hasLimit = false;
                    ch.userLimit = 0;
                    appendModeChar(res.appliedModes, currentOutSign, false, 'l');
                    res.anyChange = true;
                }
            }
        }
        // o: give/take operator
        else if (m == 'o') {
            if (argi >= msg.params.size()) {
                std::string nick = nickOf(fd);
                sendLine(fd, ":" + _serverName + " 461 " + nick + " MODE :Not enough parameters");
                break;
            }

            const std::string& nickArg = msg.params[argi++];
            int targetFd = findFdByNick(nickArg);
            // no such nick
            if (targetFd < 0) {
                std::string nick = nickOf(fd);
                sendLine(fd, ":" + _serverName + " 401 " + nick + " " + nickArg + " :No such nick/channel");
                continue;
            }
            // no such nick in the channel
            if (ch.members.count(targetFd) == 0) {
                std::string nick = nickOf(fd);
                sendLine(fd, ":" + _serverName + " 441 " + nick + " " + nickArg + " " + ch.name + " :They aren't on that channel");
                continue;
            }

            bool changed = false;
            if (adding)
            /*
            .first → iterator to the element
            .second → true if insertion happened
            .second → false if element already existed
            */
                changed = ch.operators.insert(targetFd).second;
            else
                changed = (ch.operators.erase(targetFd) > 0); // returns number of el-s removed

            if (changed) {
                appendModeChar(res.appliedModes, currentOutSign, adding, 'o'); 
                res.modeParams.push_back(nickArg);
                res.anyChange = true;
            }
        }
        // unknown mode char
        else {
            std::string nick = nickOf(fd);
            std::string mc(1, m); // to treat it like "a" not 'a'
            sendLine(fd, ":" + _serverName + " 472 " + nick + " " + mc + " :is unknown mode char to me");
            // keep going
        }
    }
    if (res.anyChange) {
        res.broadcastLine = makeModeBroadcastLine(fd, ch.name, res.appliedModes, res.modeParams);
    }
    return res;
}

void Server::appendModeChar(std::string& outModes, char& currentOutSign, bool adding, char mode) {
    char sign = adding ? '+' : '-';
    // If empty OR sign changed, add the sign
    if (outModes.empty() || currentOutSign != sign) {
        outModes.push_back(sign);
        currentOutSign = sign;
    }
    outModes.push_back(mode);
}