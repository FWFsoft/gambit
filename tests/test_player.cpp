#include "Logger.h"
#include "Player.h"
#include "test_utils.h"

TEST(Player_MoveRight) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, true, false, false, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, expectedSpeed));
  assert(floatEqual(player.vy, -expectedSpeed));
  assert(player.x > 100.0f);
  assert(player.y < 100.0f);
}

TEST(Player_MoveLeft) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(true, false, false, false, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, -expectedSpeed));
  assert(floatEqual(player.vy, expectedSpeed));
  assert(player.x < 100.0f);
  assert(player.y > 100.0f);
}

TEST(Player_MoveUp) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, false, true, false, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, -expectedSpeed));
  assert(floatEqual(player.vy, -expectedSpeed));
  assert(player.x < 100.0f);
  assert(player.y < 100.0f);
}

TEST(Player_MoveDown) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, false, false, true, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  float expectedSpeed = 200.0f * 0.707107f;
  assert(floatEqual(player.vx, expectedSpeed));
  assert(floatEqual(player.vy, expectedSpeed));
  assert(player.x > 100.0f);
  assert(player.y > 100.0f);
}

TEST(Player_DiagonalMovement) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, true, false, true, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  assert(floatEqual(player.vx, 200.0f));
  assert(floatEqual(player.vy, 0.0f));
  assert(player.x > 100.0f);
  assert(floatEqual(player.y, 100.0f));
}

TEST(Player_Stationary) {
  Player player;
  player.x = 100.0f;
  player.y = 100.0f;

  MovementInput input(false, false, false, false, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  assert(floatEqual(player.vx, 0.0f));
  assert(floatEqual(player.vy, 0.0f));
  assert(floatEqual(player.x, 100.0f));
  assert(floatEqual(player.y, 100.0f));
}

TEST(Player_BoundsClampingLeft) {
  Player player;
  player.x = 5.0f;
  player.y = 100.0f;

  MovementInput input(true, false, false, false, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  assert(player.x >= 0.0f);
  assert(player.x <= 5.0f);
}

TEST(Player_BoundsClampingRight) {
  Player player;
  player.x = 795.0f;
  player.y = 100.0f;

  MovementInput input(false, true, false, false, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  assert(player.x <= 800.0f);
}

TEST(Player_BoundsClampingTop) {
  Player player;
  player.x = 100.0f;
  player.y = 5.0f;

  MovementInput input(false, false, true, false, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  assert(player.y >= 0.0f);
}

TEST(Player_BoundsClampingBottom) {
  Player player;
  player.x = 100.0f;
  player.y = 595.0f;

  MovementInput input(false, false, false, true, 16.67f, 800.0f, 600.0f);
  applyInput(player, input);

  assert(player.y <= 600.0f);
}

TEST(Player_IsometricMapBounds_BottomRight) {
  Player player;
  player.x = 970.0f;
  player.y = 970.0f;

  MovementInput input(false, true, false, true, 16.67f, 976.0f, 976.0f);
  applyInput(player, input);

  assert(player.x <= 976.0f);
  assert(player.y <= 976.0f);
}

TEST(Player_IsometricMapBounds_TopLeft) {
  Player player;
  player.x = 5.0f;
  player.y = 5.0f;

  MovementInput input(false, false, true, false, 16.67f, 976.0f, 976.0f);
  applyInput(player, input);

  assert(player.x >= 0.0f);
  assert(player.y >= 0.0f);
}

TEST(Player_IsometricMapBounds_AllCorners) {
  float worldSize = 976.0f;

  Player topLeft;
  topLeft.x = 0.0f;
  topLeft.y = 0.0f;
  MovementInput input1(false, false, true, false, 100.0f, worldSize, worldSize);
  applyInput(topLeft, input1);
  assert(topLeft.x >= 0.0f && topLeft.y >= 0.0f);

  Player topRight;
  topRight.x = worldSize;
  topRight.y = 0.0f;
  MovementInput input2(false, true, false, false, 100.0f, worldSize, worldSize);
  applyInput(topRight, input2);
  assert(topRight.x <= worldSize && topRight.y >= 0.0f);

  Player bottomLeft;
  bottomLeft.x = 0.0f;
  bottomLeft.y = worldSize;
  MovementInput input3(true, false, false, false, 100.0f, worldSize, worldSize);
  applyInput(bottomLeft, input3);
  assert(bottomLeft.x >= 0.0f && bottomLeft.y <= worldSize);

  Player bottomRight;
  bottomRight.x = worldSize;
  bottomRight.y = worldSize;
  MovementInput input4(false, false, false, true, 100.0f, worldSize, worldSize);
  applyInput(bottomRight, input4);
  assert(bottomRight.x <= worldSize && bottomRight.y <= worldSize);
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
