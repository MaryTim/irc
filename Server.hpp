#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>

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

private:
    int _port;
    std::string _password;
    int _listenFd;
};

#endif
