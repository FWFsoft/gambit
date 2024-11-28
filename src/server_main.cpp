#include "NetworkServer.h"
#include "Logger.h"

int main() {
    Logger::init();

    NetworkServer server("0.0.0.0", 1234);
    if (!server.initialize()) {
        return EXIT_FAILURE;
    }

    server.run();

    return EXIT_SUCCESS;
}

