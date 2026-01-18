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

class Server {
    private:
        struct ParsedMessage {
            std::string prefix;
            std::string command;
            std::vector<std::string> params;
        };

    ParsedMessage parseLine(const std::string& line);
    private:
        int _port;
        int _listenFd;
        std::string _password;
        std::vector<pollfd> _pollFDs; //poll list (listen fd + client fds)

        // _inbuf is an dict where key<fd where we take message> <string message>;
        std::map<int, std::string> _inbuf;

    public:
        Server(int port, const std::string& password);
        ~Server();

        //Public functionality
        void run();

    private:
        Server(const Server&);
        Server& operator=(const Server&); 

        //Private functionality
        void setupListeningSocket();
        void setNonBlocking(int fd);
        void acceptNewClients();

        void handleClientRead(int pollFdInd);
        void disconnectClient(int pollFDInd);
};

#endif
