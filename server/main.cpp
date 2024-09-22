#include "server.hpp"

int main() {
    ChatServer server;
    server.run(9002);
    return 0;
}