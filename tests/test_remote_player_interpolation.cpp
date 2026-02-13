#include "Logger.h"
#include "RemotePlayerInterpolation.h"
#include "test_utils.h"

TEST(RemotePlayerInterpolation_PlayerLifecycle) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);

  publishPlayerJoined(1, 0, 0, 255);
  assert(interpolation.getRemotePlayerIds().size() == 0);

  publishPlayerJoined(2, 255, 0, 0);
  assert(interpolation.getRemotePlayerIds().size() == 1);
  assert(interpolation.getRemotePlayerIds()[0] == 2);

  PlayerLeftPacket leftPacket;
  leftPacket.playerId = 2;
  auto leftSerialized = serialize(leftPacket);
  EventBus::instance().publish(NetworkPacketReceivedEvent{
      0, leftSerialized.data(), leftSerialized.size()});
  assert(interpolation.getRemotePlayerIds().size() == 0);
}

TEST(RemotePlayerInterpolation_SkipLocalPlayer) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);

  publishPlayerJoined(1, 255, 0, 0);
  assert(interpolation.getRemotePlayerIds().size() == 0);

  PlayerState ps{1, 100.0f, 100.0f, 0.0f, 0.0f, 100.0f, 255, 0, 0, 0};
  publishStateUpdate(1, {ps});
  assert(interpolation.getRemotePlayerIds().size() == 0);
}

TEST(RemotePlayerInterpolation_Interpolation) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);

  publishPlayerJoined(1, 0, 0, 255);
  publishPlayerJoined(2, 255, 0, 0);

  Player player;
  assert(interpolation.getInterpolatedState(2, 0.5f, player));
  assert(player.id == 2);

  PlayerState ps1{2, 100.0f, 200.0f, 10.0f, 20.0f, 90.0f, 255, 0, 0, 0};
  publishStateUpdate(1, {ps1});
  interpolation.getInterpolatedState(2, 0.5f, player);
  assert(floatEqual(player.x, 100.0f));
  assert(floatEqual(player.y, 200.0f));

  PlayerState ps2{2, 200.0f, 400.0f, 20.0f, 40.0f, 80.0f, 255, 0, 0, 0};
  publishStateUpdate(2, {ps2});

  struct InterpolationTest {
    float t;
    float expectedX;
    float expectedY;
  };

  InterpolationTest tests[] = {
      {0.0f, 100.0f, 200.0f}, {0.5f, 150.0f, 300.0f}, {1.0f, 200.0f, 400.0f}};

  for (const auto& test : tests) {
    interpolation.getInterpolatedState(2, test.t, player);
    assert(floatEqual(player.x, test.expectedX));
    assert(floatEqual(player.y, test.expectedY));
  }
}

TEST(RemotePlayerInterpolation_MultipleRemotePlayers) {
  resetEventBus();
  RemotePlayerInterpolation interpolation(999);

  publishPlayerJoined(1, 0, 0, 255);

  for (uint32_t id = 2; id <= 4; id++) {
    publishPlayerJoined(id, static_cast<uint8_t>(id * 50), 0, 0);
  }

  assert(interpolation.getRemotePlayerIds().size() == 3);

  std::vector<PlayerState> states;
  for (uint32_t id = 2; id <= 4; id++) {
    PlayerState ps{id,
                   float(id * 100),
                   float(id * 100),
                   0.0f,
                   0.0f,
                   100.0f,
                   static_cast<uint8_t>(id * 50),
                   0,
                   0,
                   0};
    states.push_back(ps);
  }
  publishStateUpdate(1, states);

  for (uint32_t id = 2; id <= 4; id++) {
    Player player;
    assert(interpolation.getInterpolatedState(id, 0.0f, player));
    assert(floatEqual(player.x, float(id * 100)));
  }
}

int main() {
  Logger::init();

  test_RemotePlayerInterpolation_PlayerLifecycle();
  test_RemotePlayerInterpolation_SkipLocalPlayer();
  test_RemotePlayerInterpolation_Interpolation();
  test_RemotePlayerInterpolation_MultipleRemotePlayers();

  return 0;
}
