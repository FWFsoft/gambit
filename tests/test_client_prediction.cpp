#include <memory>

#include "ClientPrediction.h"
#include "Logger.h"
#include "NetworkClient.h"
#include "WorldConfig.h"
#include "config/ScreenConfig.h"
#include "test_utils.h"
#include "transport/ENetTransport.h"

TEST(ClientPrediction_ColorSyncDuringReconciliation) {
  resetEventBus();
  NetworkClient client(std::make_unique<ENetTransport>());
  WorldConfig world(static_cast<float>(Config::Screen::WIDTH),
                    static_cast<float>(Config::Screen::HEIGHT), nullptr);
  ClientPrediction prediction(&client, 1, world);

  PlayerState serverState{1, 100.0f, 100.0f, 0.0f, 0.0f, 100.0f, 255, 0, 0, 5};
  publishStateUpdate(1, {serverState});

  const Player& localPlayer = prediction.getLocalPlayer();
  assert(localPlayer.r == 255);
  assert(localPlayer.g == 0);
  assert(localPlayer.b == 0);
}

TEST(ClientPrediction_ColorPersistsAcrossMultipleReconciles) {
  resetEventBus();
  NetworkClient client(std::make_unique<ENetTransport>());
  WorldConfig world(static_cast<float>(Config::Screen::WIDTH),
                    static_cast<float>(Config::Screen::HEIGHT), nullptr);
  ClientPrediction prediction(&client, 1, world);

  for (uint32_t tick = 1; tick <= 5; tick++) {
    PlayerState serverState{1,      static_cast<float>(tick * 10),
                            100.0f, 0.0f,
                            0.0f,   100.0f,
                            0,      255,
                            0,      tick};
    publishStateUpdate(tick, {serverState});
  }

  const Player& localPlayer = prediction.getLocalPlayer();
  assert(localPlayer.r == 0);
  assert(localPlayer.g == 255);
  assert(localPlayer.b == 0);
}

TEST(ClientPrediction_ObjectiveStorage) {
  resetEventBus();
  NetworkClient client(std::make_unique<ENetTransport>());
  WorldConfig world(static_cast<float>(Config::Screen::WIDTH),
                    static_cast<float>(Config::Screen::HEIGHT), nullptr);
  ClientPrediction prediction(&client, 1, world);

  // Initially no objectives
  assert(prediction.getObjectives().empty());

  // Create an ObjectiveStatePacket and update
  ObjectiveStatePacket packet;
  packet.objectiveId = 1;
  packet.objectiveType = 0;   // AlienScrapyard
  packet.objectiveState = 0;  // Inactive
  packet.x = 100.0f;
  packet.y = 200.0f;
  packet.radius = 50.0f;
  packet.progress = 0.0f;
  packet.enemiesRequired = 0;
  packet.enemiesKilled = 0;

  prediction.updateObjective(packet);

  // Should have one objective now
  assert(prediction.getObjectives().size() == 1);

  const auto& objectives = prediction.getObjectives();
  auto it = objectives.find(1);
  assert(it != objectives.end());
  assert(it->second.id == 1);
  assert(it->second.type == ObjectiveType::AlienScrapyard);
  assert(it->second.state == ObjectiveState::Inactive);
  assert(it->second.x == 100.0f);
  assert(it->second.y == 200.0f);
  assert(it->second.radius == 50.0f);

  // Update to InProgress
  packet.objectiveState = 1;  // InProgress
  packet.progress = 0.5f;
  prediction.updateObjective(packet);

  assert(prediction.getObjectives().size() == 1);
  assert(objectives.at(1).state == ObjectiveState::InProgress);
  assert(objectives.at(1).progress == 0.5f);

  // Add a second objective (CaptureOutpost)
  ObjectiveStatePacket packet2;
  packet2.objectiveId = 2;
  packet2.objectiveType = 1;   // CaptureOutpost
  packet2.objectiveState = 0;  // Inactive
  packet2.x = 300.0f;
  packet2.y = 400.0f;
  packet2.radius = 100.0f;
  packet2.progress = 0.0f;
  packet2.enemiesRequired = 5;
  packet2.enemiesKilled = 0;

  prediction.updateObjective(packet2);

  assert(prediction.getObjectives().size() == 2);
  assert(objectives.at(2).type == ObjectiveType::CaptureOutpost);
  assert(objectives.at(2).enemiesRequired == 5);
}

int main() {
  Logger::init();

  test_ClientPrediction_ColorSyncDuringReconciliation();
  test_ClientPrediction_ColorPersistsAcrossMultipleReconciles();
  test_ClientPrediction_ObjectiveStorage();

  return 0;
}
