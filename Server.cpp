#include "Server.hpp"
#include "ModeResult.hpp"
#include <csignal>
#include <cerrno>

// global flag. volatile - "this value can change unexpectedly”
// when we receive SIGINT, we flip a flag, main loop checks it later
static volatile sig_atomic_t g_stop = 0;

static void onSigInt(int) {
    g_stop = 1;
}

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

int Server::findPollIndexByFd(int fd) const {
    for (size_t i = 0; i < _pollFDs.size(); i++) {
        if (_pollFDs[i].fd == fd)
            return static_cast<int>(i);
    }
    return -1;
}

std::string Server::nickOf(int fd) const {
    std::map<int, Client>::const_iterator it = _clients.find(fd);
    if (it != _clients.end() && !it->second.nick.empty())
        return it->second.nick;
    return "*";
}

int Server::findFdByNick(const std::string& nick) const {
    std::map<std::string, int>::const_iterator it = _nickToFd.find(nick);
    if (it == _nickToFd.end())
        return -1;
    return it->second;
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

    // Broadcast QUIT if user is known
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it != _clients.end() && it->second.hasNick) {
        std::string quitLine = ":" + userPrefix(it->second) + " QUIT :Client Quit";

        for (std::map<std::string, Channel>::iterator itc = _channels.begin();
             itc != _channels.end(); ++itc) {

            Channel& ch = itc->second;
            if (ch.members.count(fd) == 0)
                continue;

            for (std::set<int>::iterator mit = ch.members.begin();
                 mit != ch.members.end(); ++mit) {

                if (*mit != fd)
                    sendLine(*mit, quitLine);
            }
        }
    }

    // Remove from channels
    for (std::map<std::string, Channel>::iterator itc = _channels.begin(); itc != _channels.end(); ) {
        Channel& ch = itc->second;

        ch.members.erase(fd);
        ch.operators.erase(fd);
        ch.invited.erase(fd);

        // If channel still has members but no operators, promote one.
        if (!ch.members.empty() && ch.operators.empty()) {
            int newOpFd = *ch.members.begin();

            ch.operators.insert(newOpFd);

            // Optional: broadcast MODE +o nick from server
            std::map<int, Client>::iterator nit = _clients.find(newOpFd);
            if (nit != _clients.end()) {
                std::string modeLine = ":" + _serverName + " MODE " + ch.name + " +o " + nit->second.nick;
                for (std::set<int>::iterator mit = ch.members.begin(); mit != ch.members.end(); ++mit)
                    sendLine(*mit, modeLine);
            }
        }

        if (ch.members.empty()) {
            std::map<std::string, Channel>::iterator dead = itc;
            ++itc;
            _channels.erase(dead);
        } else {
            ++itc;
        }
    }

    // Clean nick map + client
    if (it != _clients.end()) {
        if (it->second.hasNick)
            _nickToFd.erase(it->second.nick);
        _clients.erase(it);
    }

    _inbuf.erase(fd);
    close(fd);
    _pollFDs.erase(_pollFDs.begin() + pollFDInd);
}

void Server::run() {
    std::signal(SIGINT, onSigInt);

    pollfd listeningPollFd;
    listeningPollFd.fd = _listenFd;
    listeningPollFd.events = POLLIN;
    listeningPollFd.revents = 0;
    _pollFDs.push_back(listeningPollFd);

    while (!g_stop) {
        // every sec loop check g_stop
        int ret = poll(&_pollFDs[0], _pollFDs.size(), 1000);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            std::cerr << "poll() failed: " << std::strerror(errno) << "\n";
            break;
        }
        if (ret == 0)
            continue; // timeout, re-check g_stop
        if (_pollFDs[0].revents & POLLIN) {
            acceptNewClients();
            _pollFDs[0].revents = 0;
            --ret;
        }

        size_t i = 1;
        while (i < _pollFDs.size() && ret > 0) {
            short re = _pollFDs[i].revents;
            if (re == 0) {
                ++i;
                continue;
            }

            int fd = _pollFDs[i].fd;

            // clear now
            _pollFDs[i].revents = 0;
            --ret;

            bool hasHangOrErr = (re & (POLLHUP | POLLERR | POLLNVAL)) != 0;

            // If readable, read first (even if HUP also set)
            if (re & POLLIN) {
                handleClientRead(static_cast<int>(i));

                // handleClientRead may have disconnected and erased index i
                if (i >= _pollFDs.size() || _pollFDs[i].fd != fd) {
                    // current index now refers to the next element; do NOT ++i
                    continue;
                }

                // still connected. If it also had HUP/ERR, disconnect now.
                if (hasHangOrErr) {
                    disconnectClient(static_cast<int>(i));
                    // disconnectClient erases i, so do NOT ++i
                    continue;
                }

                ++i;
                continue;
            }

            // Not readable; if hang/error => disconnect
            if (hasHangOrErr) {
                disconnectClient(static_cast<int>(i));
                // erased -> don't ++i
                continue;
            }
            i++;
        }
    }
}
