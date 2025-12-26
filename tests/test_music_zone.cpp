#include <cassert>

#include "Logger.h"
#include "MusicZone.h"

#define TEST(name)    \
  void test_##name(); \
  void test_##name()

TEST(MusicZone_ContainsPointInside) {
  MusicZone zone{"test", "test.ogg", 100.0f, 100.0f, 200.0f, 150.0f};

  // Points inside zone
  assert(zone.contains(150.0f, 125.0f));  // Center
  assert(zone.contains(100.0f, 100.0f));  // Top-left corner
  assert(zone.contains(299.9f, 249.9f));  // Just inside bottom-right
}

TEST(MusicZone_DoesNotContainPointOutside) {
  MusicZone zone{"test", "test.ogg", 100.0f, 100.0f, 200.0f, 150.0f};

  // Points outside zone
  assert(!zone.contains(99.9f, 100.0f));   // Just left
  assert(!zone.contains(100.0f, 99.9f));   // Just above
  assert(!zone.contains(300.0f, 125.0f));  // Right edge
  assert(!zone.contains(150.0f, 250.0f));  // Below
  assert(!zone.contains(0.0f, 0.0f));      // Far away
}

TEST(MusicZone_EdgeCases) {
  MusicZone zone{"test", "test.ogg", 0.0f, 0.0f, 100.0f, 100.0f};

  // Boundary tests
  assert(zone.contains(0.0f, 0.0f));       // Origin
  assert(zone.contains(99.99f, 99.99f));   // Almost at edge
  assert(!zone.contains(100.0f, 100.0f));  // Exactly at edge (exclusive)
}

TEST(MusicZone_NegativeCoordinates) {
  MusicZone zone{"test", "test.ogg", -50.0f, -50.0f, 100.0f, 100.0f};

  assert(zone.contains(-25.0f, -25.0f));   // Center of negative zone
  assert(zone.contains(-50.0f, -50.0f));   // Top-left
  assert(!zone.contains(-51.0f, -25.0f));  // Just outside
}

TEST(MusicZone_ZeroSizedZone) {
  MusicZone zone{"test", "test.ogg", 100.0f, 100.0f, 0.0f, 0.0f};

  // Zero-sized zone should not contain any points (except exact corner)
  assert(!zone.contains(100.0f, 100.0f));  // Edge is exclusive
  assert(!zone.contains(99.9f, 100.0f));
  assert(!zone.contains(100.1f, 100.0f));
}

TEST(MusicZone_LargeZone) {
  MusicZone zone{"test", "test.ogg", 0.0f, 0.0f, 10000.0f, 10000.0f};

  assert(zone.contains(5000.0f, 5000.0f));     // Center
  assert(zone.contains(0.0f, 0.0f));           // Top-left
  assert(zone.contains(9999.9f, 9999.9f));     // Near bottom-right
  assert(!zone.contains(10000.0f, 10000.0f));  // Edge (exclusive)
  assert(!zone.contains(-0.1f, 5000.0f));      // Just outside left
  assert(!zone.contains(5000.0f, 10000.1f));   // Just outside bottom
}

int main() {
  Logger::init();

  test_MusicZone_ContainsPointInside();
  test_MusicZone_DoesNotContainPointOutside();
  test_MusicZone_EdgeCases();
  test_MusicZone_NegativeCoordinates();
  test_MusicZone_ZeroSizedZone();
  test_MusicZone_LargeZone();

  return 0;
}
