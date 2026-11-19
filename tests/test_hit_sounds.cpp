// Hit sound rules (HitSounds): which Damage events make a sound, how loud,
// and where they pan. See docs/design/SFX.md.

#include <cassert>
#include <cmath>
#include <iostream>

#include "HitSounds.h"

using HitSounds::Sound;

namespace {

constexpr uint32_t LOCAL = 7;

GameEventPacket damage(float x, float y, float amount, uint8_t param,
                       bool onEnemy, uint32_t victim) {
  GameEventPacket p;
  p.kind = GameEventKind::Damage;
  p.x = x;
  p.y = y;
  p.value = amount;
  p.param = param;
  p.sourceIsEnemy = onEnemy;
  p.sourceId = victim;
  return p;
}

void enemyHitNearbyIsFullAndCentred() {
  auto cue = HitSounds::choose(damage(0, 0, 40, 0, true, 3), LOCAL, 0, 0);
  assert(cue.sound == Sound::EnemyHit);
  assert(std::abs(cue.volume - 1.0f) < 0.001f);
  assert(std::abs(cue.pan) < 0.001f);
}

void critHasItsOwnSound() {
  auto cue = HitSounds::choose(damage(0, 0, 5, 1, true, 3), LOCAL, 0, 0);
  assert(cue.sound == Sound::EnemyCrit);
  assert(std::abs(cue.volume - 1.0f) < 0.001f);  // crits are always loud
}

void biggerHitsAreLouder() {
  auto small = HitSounds::choose(damage(0, 0, 4, 0, true, 3), LOCAL, 0, 0);
  auto big = HitSounds::choose(damage(0, 0, 40, 0, true, 3), LOCAL, 0, 0);
  assert(small.volume < big.volume);
  assert(small.volume >= HitSounds::MIN_VOLUME);
}

void distanceFadesAndPans() {
  auto near = HitSounds::choose(damage(100, 0, 40, 0, true, 3), LOCAL, 0, 0);
  auto far = HitSounds::choose(damage(1200, 0, 40, 0, true, 3), LOCAL, 0, 0);
  auto left = HitSounds::choose(damage(-1200, 0, 40, 0, true, 3), LOCAL, 0, 0);
  auto gone = HitSounds::choose(damage(2000, 0, 40, 0, true, 3), LOCAL, 0, 0);
  assert(far.volume < near.volume);
  assert(far.pan > 0.99f && left.pan < -0.99f);
  assert(near.pan > 0.0f && near.pan < 0.2f);
  assert(gone.sound == Sound::None);
}

void localHeroHitIsHurt() {
  auto cue =
      HitSounds::choose(damage(500, 500, 3, 2, false, LOCAL), LOCAL, 0, 0);
  assert(cue.sound == Sound::LocalHurt);
  assert(cue.volume == 1.0f && cue.pan == 0.0f);
}

void teammateHitIsSilent() {
  auto cue =
      HitSounds::choose(damage(0, 0, 30, 2, false, LOCAL + 1), LOCAL, 0, 0);
  assert(cue.sound == Sound::None);
}

void ticksAndTinyHitsAreSilent() {
  assert(HitSounds::choose(damage(0, 0, 30, 4, true, 3), LOCAL, 0, 0).sound ==
         Sound::None);  // damage over time
  assert(HitSounds::choose(damage(0, 0, 30, 2 | 4, false, LOCAL), LOCAL, 0, 0)
             .sound == Sound::None);  // poison on you
  assert(HitSounds::choose(damage(0, 0, 0.2f, 0, true, 3), LOCAL, 0, 0).sound ==
         Sound::None);
}

void otherEventsAreSilent() {
  GameEventPacket heal = damage(0, 0, 30, 0, false, LOCAL);
  heal.kind = GameEventKind::Heal;
  assert(HitSounds::choose(heal, LOCAL, 0, 0).sound == Sound::None);
}

void everySoundHasLimits() {
  for (Sound s : {Sound::EnemyHit, Sound::EnemyCrit, Sound::LocalHurt}) {
    HitSounds::Limits l = HitSounds::limitsFor(s);
    assert(l.minGapMs > 0.0f && l.maxVoices > 0);
  }
}

}  // namespace

int main() {
  enemyHitNearbyIsFullAndCentred();
  critHasItsOwnSound();
  biggerHitsAreLouder();
  distanceFadesAndPans();
  localHeroHitIsHurt();
  teammateHitIsSilent();
  ticksAndTinyHitsAreSilent();
  otherEventsAreSilent();
  everySoundHasLimits();
  std::cout << "test_hit_sounds: all passed\n";
  return 0;
}
