#include "NetworkClient.h"
#include "Logger.h"

int main() {
    Logger::init();

    NetworkClient client;
    if (!client.initialize()) {
        return EXIT_FAILURE;
    }

    if (!client.connect("127.0.0.1", 1234)) {
        return EXIT_FAILURE;
    }

    client.send("Hello, Server!");

    client.run();

    client.disconnect();

    return EXIT_SUCCESS;
}

