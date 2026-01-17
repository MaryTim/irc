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
public:
    Server(int port, const std::string& password);
    ~Server();

    void run();

private:
    Server(const Server&);
    Server& operator=(const Server&); 

    void setupListeningSocket();
    void setNonBlocking(int fd);
    void acceptNewClients();

    void handleClientRead(int pollFdInd);
    void disconnectClient(int pollFDInd);

private:
    int _port;
    std::string _password;
    int _listenFd;
    std::vector<pollfd> _pollFDs; //poll list (listen fd + client fds)

    // _inbuf is an dict where key<fd where we take message> <sting message>;
    std::map<int, std::string> _inbuf;
};

#endif
