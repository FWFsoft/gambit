#include "NetworkClient.h"
#include "Logger.h"
#include "Window.h"

int main() {
    Logger::init();

    NetworkClient client;
    if (!client.initialize()) {
        return EXIT_FAILURE;
    }

    if (!client.connect("127.0.0.1", 1234)) {
        return EXIT_FAILURE;
    }

    Window window("Gambit Client", 800, 600);

    // Main loop
    while (window.isOpen()) {
        // Poll window events
        window.pollEvents();

        // Process network events
        client.run();

        // Clear the window
        SDL_SetRenderDrawColor(window.getRenderer(), 0, 0, 0, 255); // Black background
        SDL_RenderClear(window.getRenderer());

        // Render game objects
        // ...

        // Present the rendered frame
        SDL_RenderPresent(window.getRenderer());

        // Small delay to cap the frame rate
        SDL_Delay(16); // Approximately 60 FPS
    }

    client.send("Window closed, disconnecting from Gambit Server!");
    
    client.disconnect();

    return EXIT_SUCCESS;
}

