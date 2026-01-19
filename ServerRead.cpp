
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

// // is similar to fileprivate in Swift
// // anonymous namespace for helper functions only dont call class func inside
// namespace {
//     // Extract complete lines and process them
//     bool getNewLine(std::string& buf, std::string& line) {
//         size_t pos = buf.find("\r\n");
//         if (pos == std::string::npos)
//             return false;
//         line = buf.substr(0, pos);
//         buf.erase(0, pos + 2);
//         return true;
//     }
// }

void Server::handleClientRead(int indOfPoll) {
    const int fd = _pollFDs[indOfPoll].fd;
    std::string& clientBuf = _inbuf[fd];

    // Read until EAGAIN/EWOULDBLOCK (non-blocking socket)
    char tmp[512];
    while (true) {
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);

        if (n > 0) {
            clientBuf.append(tmp, n);

            // Protect against a client sending an overlong line without "\r\n"
            // (or a tail that grows beyond the IRC limit).
            if (unfinishedLineLen(clientBuf) > 510) {
                std::cout << "Protocol violation: overlong line fd=" << fd << "\n";
                disconnectClient(indOfPoll);
                return;
            }
            continue;
        }

        if (n == 0) {
            std::cout << "Client disconnected fd=" << fd << "\n";
            disconnectClient(indOfPoll);
            return;
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

    // Parse complete IRC lines after we've drained the socket
    std::string ircLine;
    while (extractLineCRLF(clientBuf, ircLine)) {
        //DELETE THIS line later
        std::cout << "RAW <- [" << ircLine << "]\n";
        ParsedMessage parsed = parseLine(ircLine);
        onMessage(fd, parsed); 

        // TODO:
        // dispatchCommand(fd, parsed);
        // (and later, queue replies into an out-buffer + enable POLLOUT)
    }
}