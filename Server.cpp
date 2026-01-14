#include "Server.hpp"

#include <iostream>
#include <cerrno>
#include <cstring>

#include <unistd.h> 
#include <fcntl.h> 
#include <poll.h>
#include <sys/socket.h> 
#include <netinet/in.h>

Server::Server(int port, const std::string& password): _port(port), _password(password), _listenFd(-1) {
    setupListeningSocket();
}

Server::~Server() {
    if (_listenFd != -1)
        close(_listenFd);
}

void Server::setNonBlocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "fcntl(O_NONBLOCK) failed: " << std::strerror(errno) << "\n";
        // For this skeleton, treat as fatal:
        std::exit(1);
    }
}

void Server::setupListeningSocket() {
    // create endpoint
    _listenFd = socket(AF_INET, SOCK_STREAM, 0); //IPv4, TCP, default protocol
    if (_listenFd < 0) {
        std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
        std::exit(1);
    }

    // configure reuse
    int yes = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno) << "\n";
        std::exit(1);
    }

    sockaddr_in addr; // IPv4 address + port
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); //Listen on all network interfaces.
    addr.sin_port = htons(static_cast<unsigned short>(_port)); //host to network short

    // assign IP + port
    // the OS reserves that port for your program
    // clients connecting to that port will reach your server
    if (bind(_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed: " << std::strerror(errno) << "\n";
        std::exit(1);
    }

    // enable incoming connection
    if (listen(_listenFd, SOMAXCONN) < 0) {
        std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
        std::exit(1);
    }

    setNonBlocking(_listenFd);

    std::cout << "Listening on port " << _port << " (fd=" << _listenFd << ")\n";
}

void Server::acceptNewClients() {
    while (true) {
        sockaddr_in clientAddr;
        socklen_t   clientLen = sizeof(clientAddr);

        int clientFd = accept(_listenFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more pending connections right now
                break;
            }
            std::cerr << "accept() failed: " << std::strerror(errno) << "\n";
            break;
        }

        setNonBlocking(clientFd);
        std::cout << "connected fd=" << clientFd << "\n";

        // For now we are not storing clients yet.
        // Next step - to add clientFd to poll list and track per-client buffers.
    }
}

void Server::run() {
    pollfd fds[1];
    fds[0].fd = _listenFd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    while (true) {
        int ret = poll(fds, 1, -1); // watch 1 fd
        if (ret < 0) {
            if (errno == EINTR)
                continue; // interrupted by signal, retry
            std::cerr << "poll() failed: " << std::strerror(errno) << "\n";
            break;
        }

        if (fds[0].revents & POLLIN) {
            acceptNewClients();
        }

        // clear revents
        fds[0].revents = 0;
    }
}
