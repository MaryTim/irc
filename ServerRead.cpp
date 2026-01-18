
///
// IS part of Server.cpp file
///

#include "Server.hpp"

// is similar to fileprivate in Swift
// anonymous namespace for helper functions only dont call class func inside
namespace {
    // Extract complete lines and process them
    bool getNewLine(std::string& buf, std::string& line) {
        size_t pos = buf.find("\r\n");
        if (pos == std::string::npos)
            return false;
        line = buf.substr(0, pos);
        buf.erase(0, pos + 2);
        return true;
    }
}

void Server::handleClientRead(int indOfPoll) {
    ssize_t readSize;
    std::vector<char> buf(512);
    int fd = _pollFDs[indOfPoll].fd;
    std::string &clientMessage = _inbuf[fd];

    while (true) {
        readSize = recv(fd, &buf[0], buf.size(), 0);
        if (readSize > 0) {
            clientMessage.append(&buf[0], readSize);

            // IRC protocol limits a single command line to 510 bytes INCLUDING "\r\n".
            // If the client keeps sending data without a line terminator, we must
            // protect the server from unbounded memory growth (DoS / protocol violation).
            // Once "\r\n" appears, the buffer may exceed 512 because it can contain
            // multiple valid IRC lines.
    // TODO: limit size of unfinished IRC line (tail after last "\r\n"), not just presence of delimiter
    // clientMessage == [hello world\r\n\ infinitygamno.......] - no r\n\ at the end and
    // clientMessage.size() > 512 limits
            if (clientMessage.find("\r\n") == std::string::npos && clientMessage.size() > 510) {
                disconnectClient(indOfPoll);
                return;
            }
            continue;
        }

        if (readSize == 0) {
            std::cout << "Client disconnected fd=" << fd << "\n";
            disconnectClient(indOfPoll);
            return;
        }

        // There is nothing to read, continue polling
        // Normal exec
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        std::cerr << "recv() failed: " << std::strerror(errno) << "\n";
        disconnectClient(indOfPoll);
        return;
    }

    // std::cout << clientMessage <<  "\n";
    // parse complete irc lines after we've drained the socket (EAGAIN)
    std::string ircLine = "";
    while (getNewLine(clientMessage, ircLine) == true) {
        ParsedMessage parsedObj = parseLine(ircLine);
        std::cout << "parsedObj.command: " << parsedObj.command << "\n";
    }
}