
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
    std::string& clientBuf = _inbuf[fd];

    bool peerClosed = false;

    // Read until EAGAIN/EWOULDBLOCK (non-blocking socket)
    char tmp[512];
    while (true) {
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);

        if (n > 0) {
            clientBuf.append(tmp, n);

            // Protect against a client sending an overlong line without newline
            if (unfinishedLineLen(clientBuf) > 510) {
                std::cout << "Protocol violation: overlong line fd=" << fd << "\n";
                disconnectClient(indOfPoll);
                return;
            }
            continue;
        }

        if (n == 0) {
            // Peer closed the connection. We still want to process any complete lines
            // already received in clientBuf before disconnecting.
            peerClosed = true;
            break;
        }

        // n < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Socket drained for now, parse whatever full lines we have.
            break;
        }

        std::cerr << "recv() failed fd=" << fd << ": " << std::strerror(errno) << "\n";
        disconnectClient(indOfPoll);
        return;
    }

    // Parse full lines.
    // Accept both "\r\n" and "\n" (nc variants may send either; -C sends CRLF).
    while (true) {
        size_t nl = clientBuf.find('\n');
        if (nl == std::string::npos)
            break;

        std::string line = clientBuf.substr(0, nl);
        clientBuf.erase(0, nl + 1);

        // Strip optional '\r' (for CRLF)
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (line.empty())
            continue;

        std::cout << "RAW <- [" << line << "]\n";

        ParsedMessage msg = parseLine(line);
        if (msg.command.empty())
            continue;

        onMessage(fd, msg);
    }

    // After processing buffered commands, disconnect if the peer closed.
    if (peerClosed) {
        std::cout << "Client disconnected fd=" << fd << "\n";
        disconnectClient(indOfPoll);
    }
}