#include "ClientPrediction.h"
#include "Logger.h"
#include "NetworkClient.h"
#include "test_utils.h"

TEST(ClientPrediction_ColorSyncDuringReconciliation) {
  resetEventBus();
  NetworkClient client;
  ClientPrediction prediction(&client, 1, 800.0f, 600.0f, nullptr);

  PlayerState serverState{1, 100.0f, 100.0f, 0.0f, 0.0f, 100.0f, 255, 0, 0, 5};
  publishStateUpdate(1, {serverState});

  const Player& localPlayer = prediction.getLocalPlayer();
  assert(localPlayer.r == 255);
  assert(localPlayer.g == 0);
  assert(localPlayer.b == 0);
}

TEST(ClientPrediction_ColorPersistsAcrossMultipleReconciles) {
  resetEventBus();
  NetworkClient client;
  ClientPrediction prediction(&client, 1, 800.0f, 600.0f, nullptr);

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

int main() {
  Logger::init();

  test_ClientPrediction_ColorSyncDuringReconciliation();
  test_ClientPrediction_ColorPersistsAcrossMultipleReconciles();

  return 0;
}
