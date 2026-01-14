#include "Server.hpp"

#include <iostream>
#include <cstdlib>

static bool isAllDigits(const char* s) {
    if (!s || !*s) 
        return false;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9')
            return false;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Check your arguments!\n";
        return 1;
    }

    if (!isAllDigits(argv[1])) {
        std::cerr << "Error: port must be a number\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "Error: port must be in range 1..65535\n";
        return 1;
    }

    std::string password(argv[2]);
    if (password.empty()) {
        std::cerr << "Error: password must not be empty\n";
        return 1;
    }

    Server server(port, password);
    server.run();
    return 0;
}
