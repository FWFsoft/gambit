#include "ClientPrediction.h"

#include <cmath>

#include "Logger.h"
#include "NetworkClient.h"

ClientPrediction::ClientPrediction(NetworkClient* client,
                                   uint32_t localPlayerId)
    : client(client), localPlayerId(localPlayerId), localInputSequence(0) {
  // Initialize local player
  localPlayer.id = localPlayerId;
  localPlayer.x = 400.0f;  // Center of screen (will be updated by server)
  localPlayer.y = 300.0f;
  localPlayer.vx = 0.0f;
  localPlayer.vy = 0.0f;
  localPlayer.health = 100.0f;
  localPlayer.r = 255;
  localPlayer.g = 255;
  localPlayer.b = 255;

  // Subscribe to local input events
  EventBus::instance().subscribe<LocalInputEvent>(
      [this](const LocalInputEvent& e) { onLocalInput(e); });

  // Subscribe to network packet events
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) {
        onNetworkPacketReceived(e);
      });
}

void ClientPrediction::onLocalInput(const LocalInputEvent& e) {
  // Apply input immediately to local player (instant response)
  applyInput(localPlayer, e.moveLeft, e.moveRight, e.moveUp, e.moveDown,
             16.67f);

  // Store input in history for reconciliation
  LocalInputEvent inputCopy = e;
  inputCopy.inputSequence = localInputSequence++;
  inputHistory.push_back(inputCopy);

  // Keep only last 60 inputs (1 second at 60 FPS)
  if (inputHistory.size() > 60) {
    inputHistory.pop_front();
  }

  // Serialize and send to server
  ClientInputPacket packet;
  packet.inputSequence = inputCopy.inputSequence;
  packet.moveLeft = e.moveLeft;
  packet.moveRight = e.moveRight;
  packet.moveUp = e.moveUp;
  packet.moveDown = e.moveDown;

  client->send(serialize(packet));
}

void ClientPrediction::onNetworkPacketReceived(
    const NetworkPacketReceivedEvent& e) {
  if (e.size == 0) return;

  PacketType type = static_cast<PacketType>(e.data[0]);

  if (type == PacketType::StateUpdate) {
    StateUpdatePacket stateUpdate = deserializeStateUpdate(e.data, e.size);
    reconcile(stateUpdate);
  } else if (type == PacketType::PlayerJoined) {
    PlayerJoinedPacket packet = deserializePlayerJoined(e.data, e.size);

    // If this is our player, update color
    if (packet.playerId == localPlayerId) {
      localPlayer.r = packet.r;
      localPlayer.g = packet.g;
      localPlayer.b = packet.b;
      Logger::info("Local player color: " + std::to_string(packet.r) + "," +
                   std::to_string(packet.g) + "," + std::to_string(packet.b));
    }
  }
}

void ClientPrediction::reconcile(const StateUpdatePacket& stateUpdate) {
  // Find our player in the state update
  const PlayerState* serverState = nullptr;
  for (const auto& player : stateUpdate.players) {
    if (player.playerId == localPlayerId) {
      serverState = &player;
      break;
    }
  }

  if (!serverState) {
    return;  // We're not in the game yet
  }

  // Update server tick
  localPlayer.lastServerTick = stateUpdate.serverTick;

  // Calculate prediction error
  float dx = localPlayer.x - serverState->x;
  float dy = localPlayer.y - serverState->y;
  float error = std::sqrt(dx * dx + dy * dy);

  // Tiger Style: Warn if prediction error is large
  if (error > 50.0f) {
    Logger::info("Large prediction error: " + std::to_string(error) +
                 " pixels");
  }

  // Rewind to server state
  localPlayer.x = serverState->x;
  localPlayer.y = serverState->y;
  localPlayer.vx = serverState->vx;
  localPlayer.vy = serverState->vy;
  localPlayer.health = serverState->health;

  // Find inputs that happened AFTER the server state
  // (These are inputs the server hasn't processed yet)
  std::deque<LocalInputEvent> inputsToReplay;
  for (const auto& input : inputHistory) {
    // Note: We're comparing input sequence with server tick
    // This assumes server processes one input per tick
    if (input.inputSequence > stateUpdate.serverTick) {
      inputsToReplay.push_back(input);
    }
  }

  // Re-apply all inputs that happened after the server state
  for (const auto& input : inputsToReplay) {
    applyInput(localPlayer, input.moveLeft, input.moveRight, input.moveUp,
               input.moveDown, 16.67f);
  }

  // Remove old inputs from history (server has processed these)
  while (!inputHistory.empty() &&
         inputHistory.front().inputSequence <= stateUpdate.serverTick) {
    inputHistory.pop_front();
  }
}
