#ifndef IRC_PARSER_HPP
#define IRC_PARSER_HPP

#include <string>
#include <vector>

struct ParsedMessage {
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
};

ParsedMessage parseLine(const std::string& line);

#endif