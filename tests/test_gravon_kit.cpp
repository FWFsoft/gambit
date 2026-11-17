// Gravon's kit, on the real combat systems: Titan's Slash (a slow wind-up,
// then a wide arc), Ravager's Edge (rooted while it builds, a big slam, the
// full-charge crit), Fury of the Blade (and its Blade Rush refunds), Blade
// Rush in two beats, and Soulblade: the host falling leaves the blade, which
// can be hit, can be carried, and raises him again.

#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "AbilitySystem.h"
#include "ArenaData.h"
#include "EffectManager.h"
#include "EnemySystem.h"
#include "Logger.h"
#include "MatchDirector.h"

namespace {

constexpr float TICK_MS = 1000.0f / 60.0f;
constexpr uint8_t LMB = 1 << 0, RMB = 1 << 1, Q = 1 << 2, SHIFT = 1 << 3;
constexpr uint32_t GRAVON_ID = 1;
constexpr uint32_t ALLY_ID = 2;

namespace Mode = Arena::GravonMode;

struct World {
  std::unordered_map<uint32_t, Player> players;
  std::vector<EnemySpawn> noSpawns;
  EnemySystem enemies{noSpawns};
  EffectManager effects;
  std::vector<GameEventPacket> events;
  AbilitySystem combat{
      players,
      &enemies,
      &effects,
      nullptr,
      8000.0f,
      4000.0f,
      [this](const GameEventPacket &p) { events.push_back(p); }};
  float aimX = 100.0f, aimY = 0.0f;

  World() {
    enemies.setCombat(&combat);
    Player gravon;
    gravon.id = GRAVON_ID;
    gravon.characterId = Arena::CharacterId::Gravon;
    players[GRAVON_ID] = gravon;
    combat.applyKitStats(players[GRAVON_ID]);
  }

  Player &gravon() { return players[GRAVON_ID]; }
  Player &ally() { return players[ALLY_ID]; }
  uint8_t form() { return Mode::form(gravon().kitMode); }

  void addAlly(float x, float y) {
    Player ally;
    ally.id = ALLY_ID;
    ally.characterId = Arena::CharacterId::Wade;
    ally.x = x;
    ally.y = y;
    players[ALLY_ID] = ally;
    combat.applyKitStats(players[ALLY_ID]);
  }

  // A sturdy foe that will not die to the test's hits, and stands still.
  uint32_t foe(float x, float y) {
    uint32_t id = enemies.spawn(Arena::EnemyKind::VenoxaridGrunt, x, y);
    Enemy &e = enemies.getEnemies().at(id);
    e.health = e.maxHealth = 5000.0f;
    return id;
  }
  Enemy &enemy(uint32_t id) { return enemies.getEnemies().at(id); }
  float lost(uint32_t id) { return 5000.0f - enemy(id).health; }

  void run(float ms, uint8_t buttons = 0) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      combat.setInput(gravon(), buttons, aimX, aimY);
      combat.update(TICK_MS);
      effects.update(TICK_MS / 1000.0f, players, enemies.getEnemies(),
                     &enemies);
    }
  }
  void tap(uint8_t button) {
    run(TICK_MS, button);
    run(TICK_MS, 0);
  }
  void aimAt(float x, float y) {
    aimX = x;
    aimY = y;
  }
  // Knock the host down with one hit from nowhere.
  void felled() { combat.damagePlayer(gravon(), gravon().health + 50.0f, 0); }
  int castsOf(int slot, uint8_t variant) {
    int n = 0;
    for (const auto &e : events) {
      if (e.kind == GameEventKind::AbilityCast && e.sourceId == GRAVON_ID &&
          e.param == slot && e.extraId == variant) {
        n++;
      }
    }
    return n;
  }
};

void test_Selectable_StartsAsTheHost() {
  assert(Arena::isPlayable(Arena::CharacterId::Gravon));
  assert(makeKit(Arena::CharacterId::Gravon));
  World w;
  w.run(50.0f);
  assert(w.form() == Mode::Host);
  assert(w.gravon().resource == 255); // the blade is whole
}

void test_TitansSlash_HeavesOffHisBack_ThenAFlatHalfCircle() {
  World w;
  uint32_t ahead = w.foe(100.0f, 0.0f);
  uint32_t wide = w.foe(10.0f, 120.0f);  // 85 degrees off the aim
  uint32_t behind = w.foe(-100.0f, 20.0f);
  assert(!(w.gravon().kitMode & Mode::DrawnBit));
  w.tap(LMB);
  // Heaving it off his back takes a while: nothing has landed yet, and he
  // crawls while he swings.
  w.run(300.0f);
  assert(w.lost(ahead) == 0.0f);
  assert(w.gravon().speedMult < 0.5f);
  w.run(200.0f);
  assert(w.lost(ahead) > 40.0f);
  assert(w.lost(wide) > 40.0f);
  assert(w.lost(behind) == 0.0f);
  assert(w.castsOf(0, Arena::GravonForm::SlashHeave) == 1);
  assert(w.gravon().kitMode & Mode::DrawnBit);
  // Then two hands, back and forth: forehand, backhand, forehand.
  for (int i = 0; i < 3; i++) {
    w.run(1050.0f);  // past the cooldown
    w.tap(LMB);
  }
  w.run(400.0f);
  assert(w.castsOf(0, Arena::GravonForm::SlashForehand) == 2);
  assert(w.castsOf(0, Arena::GravonForm::SlashBackhand) == 1);
  assert(w.castsOf(0, Arena::GravonForm::SlashHeave) == 1);
}

void test_TitansSlash_TwoHandedSwingIsQuickerThanTheHeave() {
  World w;
  w.tap(LMB);  // the heave, to get the blade out
  w.run(1200.0f);
  uint32_t ahead = w.foe(100.0f, 0.0f);
  w.tap(LMB);
  w.run(320.0f);
  assert(w.lost(ahead) > 30.0f);
}

void test_TitansSlash_OutOfARun_HeavesItUpFromTheDrag() {
  World w;
  w.tap(Q);  // blade out
  w.run(300.0f);
  w.gravon().vx = 150.0f;  // on the move
  w.run(400.0f);
  w.tap(LMB);
  assert(w.castsOf(0, Arena::GravonForm::SlashHeaveRun) == 1);
  // It comes up on his left: the next swing goes back the other way.
  w.gravon().vx = 0.0f;
  w.run(1200.0f);
  w.tap(LMB);
  assert(w.castsOf(0, Arena::GravonForm::SlashBackhand) == 1);
}

void test_Slow_SlowerWithTheBladeOut_AndHePutsItAway() {
  World w;
  w.addAlly(500.0f, 500.0f);
  assert(w.gravon().moveSpeed < 0.85f * w.ally().moveSpeed);
  w.run(50.0f);
  float sheathed = w.gravon().speedMult;
  w.tap(Q);
  w.effects.clearEntity(GRAVON_ID, false);  // Fury's Haste aside
  w.run(100.0f);
  assert(w.gravon().kitMode & Mode::DrawnBit);
  assert(w.gravon().speedMult < sheathed);
  // A few quiet seconds and it goes back on his back.
  w.run(4200.0f);
  assert(!(w.gravon().kitMode & Mode::DrawnBit));
  // ... and the next swing heaves it off again.
  w.tap(LMB);
  assert(w.castsOf(0, Arena::GravonForm::SlashHeave) == 1);
}

void test_TitansSlash_DiesWithTheHost() {
  World w;
  uint32_t ahead = w.foe(100.0f, 0.0f);
  w.tap(LMB);
  w.felled();
  w.run(600.0f);
  assert(w.lost(ahead) == 0.0f);  // the swing never lands
}

void test_RavagersEdge_Roots_Leaps_LandsOnAnArea_AndPullsItOut() {
  World w;
  uint32_t near = w.foe(110.0f, 0.0f);   // where it lands, after the lurch
  uint32_t side = w.foe(110.0f, 100.0f);
  uint32_t far = w.foe(500.0f, 0.0f);
  w.run(400.0f, RMB);
  assert(w.gravon().kitAnchored);
  assert(w.gravon().chargeSlot == 1);
  w.run(TICK_MS, 0);  // release, part charged
  // Coming over: nothing has landed yet; its weight lurches him forward.
  assert(w.lost(near) == 0.0f);
  w.run(320.0f);
  assert(w.gravon().x > 5.0f && w.gravon().x < 30.0f);
  assert(w.lost(near) > 40.0f);
  assert(w.lost(side) > 40.0f);
  assert(w.lost(far) == 0.0f);
  // Heaving it out of the floor: rooted, and no swinging.
  float x = w.gravon().x;
  assert(w.gravon().kitAnchored);
  w.tap(LMB);
  w.run(500.0f);
  assert(w.gravon().x == x);
  assert(w.castsOf(0, Arena::GravonForm::SlashForehand) == 0);
  // Then he is free again.
  w.run(500.0f);
  assert(!w.gravon().kitAnchored);
}

void test_RavagersEdge_FullChargeCritsForFarMoreThanTheSlash() {
  World w;
  w.tap(LMB);  // the heave, to get the blade out
  w.run(1200.0f);
  uint32_t slashed = w.foe(100.0f, 0.0f);
  w.tap(LMB);
  w.run(600.0f);
  float slash = w.lost(slashed);
  assert(slash > 0.0f);

  World v;
  uint32_t slammed = v.foe(110.0f, 0.0f);
  v.run(1600.0f, RMB);
  assert(v.gravon().kitMode & Mode::ChargedBit);
  v.run(TICK_MS, 0);
  assert(v.effects.getPlayerEffects(GRAVON_ID).hasEffect(EffectType::Sharpened));
  assert(v.castsOf(1, Arena::GravonForm::RavagerFull) == 1);
  v.run(400.0f);
  float slam = v.lost(slammed);
  assert(slam > 3.0f * slash);
  bool crit = false;
  for (const auto &e : v.events) {
    if (e.kind == GameEventKind::Damage && e.sourceIsEnemy && (e.param & 1)) {
      crit = true;
    }
  }
  assert(crit);
}

void test_Fury_EmpowersHastes_AndHitsRefundBladeRush() {
  World w;
  w.foe(100.0f, 0.0f);
  w.tap(SHIFT); // put Blade Rush on cooldown
  w.run(600.0f);
  w.gravon().x = 0.0f;
  w.gravon().y = 0.0f;
  w.tap(Q);
  const ActiveEffects &fx = w.effects.getPlayerEffects(GRAVON_ID);
  assert(fx.hasEffect(EffectType::Empowered));
  assert(fx.hasEffect(EffectType::Haste));
  w.run(200.0f); // past the cast's action lock
  assert(w.gravon().kitMode & Mode::FuryBit);
  float before = w.gravon().cooldownMs[3];
  w.tap(LMB);
  w.run(400.0f);
  // One hit refunds 700 ms on top of the (hasted) clock.
  assert(w.gravon().cooldownMs[3] < before - 900.0f);
}

void test_BladeRush_SlidesThenStabsTheFirstFoe() {
  World w;
  uint32_t first = w.foe(260.0f, 0.0f);
  uint32_t second = w.foe(300.0f, 0.0f);
  w.aimAt(1000.0f, 0.0f);
  w.tap(SHIFT);
  w.run(120.0f);
  // Mid-slide: moving, no stab yet.
  assert(w.gravon().x > 20.0f);
  assert(w.lost(first) == 0.0f);
  w.run(300.0f);
  assert(w.gravon().x > 200.0f);
  assert(w.lost(first) > 50.0f);
  assert(w.lost(second) == 0.0f);
  assert(w.castsOf(3, 0) == 1);
  assert(w.castsOf(3, Arena::GravonForm::RushStab) == 1);
}

void test_Soulblade_HostFalls_BladeIsExposed_AndRaisesHim() {
  World w;
  w.gravon().x = 50.0f;
  w.gravon().y = 60.0f;
  w.felled();
  assert(w.gravon().isAlive());
  assert(w.form() == Mode::Exposed);
  assert(w.gravon().maxHealth < 200.0f); // the blade's own health
  assert(w.gravon().propX == 50.0f && w.gravon().propY == 60.0f);
  w.run(100.0f);
  assert(w.gravon().kitAnchored);
  // The blade cannot swing.
  uint32_t foeId = w.foe(100.0f, 60.0f);
  w.tap(LMB);
  w.run(400.0f);
  assert(w.lost(foeId) == 0.0f);
  // It can be hit.
  float before = w.gravon().health;
  w.combat.damagePlayer(w.gravon(), 10.0f, foeId);
  assert(w.gravon().health < before);
  // After the delay the host stands up, with his own health back.
  w.run(15000.0f);
  assert(w.form() == Mode::Host);
  assert(w.gravon().maxHealth > 200.0f);
  assert(w.gravon().health > 100.0f);
  assert(!w.gravon().kitAnchored);
  // The blade keeps what it lost.
  assert(w.gravon().resource < 255);
}

void test_Soulblade_NearbyDeathsShortenTheWait() {
  World w;
  w.felled();
  w.run(10000.0f);
  assert(w.form() == Mode::Exposed);
  for (int i = 0; i < 4; i++) {
    uint32_t id = w.foe(50.0f, 0.0f);
    w.combat.onEnemyDied(w.enemy(id), 0);
  }
  w.run(1500.0f); // 15 - 4 = 11 s
  assert(w.form() == Mode::Host);
}

void test_Soulblade_BladeBreaking_IsDeath() {
  World w;
  w.felled();
  w.combat.damagePlayer(w.gravon(), 500.0f, 0);
  assert(w.gravon().isDead());
  // When an ally brings him back, it is as the host.
  assert(w.form() == Mode::Host);
  assert(w.gravon().maxHealth > 200.0f);
}

void test_Soulblade_AnAllyCarriesItSafely() {
  World w;
  w.addAlly(400.0f, 0.0f);
  w.felled();
  w.run(1000.0f);
  assert(w.form() == Mode::Exposed);
  // The ally walks over it: they pick it up.
  w.ally().x = 20.0f;
  w.run(100.0f);
  assert(w.form() == Mode::Carried);
  assert(static_cast<uint32_t>(w.gravon().kitX) == ALLY_ID);
  // It rides on their back, and nothing can touch it there.
  w.ally().x = 300.0f;
  w.ally().y = 100.0f;
  w.run(100.0f);
  assert(w.gravon().x == 300.0f && w.gravon().y == 100.0f);
  float before = w.gravon().health;
  w.combat.damagePlayer(w.gravon(), 50.0f, 0);
  assert(w.gravon().health == before);
  // The host rises on the spot, beside the carrier.
  w.run(14000.0f);
  assert(w.form() == Mode::Host);
  assert(w.gravon().x == 300.0f);
}

void test_Soulblade_CarrierFalls_BladeDrops() {
  World w;
  w.addAlly(20.0f, 0.0f);
  w.felled();
  w.run(800.0f);
  assert(w.form() == Mode::Carried);
  w.ally().health = 0.0f;
  w.run(50.0f);
  assert(w.form() == Mode::Exposed);
}

void test_Soulblade_DeathOutsideAHit_StillFellsTheHost() {
  // Damage over time takes health straight off (EffectManager), never
  // through damagePlayer: the kit must still get its say.
  World w;
  w.gravon().health = 0.0f;
  w.run(50.0f);
  assert(w.gravon().isAlive());
  assert(w.form() == Mode::Exposed);
}

void test_RoundStart_GivesTheHostBack() {
  World w;
  w.felled();
  w.run(100.0f);
  w.combat.resetPlayerForRound(w.gravon());
  w.run(50.0f);
  assert(w.form() == Mode::Host);
  assert(w.gravon().maxHealth > 200.0f);
  assert(!w.gravon().kitAnchored);
  assert(w.gravon().resource == 255);
}

void test_Soulblade_TheMatchGoesOnWhileTheBladeLives() {
  // Solo, on the real match rules, in the server's order: deaths settle,
  // then the director judges. The host falling to damage over time (which
  // never goes through damagePlayer) must leave the blade, not a defeat.
  World w;
  MatchDirector director{w.players, &w.enemies, &w.combat, 0.0f, 0.0f,
                         8000.0f, 4000.0f, MatchDirector::Hooks{}};
  director.onPlayerJoined(w.gravon());
  director.onCharacterSelected(w.gravon(), Arena::CharacterId::Gravon);
  director.onReady(w.gravon(), true);
  auto tick = [&](float ms) {
    for (float t = 0.0f; t < ms; t += TICK_MS) {
      w.combat.settleDeaths();
      director.update(TICK_MS);
      w.combat.update(TICK_MS);
    }
  };
  tick(Arena::Tuning::LOBBY_COUNTDOWN_SECONDS * 1000.0f + 500.0f);
  assert(director.phase() == Arena::MatchPhase::Round);

  w.gravon().health = 0.0f;  // as EffectManager leaves a DoT death
  tick(500.0f);
  assert(director.phase() == Arena::MatchPhase::Round);
  assert(w.form() == Mode::Exposed);
  assert(w.gravon().deaths == 0);

  // The blade breaking is the end, solo.
  w.combat.damagePlayer(w.gravon(), 500.0f, 0);
  tick(100.0f);
  assert(director.phase() == Arena::MatchPhase::Defeat);
}

} // namespace

int main() {
  Logger::init();
  test_Selectable_StartsAsTheHost();
  test_TitansSlash_HeavesOffHisBack_ThenAFlatHalfCircle();
  test_TitansSlash_TwoHandedSwingIsQuickerThanTheHeave();
  test_TitansSlash_OutOfARun_HeavesItUpFromTheDrag();
  test_Slow_SlowerWithTheBladeOut_AndHePutsItAway();
  test_TitansSlash_DiesWithTheHost();
  test_RavagersEdge_Roots_Leaps_LandsOnAnArea_AndPullsItOut();
  test_RavagersEdge_FullChargeCritsForFarMoreThanTheSlash();
  test_Fury_EmpowersHastes_AndHitsRefundBladeRush();
  test_BladeRush_SlidesThenStabsTheFirstFoe();
  test_Soulblade_HostFalls_BladeIsExposed_AndRaisesHim();
  test_Soulblade_NearbyDeathsShortenTheWait();
  test_Soulblade_BladeBreaking_IsDeath();
  test_Soulblade_AnAllyCarriesItSafely();
  test_Soulblade_CarrierFalls_BladeDrops();
  test_Soulblade_DeathOutsideAHit_StillFellsTheHost();
  test_RoundStart_GivesTheHostBack();
  test_Soulblade_TheMatchGoesOnWhileTheBladeLives();
  std::cout << "Gravon kit passed\n";
}
