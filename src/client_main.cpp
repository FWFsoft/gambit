#include "NetworkClient.h"
#include "Logger.h"
#include "Window.h"
#include "EventBus.h"
#include "GameLoop.h"
#include "InputSystem.h"

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
    GameLoop gameLoop;
    InputSystem inputSystem;

    // Subscribe to UpdateEvent for network processing
    EventBus::instance().subscribe<UpdateEvent>([&](const UpdateEvent& e) {
        // Poll window events
        window.pollEvents();

        // Process network events
        client.run();

        // Stop the game loop if window is closed
        if (!window.isOpen()) {
            gameLoop.stop();
        }

        // Log every 60 frames (once per second) to verify 60 FPS
        if (e.frameNumber % 60 == 0) {
            Logger::info("Frame: " + std::to_string(e.frameNumber) +
                        " (deltaTime: " + std::to_string(e.deltaTime) + "ms)");
        }
    });

    // Subscribe to RenderEvent for drawing
    EventBus::instance().subscribe<RenderEvent>([&](const RenderEvent& e) {
        SDL_SetRenderDrawColor(window.getRenderer(), 0, 0, 0, 255);
        SDL_RenderClear(window.getRenderer());

        // Render game objects
        // ...

        SDL_RenderPresent(window.getRenderer());
    });

    // Subscribe to LocalInputEvent for testing (log when any input changes)
    bool lastInputState = false;
    EventBus::instance().subscribe<LocalInputEvent>([&](const LocalInputEvent& e) {
        bool hasInput = e.moveLeft || e.moveRight || e.moveUp || e.moveDown;
        if (hasInput && !lastInputState) {
            Logger::info("Input active - L:" + std::to_string(e.moveLeft) +
                        " R:" + std::to_string(e.moveRight) +
                        " U:" + std::to_string(e.moveUp) +
                        " D:" + std::to_string(e.moveDown) +
                        " Seq:" + std::to_string(e.inputSequence));
        }
        lastInputState = hasInput;
    });

    // Run the game loop
    gameLoop.run();

    client.send("Window closed, disconnecting from Gambit Server!");
    client.disconnect();

    return EXIT_SUCCESS;
}

