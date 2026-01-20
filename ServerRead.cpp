
///
// IS part of Server.cpp file
///

#include "Server.hpp"


namespace {
    // Extract one complete IRC line (without "\r\n") from buf.
    // Returns true if a line was extracted into `line`.
    bool extractLineCRLF(std::string& buf, std::string& line) {
        const std::string delim = "\r\n";
        size_t pos = buf.find(delim);
        if (pos == std::string::npos)
            return false;

        line.assign(buf, 0, pos);
        buf.erase(0, pos + delim.size());
        return true;
    }

    // IRC limit applies to ONE command line (excluding "\r\n").
    // We enforce it on the *unfinished tail* (after the last CRLF).
    size_t unfinishedLineLen(const std::string& buf) {
        const std::string delim = "\r\n";
        size_t pos = buf.rfind(delim);
        if (pos == std::string::npos)
            return buf.size(); // no complete line yet -> whole buffer is unfinished line
        return buf.size() - (pos + delim.size());
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

            // Protect against a client sending an overlong line without "\r\n"
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

        // Strip optional '\r'
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
