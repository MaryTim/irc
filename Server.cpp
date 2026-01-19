#include "Server.hpp"

Server::Server(int port, const std::string& password)
    :_port(port), 
    _listenFd(-1),
    _password(password), 
    _serverName("ircserv") {
    setupListeningSocket();
}

Server::~Server() {
    for (size_t i = 1; i < _pollFDs.size(); i++) {
        if (_pollFDs[i].fd >= 0)
            close(_pollFDs[i].fd);
    }
    _pollFDs.clear();
    _clients.clear();
    _inbuf.clear();
    _nickToFd.clear();

    if (_listenFd != -1)
        close(_listenFd);
}

void Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "fcntl(F_GETFL) failed: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "fcntl(F_SETFL, O_NONBLOCK) failed: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }
}

void Server::setupListeningSocket() {
    // create endpoint
    _listenFd = socket(AF_INET, SOCK_STREAM, 0); //IPv4, TCP, default protocol
    if (_listenFd < 0) {
        std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }

    // configure reuse
    int yes = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
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
        std::exit(EXIT_FAILURE);
    }

    // enable incoming connection
    if (listen(_listenFd, SOMAXCONN) < 0) {
        std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    }

    setNonBlocking(_listenFd);

    std::cout << "Listening on port " << _port << " (fd=" << _listenFd << ")\n";
}

void Server::acceptNewClients() {
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

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

        //add client fd to poll list
        pollfd clientsPollFd;
        clientsPollFd.fd = clientFd;
        clientsPollFd.events = POLLIN;
        clientsPollFd.revents = 0;

        this->_pollFDs.push_back(clientsPollFd);
        // Ensure client state exists immediately
        Client& c = _clients[clientFd];
        c.fd = clientFd;
        // Ensure input buffer entry exists too
        _inbuf[clientFd] = "";
        std::cout << "server added new pollFd PollFDs.size = " << _pollFDs.size() << "\n";
    }
}


void Server::disconnectClient(int pollFDInd) {
    if (pollFDInd == 0)
        return;
    int fd = _pollFDs[pollFDInd].fd;
    for (std::map<std::string, Channel>::iterator itc = _channels.begin(); itc != _channels.end(); ) {
        itc->second.members.erase(fd);

        if (itc->second.members.empty()) {
            std::map<std::string, Channel>::iterator dead = itc;
            ++itc;
            _channels.erase(dead);
        } else {
            ++itc;
        }
    }
    // Clean nick mapping if any
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        if (it->second.hasNick) {
            std::map<std::string, int>::iterator nit = _nickToFd.find(it->second.nick);
            if (nit != _nickToFd.end() && nit->second == fd)
                _nickToFd.erase(nit);
        }
        _clients.erase(it);
    }

    _inbuf.erase(fd);
    if (fd >= 0)
        close(fd);
    _pollFDs.erase(_pollFDs.begin() + pollFDInd);
}

void Server::run() {
    pollfd listeningPollFd;
    listeningPollFd.fd = _listenFd;
    listeningPollFd.events = POLLIN;
    listeningPollFd.revents = 0;
    this->_pollFDs.push_back(listeningPollFd);

    // Listening for new connections
    while (true) {
        //ret == number of fds with events
        int ret = poll(&_pollFDs[0], _pollFDs.size(), -1); // watch 1 fd
        if (ret < 0) {
            if (errno == EINTR)
                continue; // interrupted by signal, retry
            std::cerr << "poll() failed: " << std::strerror(errno) << "\n";
            break;
        }

        if (_pollFDs[0].revents & POLLIN) {
            acceptNewClients();
            // clear revents
            _pollFDs[0].revents = 0;
            ret -= 1;
        } 

        size_t i = 1;
        while (i < _pollFDs.size() && ret > 0) {
            //skip current pollFD if no events in it
            short currentPollFDEvents = _pollFDs[i].revents;
            if (currentPollFDEvents == 0) {
                ++i;
                continue;
            }

            --ret;

            // POLLHUP (client disconnected) ==  client closed TCP connection
    	    // POLLERR (socket error)        ==  broken socket/connection reset
            // POLLNVAL (invalid fd)         ==  fd was already closed
            if (currentPollFDEvents & (POLLHUP | POLLERR | POLLNVAL)) {
                disconnectClient(i);
                continue;
            }
            if (currentPollFDEvents & POLLIN) {
                handleClientRead(i);
                continue;
            }
            if (currentPollFDEvents & POLLOUT) {
                //TODO:
                // send();
            }
            ++i;
        }
    }
}
