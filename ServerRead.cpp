
///
// IS part of Server.cpp file
///

#include "Server.hpp"

namespace {
    // IRC limit applies to ONE command line (excluding line ending).
    // Enforce it on the *unfinished tail* after the last '\n'
    // (we accept both "\r\n" and "\n" as line terminators).
    size_t unfinishedLineLen(const std::string& buf) {
        size_t pos = buf.rfind('\n');
        if (pos == std::string::npos)
            return buf.size();          // no complete line yet -> whole buffer is unfinished
        return buf.size() - (pos + 1);  // bytes after last '\n'
    }
}

void Server::handleClientRead(int indOfPoll) {
    const int fd = _pollFDs[indOfPoll].fd;

    // Get/ensure buffer entry (better than keeping a reference forever)
    std::map<int, std::string>::iterator bit = _inbuf.find(fd);
    if (bit == _inbuf.end())
        bit = _inbuf.insert(std::make_pair(fd, std::string())).first;

    bool peerClosed = false;

    char tmp[512];
    while (true) {
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);

        if (n > 0) {
            // Re-find each time in case fd was disconnected elsewhere (paranoia-safe)
            bit = _inbuf.find(fd);
            if (bit == _inbuf.end())
                return; // disconnected

            bit->second.append(tmp, n);

            if (unfinishedLineLen(bit->second) > 510) {
                std::cout << "Protocol violation: overlong line fd=" << fd << "\n";
                int idx = findPollIndexByFd(fd);
                if (idx != -1)
                    disconnectClient(idx);
                return;
            }
            continue;
        }

        if (n == 0) {
            peerClosed = true;
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

        std::cerr << "recv() failed fd=" << fd << ": " << std::strerror(errno) << "\n";
        int idx = findPollIndexByFd(fd);
        if (idx != -1)
            disconnectClient(idx);
        return;
    }

    // Parse full lines
    while (true) {
        bit = _inbuf.find(fd);
        if (bit == _inbuf.end())
            return; // disconnected during command handling

        std::string& buf = bit->second;

        size_t nl = buf.find('\n');
        if (nl == std::string::npos)
            break;

        std::string line = buf.substr(0, nl);
        buf.erase(0, nl + 1);

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        std::cout << "RAW <- [" << line << "]\n";

        ParsedMessage msg = parseLine(line);
        if (msg.command.empty())
            continue;

        onMessage(indOfPoll, fd, msg);

        // If QUIT (or any handler) disconnected the client, stop immediately
        if (_clients.find(fd) == _clients.end())
            return;
    }

    if (peerClosed) {
        std::cout << "Client disconnected fd=" << fd << "\n";
        int idx = findPollIndexByFd(fd);
        if (idx != -1)
            disconnectClient(idx);
    }
}