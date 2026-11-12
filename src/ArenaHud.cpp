// In-game HUD for the arena. World rendering lives in ArenaView.cpp.
//
// Passive HUD elements are drawn straight onto the background draw list (on
// top of the world, under any window). Screens you click on (the suit /
// infusion screen, scoreboard) are regular ImGui windows.

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "ArenaDraw.h"
#include "ArenaView.h"
#include "CharacterRegistry.h"
#include "ClientPrediction.h"
#include "Effect.h"
#include "EffectTracker.h"
#include "config/ScreenConfig.h"

using namespace ArenaDraw;
using Arena::Genus;
using Arena::MatchPhase;

namespace {

constexpr float SCREEN_W = static_cast<float>(Config::Screen::WIDTH);
constexpr float SCREEN_H = static_cast<float>(Config::Screen::HEIGHT);

const char* SLOT_KEYS[Arena::SLOT_COUNT] = {"LMB", "RMB", "Q", "SHIFT", "E"};

const char* characterName(uint32_t characterId) {
  const CharacterDefinition* def =
      CharacterRegistry::instance().getCharacter(characterId);
  return def ? def->name.c_str() : "...";
}

std::string formatTime(float seconds) {
  int total = static_cast<int>(std::ceil(std::max(0.0f, seconds)));
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
  return buffer;
}

ImU32 genusColor(Genus genus, float alpha = 1.0f) {
  return themeColor(genusTheme(genus), alpha);
}

ImVec4 genusColorV(Genus genus, float brightness) {
  Rgb c = themeRgb(genusTheme(genus));
  return ImVec4(c.r * brightness, c.g * brightness, c.b * brightness, 1.0f);
}

void panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float alpha = 0.72f) {
  dl->AddRectFilled(a, b, IM_COL32(14, 16, 26, static_cast<int>(255 * alpha)),
                    6.0f);
  dl->AddRect(a, b, IM_COL32(255, 255, 255, 36), 6.0f);
}

}  // namespace

// ---------------------------------------------------------------------------

void ArenaView::renderHud() {
  handleHotkeys();
  if (!arena->hasMatch()) {
    text(ImGui::GetBackgroundDrawList(), ImVec2(SCREEN_W * 0.5f, 60.0f),
         IM_COL32(255, 255, 255, 220), 18.0f, "Connecting to the match...");
    return;
  }

  MatchPhase phase = arena->phase();
  drawStatusBar();
  drawMinimap();
  drawScoreboard(ImGui::IsKeyDown(ImGuiKey_Tab));
  drawAbilityBar();
  drawPrompts();
  drawEffectLabPanel();
  drawBanners();

  if (phase == MatchPhase::Lobby) drawLobby();
  if (phase == MatchPhase::Victory || phase == MatchPhase::Defeat) {
    drawEndScreen();
  }
  if (phase == MatchPhase::Intermission) {
    drawSuitScreen(true);
  } else if (suitScreenOpen) {
    drawSuitScreen(false);
  }
  if (phase == MatchPhase::Round || phase == MatchPhase::BossFight) {
    drawDownedOverlay();
  }

  // Reticle: the cursor is the aim point, so make it easy to track.
  ImGuiIO& io = ImGui::GetIO();
  if (!io.WantCaptureMouse) {
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 m = io.MousePos;
    fg->AddCircle(m, 9.0f, IM_COL32(0, 0, 0, 200), 20, 3.5f);
    fg->AddCircle(m, 9.0f, IM_COL32(255, 255, 255, 235), 20, 1.5f);
    fg->AddCircleFilled(m, 2.0f, IM_COL32(255, 255, 255, 255));
  }
}

void ArenaView::handleHotkeys() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput) return;

  auto debug = [&](DebugCommandKind kind, uint8_t arg = 0) {
    DebugCommandPacket packet;
    packet.command = static_cast<uint8_t>(kind);
    packet.arg = arg;
    prediction->sendToServer(serialize(packet));
  };

  // R: ready up (lobby countdown is automatic; this is for intermission and
  // the end screen).
  if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
    const MatchPlayerState* me = arena->matchPlayer(localId());
    PlayerReadyPacket packet;
    packet.ready = (me && me->ready) ? 0 : 1;
    prediction->sendToServer(serialize(packet));
  }
  if (ImGui::IsKeyPressed(ImGuiKey_B, false)) suitScreenOpen = !suitScreenOpen;

  // Playtest shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) debug(DebugCommandKind::SkipPhase);
  if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) debug(DebugCommandKind::GiveInfusions);
  if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) debug(DebugCommandKind::SpawnWave);
  if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) debug(DebugCommandKind::ToggleGodMode);
  if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) debug(DebugCommandKind::KillAll);
  if (ImGui::IsKeyPressed(ImGuiKey_F10, false)) debug(DebugCommandKind::SpawnBoss);
  if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
    debug(DebugCommandKind::SpawnWave,
          static_cast<uint8_t>(Arena::Genus::Venoxarid));
  }

  // Effect lab (F12): number keys drive each effect verb near the cursor.
  if (ImGui::IsKeyPressed(ImGuiKey_F12, false)) {
    effectLab = !effectLab;
    if (effectLab) {
      debug(DebugCommandKind::EffectLab,
            static_cast<uint8_t>(EffectLabAction::SpawnAllies));
    }
  }
  if (effectLab) {
    static const std::pair<ImGuiKey, EffectLabAction> LAB_KEYS[] = {
        {ImGuiKey_1, EffectLabAction::SpawnCrowd},
        {ImGuiKey_2, EffectLabAction::Poison},
        {ImGuiKey_3, EffectLabAction::AmplifyPoison},
        {ImGuiKey_4, EffectLabAction::SpreadPoison},
        {ImGuiKey_5, EffectLabAction::DetonatePoison},
        {ImGuiKey_6, EffectLabAction::Empower},
        {ImGuiKey_7, EffectLabAction::AmplifyEmpower},
        {ImGuiKey_8, EffectLabAction::SpreadEmpower},
        {ImGuiKey_9, EffectLabAction::EnrageEnemy},
        {ImGuiKey_0, EffectLabAction::Corrupt},
        {ImGuiKey_Minus, EffectLabAction::AfflictAllies},
        {ImGuiKey_Equal, EffectLabAction::Purify},
        {ImGuiKey_F12, EffectLabAction::SpawnAllies},
    };
    for (const auto& [key, action] : LAB_KEYS) {
      if (key == ImGuiKey_F12) continue;
      if (ImGui::IsKeyPressed(key, false)) {
        debug(DebugCommandKind::EffectLab, static_cast<uint8_t>(action));
      }
    }
  }
}

void ArenaView::drawEffectLabPanel() {
  if (!effectLab) return;
  static const char* LINES[] = {
      "EFFECT LAB  (F12 to close)",
      "1  spawn a crowd at the cursor",
      "2  poison    3  amplify x4",
      "4  spread    5  detonate all",
      "6  empower yourself   7  amplify x5",
      "8  share it with allies",
      "9  enrage an enemy   0  corrupt it",
      "-  afflict allies    =  purify them",
  };
  ImDrawList* dl = ImGui::GetForegroundDrawList();
  float x = 14.0f, y = 120.0f;
  dl->AddRectFilled(ImVec2(x - 6, y - 6), ImVec2(x + 236, y + 8 * 15 + 4),
                    IM_COL32(10, 12, 20, 190), 4.0f);
  for (int i = 0; i < 8; i++) {
    text(dl, ImVec2(x, y + i * 15.0f),
         i == 0 ? IM_COL32(255, 220, 120, 255) : IM_COL32(220, 225, 235, 255),
         12.0f, LINES[i], false);
  }
}

// ---------------------------------------------------------------------------
// Ability bar + own health
// ---------------------------------------------------------------------------

void ArenaView::drawAbilityBar() {
  auto it = arena->players().find(localId());
  if (it == arena->players().end()) return;
  const PlayerState& state = it->second.state;
  const Arena::KitInfo* kit = Arena::kitInfo(state.characterId);
  if (!kit) return;
  const MatchPlayerState* me = arena->matchPlayer(localId());
  Genus core = me ? static_cast<Genus>(me->coreGenus) : Genus::None;

  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const float box = 58.0f, gap = 10.0f;
  const float totalWidth = Arena::SLOT_COUNT * box + (Arena::SLOT_COUNT - 1) * gap;
  const float left = (SCREEN_W - totalWidth) * 0.5f;
  const float top = SCREEN_H - box - 30.0f;

  // Health
  {
    const float width = totalWidth, height = 16.0f;
    ImVec2 at(left, top - 30.0f);
    float fraction = state.health / std::max(1.0f, state.maxHealth);
    ImU32 fill = fraction > 0.35f ? IM_COL32(70, 210, 100, 255)
                                  : IM_COL32(230, 70, 60, 255);
    bar(dl, at, width, height, fraction, fill, IM_COL32(10, 12, 20, 220));
    if (state.shield > 0.0f) {
      float shield = std::min(1.0f, state.shield / std::max(1.0f, state.maxHealth));
      dl->AddRectFilled(ImVec2(at.x, at.y - 5.0f),
                        ImVec2(at.x + width * shield, at.y - 1.0f),
                        IM_COL32(190, 230, 255, 255));
    }
    char buffer[48];
    if (state.shield > 0.5f) {
      std::snprintf(buffer, sizeof(buffer), "%.0f / %.0f  (+%.0f)", state.health,
                    state.maxHealth, state.shield);
    } else {
      std::snprintf(buffer, sizeof(buffer), "%.0f / %.0f", state.health,
                    state.maxHealth);
    }
    text(dl, ImVec2(left + width * 0.5f, at.y + 1.0f),
         IM_COL32(255, 255, 255, 255), 13.0f, buffer);
    text(dl, ImVec2(left, at.y - 20.0f), IM_COL32(255, 255, 255, 230), 14.0f,
         (std::string(kit->name) + "  -  " + Arena::archetypeName(kit->archetype))
             .c_str(),
         false);
  }

  // Own effects, as chips above the health bar
  if (effects) {
    float x = left + totalWidth;
    char buffer[40];
    for (const EffectInstance& effect : effects->getEffects(localId(), false)) {
      bool buff = EffectRegistry::getCategory(effect.type) == EffectCategory::Buff;
      if (effect.stacks > 1) {
        std::snprintf(buffer, sizeof(buffer), "%s x%d",
                      EffectRegistry::getName(effect.type), effect.stacks);
      } else {
        std::snprintf(buffer, sizeof(buffer), "%s",
                      EffectRegistry::getName(effect.type));
      }
      ImVec2 size = ImGui::GetFont()->CalcTextSizeA(13.0f, 1000.0f, 0.0f, buffer);
      x -= size.x + 14.0f;
      ImVec2 a(x, top - 56.0f), b(x + size.x + 10.0f, top - 37.0f);
      dl->AddRectFilled(a, b, buff ? IM_COL32(70, 60, 20, 220)
                                   : IM_COL32(80, 24, 60, 220), 4.0f);
      dl->AddRect(a, b, buff ? IM_COL32(255, 220, 110, 255)
                             : IM_COL32(255, 130, 200, 255), 4.0f);
      text(dl, ImVec2(a.x + 5.0f, a.y + 3.0f), IM_COL32(255, 255, 255, 255),
           13.0f, buffer, false);
    }
  }

  // Slots
  ImVec2 mouse = ImGui::GetIO().MousePos;
  for (int slot = 0; slot < Arena::SLOT_COUNT; slot++) {
    bool isCore = slot == static_cast<int>(Arena::AbilitySlot::Core);
    const Arena::AbilityInfo& ability =
        isCore ? Arena::coreAbility(core) : kit->abilities[slot];
    bool available = !isCore || core != Genus::None;

    ImVec2 a(left + slot * (box + gap), top);
    ImVec2 b(a.x + box, a.y + box);
    ImU32 accent = isCore ? genusColor(core) : IM_COL32(255, 255, 255, 255);
    if (isCore && core == Genus::None) accent = IM_COL32(120, 120, 130, 255);

    dl->AddRectFilled(a, b, IM_COL32(18, 20, 32, 230), 6.0f);

    // Name inside the box, wrapped to two short lines.
    if (available) {
      std::string name = ability.name;
      size_t split = name.find(' ', name.size() / 3);
      std::string line1 = split == std::string::npos ? name : name.substr(0, split);
      std::string line2 = split == std::string::npos ? "" : name.substr(split + 1);
      if (line1.size() > 9) line1 = line1.substr(0, 8) + ".";
      if (line2.size() > 9) line2 = line2.substr(0, 8) + ".";
      text(dl, ImVec2(a.x + box * 0.5f, a.y + 18.0f), withAlpha(accent, 0.95f),
           11.0f, line1.c_str());
      if (!line2.empty()) {
        text(dl, ImVec2(a.x + box * 0.5f, a.y + 30.0f), withAlpha(accent, 0.95f),
             11.0f, line2.c_str());
      }
    } else {
      text(dl, ImVec2(a.x + box * 0.5f, a.y + 22.0f),
           IM_COL32(150, 150, 160, 255), 11.0f, "no core");
    }

    // Cooldown shade drains from the top.
    float remaining = state.cooldowns[slot] * 10.0f;
    if (available && remaining > 0.0f && ability.cooldownMs > 0.0f) {
      float fraction = std::min(1.0f, remaining / ability.cooldownMs);
      dl->AddRectFilled(a, ImVec2(b.x, a.y + box * fraction),
                        IM_COL32(0, 0, 0, 170), 6.0f);
      if (remaining >= 950.0f) {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%.0f", std::ceil(remaining / 1000.0f));
        text(dl, ImVec2(a.x + box * 0.5f, a.y + 38.0f),
             IM_COL32(255, 255, 255, 255), 16.0f, buffer);
      }
    }

    bool charging = state.chargeSlot == slot;
    if (charging) {
      float progress = state.charge / 255.0f;
      dl->AddRectFilled(ImVec2(a.x, b.y - 6.0f),
                        ImVec2(a.x + box * progress, b.y),
                        IM_COL32(255, 210, 90, 255), 3.0f);
    }
    bool ready = available && remaining <= 0.0f;
    dl->AddRect(a, b,
                charging ? IM_COL32(255, 210, 90, 255)
                         : (ready ? withAlpha(accent, 0.9f)
                                  : IM_COL32(90, 90, 100, 255)),
                6.0f, 0, charging ? 3.0f : 2.0f);

    // Key cap
    text(dl, ImVec2(a.x + box * 0.5f, b.y + 4.0f), IM_COL32(230, 230, 240, 255),
         12.0f, SLOT_KEYS[slot]);

    if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(320.0f);
      ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.5f, 1.0f), "%s  [%s]",
                         ability.name, SLOT_KEYS[slot]);
      ImGui::TextUnformatted(ability.description);
      if (ability.cooldownMs > 0.0f) {
        ImGui::TextDisabled("Cooldown %.1fs", ability.cooldownMs / 1000.0f);
      }
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  }

  // Passive, left of the bar
  {
    ImVec2 a(left - 190.0f, top + 4.0f), b(left - 14.0f, top + box - 4.0f);
    panel(dl, a, b, 0.6f);
    text(dl, ImVec2(a.x + 8.0f, a.y + 5.0f), IM_COL32(255, 220, 128, 255), 12.0f,
         (std::string("Passive: ") + kit->passiveName).c_str(), false);
    if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(320.0f);
      ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.5f, 1.0f), "%s", kit->passiveName);
      ImGui::TextUnformatted(kit->passiveDescription);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
    // Infusions summary
    if (me) {
      int unslotted = me->inventory[0] + me->inventory[1] + me->inventory[2] +
                      me->inventory[3];
      char buffer[64];
      std::snprintf(buffer, sizeof(buffer), "Infusions in pack: %d", unslotted);
      text(dl, ImVec2(a.x + 8.0f, a.y + 22.0f),
           unslotted > 0 ? IM_COL32(150, 255, 190, 255)
                         : IM_COL32(170, 170, 180, 255),
           12.0f, buffer, false);
      text(dl, ImVec2(a.x + 8.0f, a.y + 36.0f), IM_COL32(150, 150, 165, 255),
           11.0f, "[B] suit   [TAB] scores", false);
    }
  }
}

// ---------------------------------------------------------------------------
// Round / zone / ship
// ---------------------------------------------------------------------------

void ArenaView::drawStatusBar() {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const MatchStatePacket& match = arena->match();
  MatchPhase phase = arena->phase();

  ImVec2 a(SCREEN_W * 0.5f - 190.0f, 8.0f), b(SCREEN_W * 0.5f + 190.0f, 70.0f);
  panel(dl, a, b);

  char title[64];
  ImU32 titleColor = IM_COL32(255, 255, 255, 255);
  switch (phase) {
    case MatchPhase::Lobby:
      std::snprintf(title, sizeof(title), "CRASH SITE");
      break;
    case MatchPhase::Round:
      std::snprintf(title, sizeof(title), "ROUND %d / %d     %s", match.round,
                    match.roundCount, formatTime(match.timeRemaining).c_str());
      if (match.timeRemaining < 25.0f) titleColor = IM_COL32(255, 120, 90, 255);
      break;
    case MatchPhase::Intermission:
      std::snprintf(title, sizeof(title), "NIGHT %d     %s", match.round,
                    formatTime(match.timeRemaining).c_str());
      titleColor = IM_COL32(160, 200, 255, 255);
      break;
    case MatchPhase::BossFight:
      std::snprintf(title, sizeof(title), "FINAL STAND");
      titleColor = IM_COL32(255, 110, 110, 255);
      break;
    case MatchPhase::Victory:
      std::snprintf(title, sizeof(title), "VICTORY");
      titleColor = IM_COL32(140, 255, 160, 255);
      break;
    case MatchPhase::Defeat:
      std::snprintf(title, sizeof(title), "DEFEAT");
      titleColor = IM_COL32(255, 110, 110, 255);
      break;
  }
  text(dl, ImVec2(SCREEN_W * 0.5f, a.y + 6.0f), titleColor, 20.0f, title);

  // Ship
  float shipFraction = match.shipHealth / std::max(1.0f, match.shipMaxHealth);
  bar(dl, ImVec2(a.x + 60.0f, a.y + 34.0f), 200.0f, 10.0f, shipFraction,
      shipFraction > 0.4f ? IM_COL32(110, 190, 255, 255)
                          : IM_COL32(255, 110, 80, 255));
  text(dl, ImVec2(a.x + 12.0f, a.y + 32.0f), IM_COL32(220, 230, 255, 255), 13.0f,
       "SHIP", false);
  char scrap[48];
  if (match.turretsOnline) {
    std::snprintf(scrap, sizeof(scrap), "Scrap %d  TURRETS ON", match.scrapBanked);
  } else {
    std::snprintf(scrap, sizeof(scrap), "Scrap %d/%d", match.scrapBanked,
                  Arena::Tuning::SHIP_TURRET_SCRAP);
  }
  text(dl, ImVec2(a.x + 272.0f, a.y + 32.0f),
       match.turretsOnline ? IM_COL32(150, 255, 190, 255)
                           : IM_COL32(255, 220, 150, 255),
       13.0f, scrap, false);

  // Distance to the ship and whether we are inside the zone
  float px, py;
  localPosition(px, py);
  float dx = match.shipX - px, dy = match.shipY - py;
  float shipDistance = std::sqrt(dx * dx + dy * dy);
  bool fighting = phase == MatchPhase::Round || phase == MatchPhase::BossFight;
  if (fighting) {
    char line[64];
    std::snprintf(line, sizeof(line), "Ship %.0fm   Zone edge %.0fm",
                  shipDistance / 10.0f,
                  std::max(0.0f, match.zoneRadius - shipDistance) / 10.0f);
    text(dl, ImVec2(SCREEN_W * 0.5f, a.y + 48.0f), IM_COL32(190, 200, 220, 255),
         12.0f, line);

    if (shipDistance > match.zoneRadius) {
      float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 8.0f);
      text(dl, ImVec2(SCREEN_W * 0.5f, 120.0f),
           IM_COL32(255, 90, 80, static_cast<int>(170 + 85 * pulse)), 24.0f,
           "OUTSIDE THE BLUE ZONE");
    }

    // Arrow at the screen edge pointing home when the ship is off screen.
    ImVec2 shipScreen = toZoomedScreen(match.shipX, match.shipY);
    if (shipScreen.x < 0 || shipScreen.x > SCREEN_W || shipScreen.y < 0 ||
        shipScreen.y > SCREEN_H) {
      float angle = std::atan2(dy, dx);
      ImVec2 centre(SCREEN_W * 0.5f, SCREEN_H * 0.5f);
      float rx = SCREEN_W * 0.5f - 60.0f, ry = SCREEN_H * 0.5f - 110.0f;
      ImVec2 tip(centre.x + std::cos(angle) * rx, centre.y + std::sin(angle) * ry);
      ImVec2 left(tip.x + std::cos(angle + 2.6f) * 18.0f,
                  tip.y + std::sin(angle + 2.6f) * 18.0f);
      ImVec2 right(tip.x + std::cos(angle - 2.6f) * 18.0f,
                   tip.y + std::sin(angle - 2.6f) * 18.0f);
      dl->AddTriangleFilled(tip, left, right, IM_COL32(130, 220, 255, 230));
      dl->AddTriangle(tip, left, right, IM_COL32(10, 20, 40, 255), 2.0f);
      text(dl, ImVec2(tip.x - std::cos(angle) * 34.0f,
                      tip.y - std::sin(angle) * 34.0f - 7.0f),
           IM_COL32(170, 230, 255, 255), 12.0f, "SHIP");
    }
  }

  // Boss health
  if (match.bossId != 0) {
    auto it = arena->enemies().find(match.bossId);
    if (it != arena->enemies().end() && !it->second.dying) {
      const NetworkEnemyState& boss = it->second.state;
      float fraction = boss.health / std::max(1.0f, boss.maxHealth);
      ImVec2 at(SCREEN_W * 0.5f - 260.0f, 84.0f);
      bar(dl, at, 520.0f, 14.0f, fraction, IM_COL32(190, 70, 230, 255));
      text(dl, ImVec2(SCREEN_W * 0.5f, at.y + 16.0f), IM_COL32(230, 180, 255, 255),
           14.0f, Arena::enemyDef(Arena::EnemyKind::Boss).name);
    }
  }
}

// ---------------------------------------------------------------------------
// Scoreboard
// ---------------------------------------------------------------------------

void ArenaView::drawScoreboard(bool expanded) {
  const MatchStatePacket& match = arena->match();
  std::vector<const MatchPlayerState*> rows;
  for (const auto& p : match.players) {
    if (p.characterId != 0) rows.push_back(&p);
  }
  std::sort(rows.begin(), rows.end(),
            [](const MatchPlayerState* a, const MatchPlayerState* b) {
              return a->score > b->score;
            });
  if (rows.empty()) return;

  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  uint32_t me = localId();

  if (!expanded) {
    // Compact: top right
    const float width = 190.0f, rowH = 18.0f;
    ImVec2 a(SCREEN_W - width - 10.0f, 8.0f);
    ImVec2 b(SCREEN_W - 10.0f, a.y + 10.0f + rowH * rows.size());
    panel(dl, a, b);
    for (size_t i = 0; i < rows.size(); i++) {
      const MatchPlayerState* p = rows[i];
      ImU32 colour = p->playerId == me ? IM_COL32(255, 230, 140, 255)
                                       : IM_COL32(225, 230, 245, 255);
      float y = a.y + 5.0f + rowH * i;
      char rank[8];
      std::snprintf(rank, sizeof(rank), "%zu", i + 1);
      text(dl, ImVec2(a.x + 8.0f, y), colour, 13.0f, rank, false);
      text(dl, ImVec2(a.x + 24.0f, y), colour, 13.0f,
           characterName(p->characterId), false);
      char score[16];
      std::snprintf(score, sizeof(score), "%d", p->score);
      ImVec2 size = ImGui::GetFont()->CalcTextSizeA(13.0f, 1000.0f, 0.0f, score);
      text(dl, ImVec2(b.x - 8.0f - size.x, y), colour, 13.0f, score, false);
    }
    return;
  }

  // Expanded (hold TAB)
  ImGui::SetNextWindowPos(ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("Scoreboard", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove);
  ImGui::Text("SCOREBOARD");
  ImGui::Separator();
  if (ImGui::BeginTable("scores", 6, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Character", 0, 110.0f);
    ImGui::TableSetupColumn("Score", 0, 70.0f);
    ImGui::TableSetupColumn("Kills", 0, 50.0f);
    ImGui::TableSetupColumn("Deaths", 0, 55.0f);
    ImGui::TableSetupColumn("Core", 0, 100.0f);
    ImGui::TableSetupColumn("Infusions", 0, 300.0f);
    ImGui::TableHeadersRow();
    for (const MatchPlayerState* p : rows) {
      ImGui::TableNextRow();
      ImVec4 colour = p->playerId == me ? ImVec4(1.0f, 0.9f, 0.55f, 1.0f)
                                        : ImVec4(0.9f, 0.92f, 0.98f, 1.0f);
      ImGui::TableNextColumn();
      ImGui::TextColored(colour, "%s", characterName(p->characterId));
      ImGui::TableNextColumn();
      ImGui::TextColored(colour, "%d", p->score);
      ImGui::TableNextColumn();
      ImGui::Text("%d", p->kills);
      ImGui::TableNextColumn();
      ImGui::Text("%d", p->deaths);
      ImGui::TableNextColumn();
      Genus core = static_cast<Genus>(p->coreGenus);
      if (core == Genus::None) {
        ImGui::TextDisabled("-");
      } else {
        ImGui::TextColored(genusColorV(core, 1.0f), "%s", Arena::genusName(core));
      }
      ImGui::TableNextColumn();
      bool any = false;
      for (int t = 0; t < Arena::TREE_COUNT; t++) {
        Genus g = static_cast<Genus>(p->treeGenus[t]);
        if (g == Genus::None) continue;
        if (any) ImGui::SameLine();
        ImGui::TextColored(
            genusColorV(g, 1.0f), "%s %d",
            Arena::infusionText(g, static_cast<Arena::InfusionTree>(t)).name,
            p->treeLevel[t]);
        any = true;
      }
      if (!any) ImGui::TextDisabled("-");
    }
    ImGui::EndTable();
  }
  ImGui::End();
}

// ---------------------------------------------------------------------------
// Banners
// ---------------------------------------------------------------------------

void ArenaView::drawBanners() {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  float y = 150.0f;
  for (const ArenaClient::Banner& banner : arena->banners()) {
    float in = std::min(1.0f, banner.ageMs / 180.0f);
    float out = std::min(1.0f, (banner.lifeMs - banner.ageMs) / 500.0f);
    float alpha = std::max(0.0f, std::min(in, out));
    float size = 34.0f + 8.0f * (1.0f - ease(in));
    ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, 2000.0f, 0.0f,
                                                    banner.title.c_str());
    float half = std::max(extent.x * 0.5f + 40.0f, 220.0f);
    dl->AddRectFilledMultiColor(
        ImVec2(SCREEN_W * 0.5f - half, y - 6.0f),
        ImVec2(SCREEN_W * 0.5f, y + 62.0f), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, static_cast<int>(150 * alpha)),
        IM_COL32(0, 0, 0, static_cast<int>(150 * alpha)), IM_COL32(0, 0, 0, 0));
    dl->AddRectFilledMultiColor(
        ImVec2(SCREEN_W * 0.5f, y - 6.0f),
        ImVec2(SCREEN_W * 0.5f + half, y + 62.0f),
        IM_COL32(0, 0, 0, static_cast<int>(150 * alpha)), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, static_cast<int>(150 * alpha)));
    text(dl, ImVec2(SCREEN_W * 0.5f, y), withAlpha(banner.color, alpha), size,
         banner.title.c_str());
    if (!banner.subtitle.empty()) {
      text(dl, ImVec2(SCREEN_W * 0.5f, y + 40.0f),
           IM_COL32(235, 235, 245, static_cast<int>(255 * alpha)), 15.0f,
           banner.subtitle.c_str());
    }
    y += 76.0f;
  }
}

// ---------------------------------------------------------------------------
// Minimap
// ---------------------------------------------------------------------------

void ArenaView::drawMinimap() {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const MatchStatePacket& match = arena->match();
  const float radius = 86.0f;
  ImVec2 centre(SCREEN_W - radius - 14.0f, SCREEN_H - radius - 14.0f);

  // Fit the current zone with a little margin.
  float worldRange = std::max(700.0f, match.zoneRadius * 1.15f);
  float scale = radius / worldRange;
  auto toMap = [&](float wx, float wy, bool clampToEdge) {
    float dx = (wx - match.shipX) * scale;
    float dy = (wy - match.shipY) * scale;
    float d = std::sqrt(dx * dx + dy * dy);
    if (clampToEdge && d > radius - 4.0f) {
      dx *= (radius - 4.0f) / d;
      dy *= (radius - 4.0f) / d;
    }
    return ImVec2(centre.x + dx, centre.y + dy);
  };
  auto inside = [&](ImVec2 p) {
    float dx = p.x - centre.x, dy = p.y - centre.y;
    return dx * dx + dy * dy <= radius * radius;
  };

  dl->AddCircleFilled(centre, radius, IM_COL32(12, 14, 24, 200), 48);

  // Faint quadrant tints so the map teaches which genus lives where.
  static const VfxTheme QUADS[4] = {VfxTheme::Fire, VfxTheme::Poison,
                                    VfxTheme::Psychic, VfxTheme::Ice};
  for (int i = 0; i < 4; i++) {
    float mid = i * PI * 0.5f;  // east, south, west, north
    sector(dl, centre, radius - 1.0f, mid, PI * 0.25f,
           themeColor(QUADS[i], 0.10f), 0);
  }

  // Zone
  MatchPhase phase = arena->phase();
  if (phase == MatchPhase::Round || phase == MatchPhase::BossFight) {
    dl->AddCircle(centre, std::min(radius, match.zoneRadius * scale),
                  IM_COL32(90, 170, 255, 255), 48, 2.0f);
  }
  dl->AddCircle(centre, radius, IM_COL32(255, 255, 255, 70), 48, 2.0f);

  // Objectives
  for (const auto& [id, objective] : prediction->getObjectives()) {
    ImVec2 p = toMap(objective.x, objective.y, false);
    if (!inside(p)) continue;
    ImU32 colour = objective.state == ObjectiveState::Completed
                       ? IM_COL32(110, 200, 130, 120)
                       : IM_COL32(255, 220, 90, 255);
    ImVec2 diamond[4] = {ImVec2(p.x, p.y - 4), ImVec2(p.x + 4, p.y),
                         ImVec2(p.x, p.y + 4), ImVec2(p.x - 4, p.y)};
    dl->AddConvexPolyFilled(diamond, 4, colour);
  }

  // Pickups
  for (const auto& [id, item] : prediction->getWorldItems()) {
    ImVec2 p = toMap(item.x, item.y, false);
    if (!inside(p)) continue;
    ImU32 colour = IM_COL32(220, 220, 230, 255);
    if (Arena::PickupId::isInfusion(item.itemId)) {
      colour = genusColor(Arena::PickupId::infusionGenus(item.itemId));
    }
    dl->AddRectFilled(ImVec2(p.x - 1.5f, p.y - 1.5f), ImVec2(p.x + 1.5f, p.y + 1.5f),
                      colour);
  }

  // Enemies
  for (const auto& [id, view] : arena->enemies()) {
    if (view.dying) continue;
    ImVec2 p = toMap(view.x, view.y, false);
    if (!inside(p)) continue;
    bool boss = static_cast<Arena::EnemyKind>(view.state.type) ==
                Arena::EnemyKind::Boss;
    bool raider = (view.state.flags & EnemyFlags::AttackingShip) != 0;
    dl->AddCircleFilled(p, boss ? 5.0f : 2.0f,
                        boss ? IM_COL32(210, 90, 255, 255)
                             : (raider ? IM_COL32(255, 170, 60, 255)
                                       : IM_COL32(240, 70, 70, 255)));
  }

  // Ship
  dl->AddRectFilled(ImVec2(centre.x - 4, centre.y - 3),
                    ImVec2(centre.x + 4, centre.y + 3),
                    IM_COL32(170, 225, 255, 255));

  // Players
  uint32_t me = localId();
  const Player& local = prediction->getLocalPlayer();
  for (const auto& [id, view] : arena->players()) {
    if (view.state.characterId == 0) continue;
    float x = id == me ? local.x : view.x;
    float y = id == me ? local.y : view.y;
    ImVec2 p = toMap(x, y, true);
    bool dead = view.state.health <= 0.0f;
    ImU32 colour = dead ? IM_COL32(255, 90, 90, 255)
                        : (id == me ? IM_COL32(255, 255, 255, 255)
                                    : IM_COL32(110, 200, 255, 255));
    dl->AddCircleFilled(p, id == me ? 4.0f : 3.5f, colour);
    dl->AddCircle(p, id == me ? 4.0f : 3.5f, IM_COL32(0, 0, 0, 255), 12, 1.0f);
  }

  // Compass labels: which genus lives in which direction.
  text(dl, ImVec2(centre.x, centre.y - radius + 6.0f), themeHot(VfxTheme::Ice, 0.9f),
       10.0f, "ICE");
  text(dl, ImVec2(centre.x + radius - 20.0f, centre.y - 5.0f),
       themeHot(VfxTheme::Fire, 0.9f), 10.0f, "FIRE");
  text(dl, ImVec2(centre.x, centre.y + radius - 16.0f),
       themeHot(VfxTheme::Poison, 0.9f), 10.0f, "POISON");
  text(dl, ImVec2(centre.x - radius + 24.0f, centre.y - 5.0f),
       themeHot(VfxTheme::Psychic, 0.9f), 10.0f, "PSYCHIC");
}

// ---------------------------------------------------------------------------
// Suit screen (infusions)
// ---------------------------------------------------------------------------

void ArenaView::drawSuitScreen(bool intermission) {
  const MatchPlayerState* me = arena->matchPlayer(localId());
  if (!me) return;
  const MatchStatePacket& match = arena->match();
  Genus core = static_cast<Genus>(me->coreGenus);

  auto slotInfusion = [&](int genus, int slot) {
    InfusionSlotPacket packet;
    packet.genus = static_cast<uint8_t>(genus);
    packet.slot = static_cast<uint8_t>(slot);
    prediction->sendToServer(serialize(packet));
  };

  // Drop the selection once that genus runs out.
  if (selectedGenus != 0 && me->inventory[selectedGenus - 1] == 0) {
    selectedGenus = 0;
  }
  if (selectedGenus == 0) {
    for (int g = 0; g < Arena::GENUS_COUNT; g++) {
      if (me->inventory[g] > 0) {
        selectedGenus = g + 1;
        break;
      }
    }
  }

  ImGui::SetNextWindowPos(ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 20.0f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(880.0f, 440.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.94f);
  ImGui::Begin("Suit", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);

  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("SUIT INFUSIONS");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::SameLine(520.0f);
  if (intermission) {
    ImGui::Text("Next round in %s", formatTime(match.timeRemaining).c_str());
    ImGui::SameLine(740.0f);
    bool ready = me->ready != 0;
    ImGui::PushStyleColor(ImGuiCol_Button, ready ? ImVec4(0.2f, 0.55f, 0.3f, 1.0f)
                                                 : ImVec4(0.6f, 0.4f, 0.1f, 1.0f));
    if (ImGui::Button(ready ? "READY  [R]" : "READY UP  [R]", ImVec2(124.0f, 28.0f))) {
      PlayerReadyPacket packet;
      packet.ready = ready ? 0 : 1;
      prediction->sendToServer(serialize(packet));
    }
    ImGui::PopStyleColor();
  } else {
    ImGui::TextDisabled("Only works beside the ship.  [B] closes");
  }
  ImGui::TextDisabled(
      "Infusions drop from aliens. Your first one powers the CORE (ability on "
      "E); the rest go into one of four trees.");
  ImGui::Separator();

  // --- inventory ---------------------------------------------------------
  ImGui::Text("In your pack (click to select):");
  for (int g = 0; g < Arena::GENUS_COUNT; g++) {
    Genus genus = static_cast<Genus>(g + 1);
    int count = me->inventory[g];
    bool selected = selectedGenus == g + 1;
    if (g > 0) ImGui::SameLine();
    ImGui::PushID(g);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          genusColorV(genus, selected ? 0.75f : 0.32f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, genusColorV(genus, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          count > 0 ? ImVec4(1, 1, 1, 1) : ImVec4(1, 1, 1, 0.4f));
    char label[48];
    std::snprintf(label, sizeof(label), "%s  x%d", Arena::genusName(genus), count);
    if (ImGui::Button(label, ImVec2(205.0f, 34.0f)) && count > 0) {
      selectedGenus = g + 1;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopID();
  }
  ImGui::Spacing();

  Genus picked = static_cast<Genus>(selectedGenus);
  bool havePick = selectedGenus != 0;

  // --- core ----------------------------------------------------------------
  ImGui::BeginChild("core", ImVec2(0.0f, 86.0f), true);
  if (core == Genus::None) {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "CORE  -  empty");
    if (havePick) {
      const Arena::AbilityInfo& ability = Arena::coreAbility(picked);
      ImGui::TextColored(genusColorV(picked, 1.0f), "%s core: %s",
                         Arena::genusName(picked), ability.name);
      ImGui::TextWrapped("%s", ability.description);
      ImGui::SameLine(700.0f);
      ImGui::SetCursorPosY(26.0f);
      ImGui::PushStyleColor(ImGuiCol_Button, genusColorV(picked, 0.6f));
      if (ImGui::Button("SLOT AS CORE", ImVec2(150.0f, 40.0f))) {
        slotInfusion(selectedGenus, 0);
      }
      ImGui::PopStyleColor();
    } else {
      ImGui::TextDisabled("Kill aliens to find your first infusion.");
    }
  } else {
    const Arena::AbilityInfo& ability = Arena::coreAbility(core);
    ImGui::TextColored(genusColorV(core, 1.0f), "CORE  -  %s: %s  [E]",
                       Arena::genusName(core), ability.name);
    ImGui::TextWrapped("%s", ability.description);
  }
  ImGui::EndChild();

  // --- trees -----------------------------------------------------------------
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    Arena::InfusionTree tree = static_cast<Arena::InfusionTree>(t);
    Genus current = static_cast<Genus>(me->treeGenus[t]);
    int level = me->treeLevel[t];
    if (t > 0) ImGui::SameLine();
    ImGui::PushID(100 + t);
    ImGui::BeginChild("tree", ImVec2(208.0f, 196.0f), true);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s",
                       Arena::treeName(tree));
    ImGui::Separator();
    if (current != Genus::None) {
      const Arena::InfusionText& info = Arena::infusionText(current, tree);
      ImGui::TextColored(genusColorV(current, 1.0f), "%s  (level %d)", info.name,
                         level);
      ImGui::PushTextWrapPos(196.0f);
      ImGui::TextUnformatted(info.description);
      ImGui::PopTextWrapPos();
    } else {
      ImGui::TextDisabled("empty");
    }

    if (havePick && core != Genus::None) {
      const Arena::InfusionText& preview = Arena::infusionText(picked, tree);
      bool upgrade = current == picked;
      bool maxed = upgrade && level >= Arena::MAX_TREE_LEVEL;
      ImGui::SetCursorPosY(118.0f);
      ImGui::Separator();
      if (!upgrade) {
        ImGui::TextColored(genusColorV(picked, 0.9f), "-> %s", preview.name);
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::PushTextWrapPos(300.0f);
          ImGui::TextUnformatted(preview.description);
          if (current != Genus::None) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f),
                               "Replaces what is slotted here.");
          }
          ImGui::PopTextWrapPos();
          ImGui::EndTooltip();
        }
      }
      ImGui::SetCursorPosY(156.0f);
      ImGui::PushStyleColor(ImGuiCol_Button, genusColorV(picked, maxed ? 0.2f : 0.55f));
      const char* label = maxed ? "MAX LEVEL"
                                : (upgrade ? "UPGRADE" : (current == Genus::None
                                                              ? "SLOT HERE"
                                                              : "REPLACE"));
      if (ImGui::Button(label, ImVec2(190.0f, 30.0f)) && !maxed) {
        slotInfusion(selectedGenus, t + 1);
      }
      ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopID();
  }
  ImGui::End();
}

// ---------------------------------------------------------------------------
// Lobby / end / downed / prompts
// ---------------------------------------------------------------------------

void ArenaView::drawLobby() {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const MatchStatePacket& match = arena->match();
  int total = static_cast<int>(match.players.size());
  int picked = 0;
  for (const auto& p : match.players) {
    if (p.characterId != 0) picked++;
  }
  ImVec2 a(SCREEN_W * 0.5f - 260.0f, 200.0f), b(SCREEN_W * 0.5f + 260.0f, 300.0f);
  panel(dl, a, b, 0.8f);
  if (picked == total && match.timeRemaining > 0.0f) {
    char line[48];
    std::snprintf(line, sizeof(line), "DEPLOYING IN %.0f",
                  std::ceil(match.timeRemaining));
    text(dl, ImVec2(SCREEN_W * 0.5f, a.y + 18.0f), IM_COL32(255, 230, 150, 255),
         30.0f, line);
  } else {
    text(dl, ImVec2(SCREEN_W * 0.5f, a.y + 18.0f), IM_COL32(255, 255, 255, 255),
         24.0f, "WAITING FOR THE CREW");
  }
  char line[64];
  std::snprintf(line, sizeof(line), "%d of %d players have picked a character",
                picked, total);
  text(dl, ImVec2(SCREEN_W * 0.5f, a.y + 62.0f), IM_COL32(200, 210, 230, 255),
       14.0f, line);
}

void ArenaView::drawEndScreen() {
  const MatchStatePacket& match = arena->match();
  bool victory = arena->phase() == MatchPhase::Victory;
  const MatchPlayerState* me = arena->matchPlayer(localId());

  std::vector<const MatchPlayerState*> rows;
  for (const auto& p : match.players) {
    if (p.characterId != 0) rows.push_back(&p);
  }
  std::sort(rows.begin(), rows.end(),
            [](const MatchPlayerState* a, const MatchPlayerState* b) {
              return a->score > b->score;
            });

  ImGui::SetNextWindowPos(ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.94f);
  ImGui::Begin("End", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::SetWindowFontScale(2.4f);
  ImGui::TextColored(victory ? ImVec4(0.55f, 1.0f, 0.62f, 1.0f)
                             : ImVec4(1.0f, 0.42f, 0.42f, 1.0f),
                     victory ? "VICTORY" : "DEFEAT");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::TextDisabled(victory ? "The Matriarch is dead. You made it off-world."
                              : (match.shipHealth <= 0.0f
                                     ? "The ship was destroyed."
                                     : "The whole crew went down."));
  ImGui::Separator();
  if (ImGui::BeginTable("final", 5, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("#", 0, 24.0f);
    ImGui::TableSetupColumn("Character", 0, 150.0f);
    ImGui::TableSetupColumn("Score", 0, 90.0f);
    ImGui::TableSetupColumn("Kills", 0, 70.0f);
    ImGui::TableSetupColumn("Deaths", 0, 70.0f);
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < rows.size(); i++) {
      const MatchPlayerState* p = rows[i];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%zu", i + 1);
      ImGui::TableNextColumn();
      ImGui::TextColored(p->playerId == localId()
                             ? ImVec4(1.0f, 0.9f, 0.55f, 1.0f)
                             : ImVec4(0.9f, 0.92f, 0.98f, 1.0f),
                         "%s%s", characterName(p->characterId),
                         p->ready ? "  (ready)" : "");
      ImGui::TableNextColumn();
      ImGui::Text("%d", p->score);
      ImGui::TableNextColumn();
      ImGui::Text("%d", p->kills);
      ImGui::TableNextColumn();
      ImGui::Text("%d", p->deaths);
    }
    ImGui::EndTable();
  }
  ImGui::Spacing();
  bool ready = me && me->ready;
  if (ImGui::Button(ready ? "WAITING FOR CREW...  [R]" : "PLAY AGAIN  [R]",
                    ImVec2(500.0f, 34.0f))) {
    PlayerReadyPacket packet;
    packet.ready = ready ? 0 : 1;
    prediction->sendToServer(serialize(packet));
  }
  ImGui::End();
}

void ArenaView::drawDownedOverlay() {
  auto it = arena->players().find(localId());
  if (it == arena->players().end()) return;
  if (it->second.state.characterId == 0 || it->second.state.health > 0.0f) return;

  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(SCREEN_W, SCREEN_H),
                    IM_COL32(60, 0, 0, 70));
  text(dl, ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 120.0f),
       IM_COL32(255, 110, 110, 255), 40.0f, "YOU ARE DOWN");
  const MatchPlayerState* me = arena->matchPlayer(localId());
  if (me && me->reviveProgress > 0.0f) {
    bar(dl, ImVec2(SCREEN_W * 0.5f - 120.0f, SCREEN_H * 0.5f - 66.0f), 240.0f,
        12.0f, me->reviveProgress, IM_COL32(120, 255, 170, 255));
    text(dl, ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 50.0f),
         IM_COL32(180, 255, 200, 255), 15.0f, "Being revived...");
  } else {
    text(dl, ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 70.0f),
         IM_COL32(235, 235, 245, 255), 15.0f,
         "A crewmate can revive you by standing over you. Otherwise you are "
         "back next round.");
  }
}

void ArenaView::drawPrompts() {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  MatchPhase phase = arena->phase();
  if (phase != MatchPhase::Round && phase != MatchPhase::BossFight) return;
  const MatchStatePacket& match = arena->match();
  const MatchPlayerState* me = arena->matchPlayer(localId());
  float px, py;
  localPosition(px, py);
  const float y = SCREEN_H - 176.0f;

  const char* prompt = nullptr;
  ImU32 colour = IM_COL32(255, 235, 150, 255);
  for (const auto& [id, objective] : prediction->getObjectives()) {
    float dx = px - objective.x, dy = py - objective.y;
    bool inRange = dx * dx + dy * dy <= objective.radius * objective.radius;
    if (objective.state == ObjectiveState::Inactive && inRange) {
      prompt = objective.type == ObjectiveType::AlienScrapyard
                   ? "[F] Salvage scrap  (stay close while you work)"
                   : "[F] Start objective  (stay close while you work)";
    } else if (objective.state == ObjectiveState::InProgress && inRange) {
      prompt = "Working... hold your ground";
      colour = IM_COL32(150, 220, 255, 255);
    } else if (objective.state == ObjectiveState::ReadyToDeposit) {
      float sx = px - objective.depositX, sy = py - objective.depositY;
      if (sx * sx + sy * sy <= objective.radius * objective.radius) {
        prompt = "[F] Deliver the scrap to the ship";
      } else if (!prompt) {
        prompt = "Scrap salvaged: haul it back to the ship and press [F]";
        colour = IM_COL32(255, 190, 110, 255);
      }
    }
  }
  if (!prompt && me && me->carryingScrap) {
    prompt = "Carrying scrap: walk it to the ship";
    colour = IM_COL32(255, 190, 110, 255);
  }
  if (!prompt && me && me->coreGenus == 0) {
    int unslotted = me->inventory[0] + me->inventory[1] + me->inventory[2] +
                    me->inventory[3];
    float dx = px - match.shipX, dy = py - match.shipY;
    if (unslotted > 0 &&
        dx * dx + dy * dy <= Arena::Tuning::SHIP_SAFE_RADIUS *
                                 Arena::Tuning::SHIP_SAFE_RADIUS) {
      prompt = "[B] You have an infusion: open your suit beside the ship";
      colour = IM_COL32(150, 255, 190, 255);
    }
  }
  if (prompt) text(dl, ImVec2(SCREEN_W * 0.5f, y), colour, 16.0f, prompt);
}
