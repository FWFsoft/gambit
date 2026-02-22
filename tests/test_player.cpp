#include <cmath>

#include "Logger.h"
#include "Player.h"
#include "config/PlayerConfig.h"
#include "config/ScreenConfig.h"
#include "config/TimingConfig.h"
#include "test_utils.h"

TEST(Player_MoveRight) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, true, false, false,
                      Config::Timing::TARGET_DELTA_MS,
                      static_cast<float>(Config::Screen::WIDTH),
                      static_cast<float>(Config::Screen::HEIGHT));
  applyInput(player, input);

  // Orthogonal movement: Right = +X only
  assert(floatEqual(player.vx, Config::Player::SPEED));
  assert(floatEqual(player.vy, 0.0f));
  assert(player.x > 100.0f);
}

TEST(Player_MoveLeft) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(true, false, false, false,
                      Config::Timing::TARGET_DELTA_MS,
                      static_cast<float>(Config::Screen::WIDTH),
                      static_cast<float>(Config::Screen::HEIGHT));
  applyInput(player, input);

  // Orthogonal movement: Left = -X only
  assert(floatEqual(player.vx, -Config::Player::SPEED));
  assert(floatEqual(player.vy, 0.0f));
  assert(player.x < 100.0f);
}

TEST(Player_MoveUp) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, false, true, false,
                      Config::Timing::TARGET_DELTA_MS,
                      static_cast<float>(Config::Screen::WIDTH),
                      static_cast<float>(Config::Screen::HEIGHT));
  applyInput(player, input);

  // Orthogonal movement: Up = -Y only
  assert(floatEqual(player.vx, 0.0f));
  assert(floatEqual(player.vy, -Config::Player::SPEED));
  assert(player.y < 100.0f);
}

TEST(Player_MoveDown) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, false, false, true,
                      Config::Timing::TARGET_DELTA_MS,
                      static_cast<float>(Config::Screen::WIDTH),
                      static_cast<float>(Config::Screen::HEIGHT));
  applyInput(player, input);

  // Orthogonal movement: Down = +Y only
  assert(floatEqual(player.vx, 0.0f));
  assert(floatEqual(player.vy, Config::Player::SPEED));
  assert(player.y > 100.0f);
}

TEST(Player_DiagonalMovement) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, true, false, true, Config::Timing::TARGET_DELTA_MS,
                      static_cast<float>(Config::Screen::WIDTH),
                      static_cast<float>(Config::Screen::HEIGHT));
  applyInput(player, input);

  // Diagonal Right+Down: normalized (1,1) * speed
  float diagonalSpeed = Config::Player::SPEED * 0.707107f;
  assert(floatEqual(player.vx, diagonalSpeed));
  assert(floatEqual(player.vy, diagonalSpeed));
  assert(player.x > 100.0f);
  assert(player.y > 100.0f);
}

TEST(Player_Stationary) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, false, false, false,
                      Config::Timing::TARGET_DELTA_MS,
                      static_cast<float>(Config::Screen::WIDTH),
                      static_cast<float>(Config::Screen::HEIGHT));
  applyInput(player, input);

  assert(floatEqual(player.vx, 0.0f));
  assert(floatEqual(player.vy, 0.0f));
  assert(floatEqual(player.x, 100.0f));
  assert(floatEqual(player.y, 100.0f));
}

TEST(Player_BoundsClampingLeft) {
  // Diamond bounds: |x/halfW| + |y/halfH| <= 1
  // Start near the left edge of the diamond
  float worldW = static_cast<float>(Config::Screen::WIDTH);
  float worldH = static_cast<float>(Config::Screen::HEIGHT);
  float halfW = worldW / 2.0f;

  Player player;
  player.x = -halfW + 5.0f;  // Near left edge of diamond, y=0
  player.y = 0.0f;

  MovementInput input(true, false, false, false,
                      Config::Timing::TARGET_DELTA_MS, worldW, worldH);
  applyInput(player, input);

  // Should be clamped to diamond boundary: |x|/halfW + |y|/halfH <= 1
  float normX = std::abs(player.x) / halfW;
  float normY = std::abs(player.y) / (worldH / 2.0f);
  assert(normX + normY <= 1.01f);  // Small tolerance
}

TEST(Player_BoundsClampingRight) {
  float worldW = static_cast<float>(Config::Screen::WIDTH);
  float worldH = static_cast<float>(Config::Screen::HEIGHT);
  float halfW = worldW / 2.0f;

  Player player;
  player.x = halfW - 5.0f;  // Near right edge of diamond, y=0
  player.y = 0.0f;

  MovementInput input(false, true, false, false,
                      Config::Timing::TARGET_DELTA_MS, worldW, worldH);
  applyInput(player, input);

  float normX = std::abs(player.x) / halfW;
  float normY = std::abs(player.y) / (worldH / 2.0f);
  assert(normX + normY <= 1.01f);
}

TEST(Player_BoundsClampingTop) {
  float worldW = static_cast<float>(Config::Screen::WIDTH);
  float worldH = static_cast<float>(Config::Screen::HEIGHT);
  float halfH = worldH / 2.0f;

  Player player;
  player.x = 0.0f;
  player.y = -halfH + 5.0f;  // Near top edge of diamond, x=0

  MovementInput input(false, false, true, false,
                      Config::Timing::TARGET_DELTA_MS, worldW, worldH);
  applyInput(player, input);

  float normX = std::abs(player.x) / (worldW / 2.0f);
  float normY = std::abs(player.y) / halfH;
  assert(normX + normY <= 1.01f);
}

TEST(Player_BoundsClampingBottom) {
  float worldW = static_cast<float>(Config::Screen::WIDTH);
  float worldH = static_cast<float>(Config::Screen::HEIGHT);
  float halfH = worldH / 2.0f;

  Player player;
  player.x = 0.0f;
  player.y = halfH - 5.0f;  // Near bottom edge of diamond, x=0

  MovementInput input(false, false, false, true,
                      Config::Timing::TARGET_DELTA_MS, worldW, worldH);
  applyInput(player, input);

  float normX = std::abs(player.x) / (worldW / 2.0f);
  float normY = std::abs(player.y) / halfH;
  assert(normX + normY <= 1.01f);
}

TEST(Player_IsometricMapBounds_BottomRight) {
  // Diamond bounds: |x/halfW| + |y/halfH| <= 1
  // Start at corner of diamond and try to move further out
  float worldSize = 976.0f;
  float halfSize = worldSize / 2.0f;

  Player player;
  player.x = halfSize - 5.0f;   // Near right diamond edge at y=0
  player.y = 0.0f;

  MovementInput input(false, true, false, false, 16.67f, worldSize, worldSize);
  applyInput(player, input);

  // Should be clamped to diamond boundary
  float normX = std::abs(player.x) / halfSize;
  float normY = std::abs(player.y) / halfSize;
  assert(normX + normY <= 1.01f);
}

TEST(Player_IsometricMapBounds_TopLeft) {
  float worldSize = 976.0f;
  float halfSize = worldSize / 2.0f;

  Player player;
  player.x = 0.0f;
  player.y = -halfSize + 5.0f;  // Near top diamond edge at x=0

  MovementInput input(false, false, true, false, 16.67f, worldSize, worldSize);
  applyInput(player, input);

  // Should be clamped to diamond boundary
  float normX = std::abs(player.x) / halfSize;
  float normY = std::abs(player.y) / halfSize;
  assert(normX + normY <= 1.01f);
}

TEST(Player_IsometricMapBounds_AllCorners) {
  // Verify that diamond clamping works from all directions
  float worldSize = 976.0f;
  float halfSize = worldSize / 2.0f;

  // Move up from origin
  Player p1;
  p1.x = 0.0f;
  p1.y = 0.0f;
  MovementInput input1(false, false, true, false, 100.0f, worldSize, worldSize);
  applyInput(p1, input1);
  float normX1 = std::abs(p1.x) / halfSize;
  float normY1 = std::abs(p1.y) / halfSize;
  assert(normX1 + normY1 <= 1.01f);

  // Move right from far right edge
  Player p2;
  p2.x = halfSize - 1.0f;
  p2.y = 0.0f;
  MovementInput input2(false, true, false, false, 100.0f, worldSize, worldSize);
  applyInput(p2, input2);
  float normX2 = std::abs(p2.x) / halfSize;
  float normY2 = std::abs(p2.y) / halfSize;
  assert(normX2 + normY2 <= 1.01f);

  // Move left from far left edge
  Player p3;
  p3.x = -halfSize + 1.0f;
  p3.y = 0.0f;
  MovementInput input3(true, false, false, false, 100.0f, worldSize, worldSize);
  applyInput(p3, input3);
  float normX3 = std::abs(p3.x) / halfSize;
  float normY3 = std::abs(p3.y) / halfSize;
  assert(normX3 + normY3 <= 1.01f);

  // Move down from bottom edge
  Player p4;
  p4.x = 0.0f;
  p4.y = halfSize - 1.0f;
  MovementInput input4(false, false, false, true, 100.0f, worldSize, worldSize);
  applyInput(p4, input4);
  float normX4 = std::abs(p4.x) / halfSize;
  float normY4 = std::abs(p4.y) / halfSize;
  assert(normX4 + normY4 <= 1.01f);
}

int main() {
  Logger::init();

  test_Player_MoveRight();
  test_Player_MoveLeft();
  test_Player_MoveUp();
  test_Player_MoveDown();
  test_Player_DiagonalMovement();
  test_Player_Stationary();
  test_Player_BoundsClampingLeft();
  test_Player_BoundsClampingRight();
  test_Player_BoundsClampingTop();
  test_Player_BoundsClampingBottom();
  test_Player_IsometricMapBounds_BottomRight();
  test_Player_IsometricMapBounds_TopLeft();
  test_Player_IsometricMapBounds_AllCorners();

  return 0;
}
