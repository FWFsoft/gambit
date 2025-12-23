#pragma once

#include "Player.h"
#include "EventBus.h"
#include "NetworkProtocol.h"
#include <deque>

class NetworkClient;

class ClientPrediction {
public:
    ClientPrediction(NetworkClient* client, uint32_t localPlayerId);

    const Player& getLocalPlayer() const { return localPlayer; }

private:
    NetworkClient* client;
    uint32_t localPlayerId;
    Player localPlayer;

    // Input history for reconciliation (store last 60 inputs = 1 second)
    std::deque<LocalInputEvent> inputHistory;
    uint32_t localInputSequence;

    void onLocalInput(const LocalInputEvent& e);
    void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);

    void reconcile(const StateUpdatePacket& stateUpdate);
};
