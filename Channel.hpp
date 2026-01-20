#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>

struct Channel {
    std::string name;
    std::set<int> members; // store client fds
    std::set<int> operators;
    std::set<int> invited;
    std::string topic;

    // modes
    bool inviteOnly;     // +i
    bool topicOpsOnly;   // +t
    bool hasKey;         // +k
    std::string key;
    bool hasLimit;       // +l
    size_t userLimit;

    Channel(): inviteOnly(false), 
                topicOpsOnly(false),
                hasKey(false),
                key(""),
                hasLimit(false),
                userLimit(0) {}
};

#endif
