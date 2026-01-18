#include "IRCParser.hpp"

ParsedMessage parseLine(const std::string& line) {
    ParsedMessage msg;
    std::string temp;
    std::string trailing;
    size_t pos;

    temp = line;
    trailing = "";

    if (temp.empty())
        return msg;

    // 1) Optional prefix
    if (temp[0] == ':') {
        pos = temp.find(' ');
        if (pos == std::string::npos)
            return msg; // malformed
        msg.prefix = temp.substr(1, pos - 1);
        temp.erase(0, pos + 1);
    }

    // trim leading spaces
    while (!temp.empty() && temp[0] == ' ')
        temp.erase(0, 1);

    if (temp.empty())
        return msg;

    // 2) Optional trailing param (everything after " :")
    pos = temp.find(" :");
    if (pos != std::string::npos) {
        trailing = temp.substr(pos + 2);
        temp.erase(pos);
    }

    // 3) Split remaining by spaces: command + middle params
    pos = temp.find(' ');
    if (pos == std::string::npos) {
        msg.command = temp;
    } else {
        msg.command = temp.substr(0, pos);
        temp.erase(0, pos + 1);

        while (true) {
            while (!temp.empty() && temp[0] == ' ')
                temp.erase(0, 1);
            if (temp.empty())
                break;

            pos = temp.find(' ');
            if (pos == std::string::npos) {
                msg.params.push_back(temp);
                break;
            }
            msg.params.push_back(temp.substr(0, pos));
            temp.erase(0, pos + 1);
        }
    }

    // 4) Add trailing as last param if present
    if (!trailing.empty())
        msg.params.push_back(trailing);

    return msg;
}