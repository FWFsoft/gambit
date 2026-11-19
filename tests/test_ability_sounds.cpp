// Ability cast sounds (AbilitySounds): which shape and tint a cast makes,
// how loud, and which shape events belong to it. See docs/design/SFX.md.

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "AbilitySounds.h"

using AbilitySounds::Shape;

namespace {

constexpr uint32_t LOCAL = 7;
constexpr uint32_t MATE = 8;

GameEventPacket cast(uint32_t caster, float x, float y, uint8_t slot = 2) {
  GameEventPacket p;
  p.kind = GameEventKind::AbilityCast;
  p.sourceId = caster;
  p.x = x;
  p.y = y;
  p.x2 = x + 300.0f;  // aiming right
  p.y2 = y;
  p.param = slot;
  return p;
}

GameEventPacket shape(GameEventKind kind, float x, float y, VfxTheme theme,
                      uint32_t owner = 0, bool enemy = false) {
  GameEventPacket p;
  p.kind = kind;
  p.x = x;
  p.y = y;
  p.x2 = x;
  p.y2 = y;
  p.param2 = static_cast<uint8_t>(theme);
  p.sourceId = owner;
  p.sourceIsEnemy = enemy;
  return p;
}

AbilitySounds::Cue mine(const std::vector<GameEventPacket>& group,
                        uint8_t slot = 2) {
  return AbilitySounds::chooseCast(cast(LOCAL, 0, 0, slot), group, LOCAL, 0, 0);
}

void castWithNoShapeIsGeneric() {
  auto cue = mine({});
  assert(cue.shape == Shape::Generic);
  assert(cue.theme == VfxTheme::Neutral);
  assert(cue.volume == AbilitySounds::LOCAL_ABILITY_VOLUME);
}

void shapeAndThemeComeFromTheEvents() {
  auto cue = mine({shape(GameEventKind::Cone, 40, 0, VfxTheme::Fire)});
  assert(cue.shape == Shape::Cone);
  assert(cue.theme == VfxTheme::Fire);
}

void theMostTellingShapeWins() {
  // A beam with a ring and a burst at the muzzle is a beam.
  auto cue = mine({shape(GameEventKind::Ring, 0, 0, VfxTheme::Neutral),
                   shape(GameEventKind::Burst, 0, 0, VfxTheme::Optic),
                   shape(GameEventKind::Beam, 0, 0, VfxTheme::Optic)});
  assert(cue.shape == Shape::Beam);
  assert(cue.theme == VfxTheme::Optic);
}

void untintedPrimaryBorrowsATint() {
  // A Wind slash (no tint) with a Holy ring: the slash, tinted Holy.
  auto cue = mine({shape(GameEventKind::Slash, 0, 0, VfxTheme::Wind),
                   shape(GameEventKind::Ring, 0, 0, VfxTheme::Holy)});
  assert(cue.shape == Shape::Slash);
  assert(cue.theme == VfxTheme::Holy);
  assert(!AbilitySounds::hasTint(VfxTheme::Wind));
  assert(!AbilitySounds::hasTint(VfxTheme::Neutral));
}

void farOrForeignShapesAreNotTheCasts() {
  auto farBurst = shape(GameEventKind::Burst, 3000, 3000, VfxTheme::Fire);
  auto otherSlash = shape(GameEventKind::Slash, 0, 0, VfxTheme::Steel, MATE);
  auto enemyShot =
      shape(GameEventKind::ProjectileSpawn, 0, 0, VfxTheme::Fire, 99, true);
  auto mateShot =
      shape(GameEventKind::ProjectileSpawn, 0, 0, VfxTheme::Fire, MATE);
  auto telegraph = shape(GameEventKind::Burst, 0, 0, VfxTheme::Danger);
  GameEventPacket me = cast(LOCAL, 0, 0);
  for (const auto& e : {farBurst, otherSlash, enemyShot, mateShot, telegraph}) {
    assert(!AbilitySounds::belongsTo(e, me));
  }
  assert(mine({farBurst, otherSlash, enemyShot, mateShot, telegraph}).shape ==
         Shape::Generic);
}

void shapesNearTheAimPointBelong() {
  // A lob's burst lands at the aim point, 300 px off; a beam's far end too.
  GameEventPacket me = cast(LOCAL, 0, 0);
  me.x2 = 1200.0f;
  assert(AbilitySounds::belongsTo(
      shape(GameEventKind::Burst, 1200, 0, VfxTheme::Fire), me));
  GameEventPacket beam = shape(GameEventKind::Beam, 2500, 0, VfxTheme::Optic);
  beam.x2 = 1250.0f;
  assert(AbilitySounds::belongsTo(beam, me));
  assert(AbilitySounds::belongsTo(
      shape(GameEventKind::ProjectileSpawn, 0, 0, VfxTheme::Fire, LOCAL), me));
}

void basicAttackIsQuieter() {
  auto basic = mine({shape(GameEventKind::Slash, 0, 0, VfxTheme::Steel)}, 0);
  auto special = mine({shape(GameEventKind::Slash, 0, 0, VfxTheme::Steel)}, 2);
  assert(basic.volume < special.volume);
}

void teammatesAreQuieterAndPanned() {
  auto group = std::vector<GameEventPacket>{
      shape(GameEventKind::Cone, 600, 0, VfxTheme::Fire)};
  auto mate = AbilitySounds::chooseCast(cast(MATE, 600, 0), group, LOCAL, 0, 0);
  auto me = mine({shape(GameEventKind::Cone, 0, 0, VfxTheme::Fire)});
  assert(mate.shape == Shape::Cone);
  assert(mate.volume < me.volume);
  assert(mate.pan > 0.5f);
  auto gone =
      AbilitySounds::chooseCast(cast(MATE, 3000, 0), group, LOCAL, 0, 0);
  assert(gone.shape == Shape::None);
}

void everyShapeHasAFileAndLimits() {
  for (int s = static_cast<int>(Shape::Generic);
       s < static_cast<int>(Shape::Count); s++) {
    Shape shape = static_cast<Shape>(s);
    assert(AbilitySounds::shapeName(shape)[0] != '\0');
    auto l = AbilitySounds::limitsFor(shape);
    assert(l.minGapMs > 0.0f && l.maxVoices > 0);
  }
}

void nonShapeEventsAreNone() {
  assert(AbilitySounds::shapeOf(shape(GameEventKind::Damage, 0, 0,
                                      VfxTheme::Neutral)) == Shape::None);
  assert(AbilitySounds::shapeOf(shape(GameEventKind::TelegraphCircle, 0, 0,
                                      VfxTheme::Danger)) == Shape::None);
  assert(AbilitySounds::shapeOf(shape(GameEventKind::ZoneSpawn, 0, 0,
                                      VfxTheme::Poison, 5, true)) ==
         Shape::None);
}

void grouperJoinsShapesToTheCastThatFollows() {
  AbilitySounds::CastGrouper g;
  // Same tick: the kit's cone, then the cast event.
  assert(g.onEvent(shape(GameEventKind::Cone, 30, 0, VfxTheme::Fire), 0.0f,
                   LOCAL, 0, 0)
             .empty());
  auto cues = g.onEvent(cast(LOCAL, 0, 0), 0.0f, LOCAL, 0, 0);
  assert(cues.size() == 1 && cues[0].shape == Shape::Cone);
  // The cone is spent: the next cast does not reuse it.
  assert(g.onEvent(cast(LOCAL, 0, 0), 5.0f, LOCAL, 0, 0).empty());
}

void grouperWaitsForAWindUp() {
  AbilitySounds::CastGrouper g;
  assert(g.onEvent(cast(LOCAL, 0, 0), 0.0f, LOCAL, 0, 0).empty());
  assert(g.onTick(200.0f, LOCAL, 0, 0).empty());
  // The swing lands 370 ms later: that is when it sounds.
  auto cues = g.onEvent(shape(GameEventKind::Slash, 0, 0, VfxTheme::Shadow),
                        370.0f, LOCAL, 0, 0);
  assert(cues.size() == 1 && cues[0].shape == Shape::Slash);
  assert(cues[0].theme == VfxTheme::Shadow);
  assert(g.onTick(1000.0f, LOCAL, 0, 0).empty());  // nothing left waiting
}

void stalePendingShapesAreNotClaimed() {
  AbilitySounds::CastGrouper g;
  g.onEvent(shape(GameEventKind::Burst, 0, 0, VfxTheme::Fire), 0.0f, LOCAL, 0,
            0);
  // Long after: the burst was something else (an enemy's, a delayed blast).
  assert(g.onEvent(cast(LOCAL, 0, 0), 500.0f, LOCAL, 0, 0).empty());
}

void shapelessCastsAreLearned() {
  AbilitySounds::CastGrouper g;
  assert(g.onEvent(cast(LOCAL, 0, 0, 1), 0.0f, LOCAL, 0, 0).empty());
  auto late = g.onTick(AbilitySounds::CAST_WAIT_MS + 20.0f, LOCAL, 0, 0);
  assert(late.size() == 1 && late[0].shape == Shape::Generic);
  // Next time the same slot sounds at once.
  auto now = g.onEvent(cast(LOCAL, 0, 0, 1), 1000.0f, LOCAL, 0, 0);
  assert(now.size() == 1 && now[0].shape == Shape::Generic);
  // Another slot still waits.
  assert(g.onEvent(cast(LOCAL, 0, 0, 2), 1000.0f, LOCAL, 0, 0).empty());
}

}  // namespace

int main() {
  castWithNoShapeIsGeneric();
  shapeAndThemeComeFromTheEvents();
  theMostTellingShapeWins();
  untintedPrimaryBorrowsATint();
  farOrForeignShapesAreNotTheCasts();
  shapesNearTheAimPointBelong();
  basicAttackIsQuieter();
  teammatesAreQuieterAndPanned();
  everyShapeHasAFileAndLimits();
  nonShapeEventsAreNone();
  grouperJoinsShapesToTheCastThatFollows();
  grouperWaitsForAWindUp();
  stalePendingShapesAreNotClaimed();
  shapelessCastsAreLearned();
  std::cout << "test_ability_sounds: all passed\n";
  return 0;
}
