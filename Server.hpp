#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <cerrno>
#include <cstring>

#include <unistd.h> 
#include <fcntl.h> 
#include <poll.h>
#include <sys/socket.h> 
#include <netinet/in.h>

#include <string>
#include <vector>
#include <map>

#include "IRCParser.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "ModeResult.hpp"

class Server {
    public:
        Server(int port, const std::string& password);
        ~Server();
        void run();

    private:
        Server(const Server&);
        Server& operator=(const Server&); 

        int _port;
        int _listenFd;
        std::string _password;
        std::string _serverName;
        std::vector<pollfd> _pollFDs; // poll list (index 0 = listen fd, index 1..N = clients)
        std::map<int, Client> _clients;
        std::map<int, std::string> _inbuf; // _inbuf is a dict where key<fd where we take message> <string message>;
        std::map<std::string, int> _nickToFd; // nick -> fd (for uniqueness checks)
        std::map<std::string, Channel> _channels;

        void setupListeningSocket();
        void setNonBlocking(int fd);
        void acceptNewClients();

        void handleClientRead(int pollFdInd);
        void disconnectClient(int pollFDInd);
        void disconnectClientByFd(int fd);
        void sendLine(int fd, const std::string& line);
        void onMessage(int fd, const ParsedMessage& msg);
        void tryRegister(int fd);

        // Handlers
        void handleCAP(int fd, const ParsedMessage& msg);
        void handlePASS(int fd, const ParsedMessage& msg);
        void handleNICK(int fd, const ParsedMessage& msg);
        void handleUSER(int fd, const ParsedMessage& msg);
        void handlePING(int fd, const ParsedMessage& msg);
        void handleJOIN(int fd, const ParsedMessage& msg);
        void handlePRIVMSG(int fd, const ParsedMessage& msg);
        void handleMODE(int fd, const ParsedMessage& msg);
        void handleWHO(int fd, const ParsedMessage& msg);
        void handleQUIT(int fd, const ParsedMessage& msg);
        void handleTOPIC(int fd, const ParsedMessage& msg);

        // work with modes
        ModeResult applyChannelModeChanges(int fd, Channel& ch, const ParsedMessage& msg);
        std::string makeModeBroadcastLine(int fd,
                                      const std::string& chan,
                                      const std::string& modeStr,
                                      const std::vector<std::string>& modeParams);

        static void appendModeChar(std::string& out, char& currentOutSign, bool adding, char modeChar);
        static bool parsePositiveSizeT(const std::string& s, size_t& out);

        int findFdByNick(const std::string& nick) const;

        //helpers
        std::string toUpper(std::string s);
        std::string userPrefix(const Client& c);
        bool isChannelOperator(const Channel& ch, int fd) const;
        void broadcastToChannel(const Channel& ch, const std::string& line, int exceptFd);
        int findPollIndexByFd(int fd) const;
        std::string nickOf(int fd) const;
};

#endif
