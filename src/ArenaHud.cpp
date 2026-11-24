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

bool ArenaView::closeSuitScreen() {
  if (!suitScreenOpen) return false;
  // During intermission at the ship the suit screen is the night itself;
  // Esc then falls through to the pause menu.
  if (arena->hasMatch() && arena->phase() == MatchPhase::Intermission) {
    return false;
  }
  suitScreenOpen = false;
  return true;
}

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
  drawCameraHud();
  drawEffectLabPanel();
  drawBanners();

  if (phase == MatchPhase::Lobby) drawLobby();
  if (phase == MatchPhase::Victory || phase == MatchPhase::Defeat) {
    drawEndScreen();
  }
  // A new ship tier: step back from the suit for a moment so everyone sees
  // what the scrap built.
  if (arena->hasMatch()) {
    int tier = arena->match().shipTier;
    if (tier > lastShipTier) shipRevealUntil = timeSeconds + 3.5f;
    lastShipTier = tier;
  }
  trackAscensions();
  if (phase == MatchPhase::Intermission) {
    if (!localAtShip()) {
      drawCampPanel();
    } else if (timeSeconds >= shipRevealUntil) {
      drawSuitScreen(true);
    }
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
  // U: spend the crew's banked scrap on the ship's next tier (intermission).
  if (ImGui::IsKeyPressed(ImGuiKey_U, false) && canUpgradeShip()) {
    prediction->sendToServer(serialize(ShipUpgradePacket()));
  }

  // N: tour the planet. Frees the camera and jumps it to the next
  // landmark, the city and the far places, so the map can be looked at
  // without walking it (for reviewing the scenery; Y takes the camera back).
  if (ImGui::IsKeyPressed(ImGuiKey_N, false) && props) {
    std::vector<std::pair<float, float>> stops;
    float cityX = 0.0f, cityY = 0.0f;
    int city = 0;
    for (const MapProp& p : *props) {
      if (p.kind == "city_block") {
        cityX += p.x;
        cityY += p.y;
        city++;
      } else if (p.kind == "monolith" || p.kind == "spire" ||
                 p.kind == "ribcage" || p.kind == "crater" ||
                 p.kind == "freighter") {
        stops.push_back({p.x, p.y});
      }
    }
    if (city > 0) stops.insert(stops.begin(), {cityX / city, cityY / city});
    if (!stops.empty()) {
      tourStop = (tourStop + 1) % static_cast<int>(stops.size());
      if (camera->isLocked()) camera->toggleMode();
      camera->x = stops[tourStop].first;
      camera->y = stops[tourStop].second - liftAt(stops[tourStop].first,
                                                   stops[tourStop].second);
    }
  }
  // F3: fog of war on / off, to compare.
  if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) fogEnabled = !fogEnabled;
  // F4: the look on / off, to compare with the plain render.
  if (ImGui::IsKeyPressed(ImGuiKey_F4, false) && presentation) {
    presentation->toggle();
  }

  // Playtest shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
    debug(DebugCommandKind::RoofTurrets);
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
      std::string name = ability.shortName ? ability.shortName : ability.name;
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
    const bool kade = state.characterId == Arena::CharacterId::Kade;
    if (kade && slot == 0) {
      // Prismatic Advantage: his mirror charges, on the box's top edge.
      // Lit for a charge ready; the next one fills from the bottom as it
      // comes back.
      const int ready = Arena::KadeMode::charges(state.resource);
      const float progress = Arena::KadeMode::rechargeProgress(state.resource);
      const float pw = 9.0f, ph = 13.0f, pg = 5.0f;
      float x0 = a.x + box * 0.5f - (3.0f * pw + 2.0f * pg) * 0.5f;
      for (int i = 0; i < Arena::KadeMode::MAX_MIRRORS; i++) {
        ImVec2 p0(x0 + i * (pw + pg), a.y - ph * 0.5f);
        ImVec2 p1(p0.x + pw, p0.y + ph);
        dl->AddRectFilled(p0, p1, IM_COL32(10, 12, 24, 240), 2.0f);
        float fill = i < ready ? 1.0f : (i == ready ? progress : 0.0f);
        if (fill > 0.0f) {
          float top = p1.y - 1.5f - (ph - 3.0f) * fill;
          dl->AddRectFilled(ImVec2(p0.x + 1.5f, top), ImVec2(p1.x - 1.5f, p1.y - 1.5f),
                            i < ready ? IM_COL32(90, 160, 255, 255)
                                      : IM_COL32(60, 100, 170, 200),
                            1.5f);
        }
        dl->AddRect(p0, p1, IM_COL32(220, 214, 200, 255), 2.0f, 0, 1.5f);
      }
      if (ready == 0 && progress < 1.0f) {
        // Out of charges: the box reads as cooling down.
        float left = (1.0f - progress) * Arena::KadeMode::CHARGE_MS;
        dl->AddRectFilled(a, ImVec2(b.x, a.y + box * (1.0f - progress)),
                          IM_COL32(0, 0, 0, 170), 6.0f);
        char secs[8];
        std::snprintf(secs, sizeof(secs), "%.0f", std::ceil(left / 1000.0f));
        text(dl, ImVec2(a.x + box * 0.5f, a.y + 38.0f), IM_COL32(255, 255, 255, 255),
             16.0f, secs);
      }
    }
    if (state.characterId == Arena::CharacterId::Jeff && slot == 0) {
      // Biomass Reactor: the beam's heat along the box's top edge. Venting,
      // it blinks red and the box reads as a reload.
      const float heat = state.resource / 255.0f;
      const bool venting = Arena::JeffMode::overheated(state.kitMode);
      ImVec2 h0(a.x + 4.0f, a.y - 5.0f), h1(b.x - 4.0f, a.y + 1.0f);
      dl->AddRectFilled(h0, h1, IM_COL32(10, 12, 24, 240), 2.0f);
      if (heat > 0.0f) {
        bool blink = venting && std::fmod(ImGui::GetTime(), 0.25) < 0.125;
        ImU32 fill = venting ? (blink ? IM_COL32(255, 70, 50, 255)
                                      : IM_COL32(150, 30, 20, 255))
                             : IM_COL32(255, static_cast<int>(200 - 130 * heat),
                                        static_cast<int>(60 - 40 * heat), 255);
        dl->AddRectFilled(ImVec2(h0.x + 1.0f, h0.y + 1.0f),
                          ImVec2(h0.x + 1.0f + (h1.x - h0.x - 2.0f) * heat, h1.y - 1.0f),
                          fill, 1.5f);
      }
      dl->AddRect(h0, h1, IM_COL32(220, 214, 200, 255), 2.0f, 0, 1.0f);
      if (venting) {
        dl->AddRectFilled(a, ImVec2(b.x, a.y + box * heat), IM_COL32(0, 0, 0, 150), 6.0f);
        text(dl, ImVec2(a.x + box * 0.5f, a.y + 38.0f), IM_COL32(255, 120, 90, 255),
             12.0f, "VENT");
      }
    }
    if (state.characterId == Arena::CharacterId::Jeff && slot == 1 &&
        Arena::JeffMode::treads(state.kitMode)) {
      // Usable on treads: it stands him up first.
      text(dl, ImVec2(a.x + box * 0.5f, a.y + 38.0f), IM_COL32(200, 200, 210, 255),
           11.0f, "legs");
    }
    if (charging && !(kade && slot == 0)) {
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
    // Long names are cut to the panel; the tooltip has the whole thing.
    std::string passive = std::string("Passive: ") + kit->passiveName;
    const float room = b.x - a.x - 16.0f;
    ImFont* font = ImGui::GetFont();
    if (font->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, passive.c_str()).x > room) {
      while (passive.size() > 12 &&
             font->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, (passive + "...").c_str()).x >
                 room) {
        passive.pop_back();
      }
      passive += "...";
    }
    text(dl, ImVec2(a.x + 8.0f, a.y + 5.0f), IM_COL32(255, 220, 128, 255), 12.0f,
         passive.c_str(), false);
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
      int unslotted = 0;
      for (const auto& genusRow : me->inventory) {
        for (const auto& partRow : genusRow) {
          for (uint8_t n : partRow) unslotted += n;
        }
      }
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
    case MatchPhase::Intro:  // the HUD is hidden under the intro
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
  // Banked scrap against the next tier's price; green when it can be bought
  // (between rounds).
  char scrap[64];
  int tier = std::min<int>(match.shipTier, Arena::Tuning::SHIP_TIER_COUNT - 1);
  bool upgradable = canUpgradeShip();
  if (tier + 1 >= Arena::Tuning::SHIP_TIER_COUNT) {
    std::snprintf(scrap, sizeof(scrap), "Scrap %d  FORT", match.scrapBanked);
  } else if (upgradable) {
    std::snprintf(scrap, sizeof(scrap), "Scrap %d/%d  [U] UPGRADE",
                  match.scrapBanked, Arena::Tuning::SHIP_TIER_COST[tier + 1]);
  } else {
    std::snprintf(scrap, sizeof(scrap), "Scrap %d/%d", match.scrapBanked,
                  Arena::Tuning::SHIP_TIER_COST[tier + 1]);
  }
  text(dl, ImVec2(a.x + 272.0f, a.y + 32.0f),
       upgradable ? IM_COL32(150, 255, 190, 255) : IM_COL32(255, 220, 150, 255),
       13.0f, scrap, false);

  // Distance to the ship and whether we are inside the zone
  float px, py;
  localPosition(px, py);
  float dx = match.shipX - px, dy = match.shipY - py;
  float shipDistance = std::sqrt(dx * dx + dy * dy);
  bool fighting = phase == MatchPhase::Round || phase == MatchPhase::BossFight;
  if (fighting) {
    const bool finale = phase == MatchPhase::BossFight;
    char line[64];
    if (finale) {
      std::snprintf(line, sizeof(line), "Ship %.0fm   Shield edge %.0fm",
                    shipDistance / 10.0f,
                    std::max(0.0f, match.zoneRadius - shipDistance) / 10.0f);
    } else {
      std::snprintf(line, sizeof(line), "Ship %.0fm", shipDistance / 10.0f);
    }
    text(dl, ImVec2(SCREEN_W * 0.5f, a.y + 48.0f), IM_COL32(190, 200, 220, 255),
         12.0f, line);

    if (finale && shipDistance > match.zoneRadius) {
      float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 8.0f);
      text(dl, ImVec2(SCREEN_W * 0.5f, 120.0f),
           IM_COL32(255, 90, 80, static_cast<int>(170 + 85 * pulse)), 24.0f,
           "OUTSIDE THE BLUE ZONE");
    }

    // Arrow at the screen edge pointing home when the ship is off screen.
    ImVec2 shipScreen = toZoomedScreen(match.shipX, match.shipY);
    if (shipScreen.x < 0 || shipScreen.x > SCREEN_W || shipScreen.y < 0 ||
        shipScreen.y > SCREEN_H) {
      // From what the screen is centred on, which is not always the hero.
      float angle =
          std::atan2(match.shipY - camera->y, match.shipX - camera->x);
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
    ImGui::TableSetupColumn("Core", 0, 170.0f);
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
      Genus genusBuffer[Arena::MAX_SUIT_NODES];
      Genus ascended = Arena::ascendedGenus(p->suitSlots(genusBuffer));
      if (ascended != Genus::None) {
        // The Ascendant title shows off the build to the crew.
        ImGui::TextColored(genusColorV(ascended, 1.0f), "%s Ascendant",
                           Arena::genusName(ascended));
      } else if (core == Genus::None) {
        ImGui::TextDisabled("-");
      } else {
        ImGui::TextColored(genusColorV(core, 1.0f), "%s", Arena::genusName(core));
      }
      ImGui::TableNextColumn();
      // Genus effects by level, and the best grade's effect (Apex
      // Mutation's bonus is not shown here).
      bool any = false;
      for (int t = 0; t < Arena::TREE_COUNT; t++) {
        for (int g = 1; g <= Arena::GENUS_COUNT; g++) {
          int level = 0, grade = 0;
          for (int n = 0; n < Arena::suitNodeCount(); n++) {
            if (p->nodeGenus[n] == g &&
                static_cast<int>(Arena::suitNode(n).tree) == t) {
              level++;
              grade = std::max<int>(grade, p->nodeGrade[n]);
            }
          }
          if (level == 0) continue;
          if (any) ImGui::SameLine();
          Genus genus = static_cast<Genus>(g);
          Arena::InfusionTree tree = static_cast<Arena::InfusionTree>(t);
          if (grade > Arena::GRADE_RAW) {
            const Arena::InfusionText& top =
                Arena::infusionText(genus, tree, grade);
            ImGui::TextColored(genusColorV(genus, 1.0f), "%s %d + %s%s",
                               Arena::infusionText(genus, tree).name, level,
                               top.name, top.built ? "" : " (soon)");
          } else {
            ImGui::TextColored(genusColorV(genus, 1.0f), "%s %d",
                               Arena::infusionText(genus, tree).name, level);
          }
          any = true;
        }
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

  const TerrainGrid* grid = terrain();
  const bool planet = grid && grid->active();
  if (planet) {
    // The planet as the crew has seen it: explored land, tinted by the
    // genus that holds it this match. Unexplored is blank: what lives out
    // there is found by going (Clarity once seen, discovery before).
    const int cells = 30;
    const float step = 2.0f * radius / cells;
    for (int j = 0; j < cells; j++) {
      for (int i = 0; i < cells; i++) {
        ImVec2 p(centre.x - radius + (i + 0.5f) * step,
                 centre.y - radius + (j + 0.5f) * step);
        if (!inside(p)) continue;
        float wx = match.shipX + (p.x - centre.x) / scale;
        float wy = match.shipY + (p.y - centre.y) / scale;
        if (!wasSeen(wx, wy)) continue;
        if (grid->regionAt(wx, wy) == 0) continue;  // sea
        Arena::Genus genus = regionGenusAt(wx, wy);
        ImU32 col = grid->blockedAt(wx, wy)
                        ? IM_COL32(70, 72, 86, 200)
                        : (genus == Arena::Genus::None
                               ? IM_COL32(60, 64, 76, 170)
                               : themeColor(genusTheme(genus), 0.30f));
        dl->AddRectFilled(ImVec2(p.x - step * 0.5f, p.y - step * 0.5f),
                          ImVec2(p.x + step * 0.5f, p.y + step * 0.5f), col);
      }
    }
  } else {
    // The prototype arena: faint quadrant tints teach which genus lives
    // where.
    static const VfxTheme QUADS[4] = {VfxTheme::Fire, VfxTheme::Poison,
                                      VfxTheme::Psychic, VfxTheme::Ice};
    for (int i = 0; i < 4; i++) {
      float mid = i * PI * 0.5f;  // east, south, west, north
      sector(dl, centre, radius - 1.0f, mid, PI * 0.25f,
             themeColor(QUADS[i], 0.10f), 0);
    }
  }

  // Zone: the finale's shield only (a round has none).
  MatchPhase phase = arena->phase();
  if (phase == MatchPhase::BossFight) {
    dl->AddCircle(centre, std::min(radius, match.zoneRadius * scale),
                  IM_COL32(90, 170, 255, 255), 48, 2.0f);
  }
  dl->AddCircle(centre, radius, IM_COL32(255, 255, 255, 70), 48, 2.0f);

  // Objectives
  // Only places the crew has seen: the rest of the map is for discovering.
  for (const auto& [id, objective] : prediction->getObjectives()) {
    if (!wasSeen(objective.x, objective.y)) continue;
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
    if (!wasSeen(item.x, item.y)) continue;
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
    if (!inSight(view.x, view.y)) continue;  // fog of war
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

  // Compass labels: which genus lives in which direction (the prototype
  // arena only; on the planet it changes every match).
  if (planet) return;
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
// Camera: which mode it is in, and where you are when it has left you
// ---------------------------------------------------------------------------

void ArenaView::drawCameraHud() {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const bool locked = camera->isLocked();
  text(dl, ImVec2(16.0f, SCREEN_H - 22.0f),
       locked ? IM_COL32(150, 160, 180, 200) : IM_COL32(255, 228, 130, 255),
       12.0f,
       locked ? "[Y] camera: locked"
              : "[Y] camera: free   edges pan   hold [SPACE] to recentre",
       false);

  // You never lose your hero: when the free camera has left them behind,
  // an arrow at the screen edge points back.
  float px, py;
  localPosition(px, py);
  ImVec2 hero = toZoomedScreen(px, py);
  if (hero.x >= 0 && hero.x <= SCREEN_W && hero.y >= 0 && hero.y <= SCREEN_H) {
    return;
  }
  float angle = std::atan2(py - camera->y, px - camera->x);
  ImVec2 centre(SCREEN_W * 0.5f, SCREEN_H * 0.5f);
  // Inside the ship arrow's ring, so the two never sit on top of each other.
  float rx = SCREEN_W * 0.5f - 110.0f, ry = SCREEN_H * 0.5f - 160.0f;
  ImVec2 tip(centre.x + std::cos(angle) * rx, centre.y + std::sin(angle) * ry);
  ImVec2 left(tip.x + std::cos(angle + 2.6f) * 20.0f,
              tip.y + std::sin(angle + 2.6f) * 20.0f);
  ImVec2 right(tip.x + std::cos(angle - 2.6f) * 20.0f,
               tip.y + std::sin(angle - 2.6f) * 20.0f);
  dl->AddTriangleFilled(tip, left, right, IM_COL32(255, 255, 255, 240));
  dl->AddTriangle(tip, left, right, IM_COL32(10, 20, 40, 255), 2.0f);
  text(dl,
       ImVec2(tip.x - std::cos(angle) * 36.0f,
              tip.y - std::sin(angle) * 36.0f - 7.0f),
       IM_COL32(255, 255, 255, 255), 12.0f, "YOU");
}

// ---------------------------------------------------------------------------
// Suit screen (infusions)
// ---------------------------------------------------------------------------
//
// Laid out like the paper sketch in the Genera and Infusion doc: the core in
// the middle, each tree growing out from it into its own corner (Offense top
// left, Defense top right, Genealogy bottom left, Mobility bottom right),
// one row of nodes per tier. The pack is on the left, a summary of what the
// suit gives on the right. Details are in hover tooltips.

namespace {

using Arena::InfusionPart;
using Arena::InfusionTree;
using Arena::NodeState;

// Which way each tree grows from the core (screen space, y down).
ImVec2 treeDirection(InfusionTree tree) {
  const float d = 0.70710678f;
  switch (tree) {
    case InfusionTree::Offense:
      return ImVec2(-d, -d);
    case InfusionTree::Defense:
      return ImVec2(d, -d);
    case InfusionTree::Genealogy:
      return ImVec2(-d, d);
    case InfusionTree::Mobility:
      return ImVec2(d, d);
  }
  return ImVec2(0.0f, 0.0f);
}

struct SuitView {
  Genus core = Genus::None;
  InfusionPart corePart = InfusionPart::Talon;
  int coreGrade = Arena::GRADE_RAW;
  Genus nodes[Arena::MAX_SUIT_NODES] = {};
  InfusionPart parts[Arena::MAX_SUIT_NODES] = {};
  uint8_t grades[Arena::MAX_SUIT_NODES] = {};
  int maxTier = 4;
  Genus ascended = Genus::None;
  Genus attuned[Arena::TREE_COUNT] = {};

  SuitView(const MatchPlayerState& p, int round)
      : maxTier(Arena::suitTierCap(round)) {
    core = static_cast<Genus>(p.coreGenus);
    corePart = static_cast<InfusionPart>(p.corePart);
    coreGrade = Arena::validGrade(p.coreGrade) ? p.coreGrade : 1;
    for (int i = 0; i < Arena::MAX_SUIT_NODES; i++) {
      nodes[i] = static_cast<Genus>(p.nodeGenus[i]);
      parts[i] = static_cast<InfusionPart>(p.nodePart[i]);
      grades[i] = p.nodeGrade[i];
    }
    ascended = Arena::ascendedGenus(slots());
    for (int t = 0; t < Arena::TREE_COUNT; t++) {
      attuned[t] = Arena::treeAttunement(slots(), static_cast<InfusionTree>(t));
    }
  }
  Arena::SuitSlots slots() const {
    Arena::SuitSlots suit;
    suit.core = core;
    suit.coreGrade = coreGrade;
    suit.nodeGenus = nodes;
    suit.nodeGrade = grades;
    return suit;
  }
  bool has(Arena::Talent talent) const {
    return nodes[static_cast<int>(talent)] != Genus::None;
  }
  NodeState state(int node) const {
    return Arena::nodeState(nodes, core != Genus::None, node, maxTier);
  }
  Arena::SlotBlock block(int node, Genus genus, int grade) const {
    return Arena::slotBlock(slots(), node, genus, grade, maxTier);
  }
  // Same rule as Player::infusionLevel, with `extra` more of `genus` in the
  // tree (to preview a slot).
  int level(Genus genus, InfusionTree tree, int extra = 0) const {
    int count = extra;
    for (int i = 0; i < Arena::suitNodeCount(); i++) {
      if (nodes[i] == genus && Arena::suitNode(i).tree == tree) count++;
    }
    if (count > 0 && genus == core && has(Arena::Talent::ApexMutation)) count++;
    return count;
  }
  // Same rule as Player::infusionGrade (0 = none of `genus` in the tree).
  int grade(Genus genus, InfusionTree tree) const {
    int best = 0;
    for (int i = 0; i < Arena::suitNodeCount(); i++) {
      if (nodes[i] == genus && Arena::suitNode(i).tree == tree) {
        best = std::max<int>(best, grades[i]);
      }
    }
    return best;
  }
  // Copies of a part's stat in the suit: each item counts its Raw inside.
  int partCount(InfusionPart part) const {
    int count = core != Genus::None && corePart == part
                    ? Arena::gradeRawCount(coreGrade)
                    : 0;
    for (int i = 0; i < Arena::suitNodeCount(); i++) {
      if (nodes[i] != Genus::None && parts[i] == part) {
        count += Arena::gradeRawCount(grades[i]);
      }
    }
    return count;
  }
};

// "Venoxarid Talon", "Refined Venoxarid Talon", "Exalted ...".
std::string itemName(Genus genus, InfusionPart part,
                     int grade = Arena::GRADE_RAW) {
  std::string name =
      std::string(Arena::genusName(genus)) + " " + Arena::partInfo(part).name;
  if (grade > Arena::GRADE_RAW) {
    name = std::string(Arena::gradeName(grade)) + " " + name;
  }
  return name;
}

// The grade pips: one dot per grade, centred on `at`. Hollow pips show
// what a node needs; filled pips show what an item is. Same everywhere.
void gradePips(ImDrawList* dl, ImVec2 at, int count, ImU32 color, bool hollow,
               float radius = 2.6f) {
  float gap = radius * 2.6f;
  float x = at.x - (count - 1) * gap * 0.5f;
  for (int k = 0; k < count; k++) {
    ImVec2 c(x + k * gap, at.y);
    if (hollow) {
      dl->AddCircle(c, radius, color, 10, 1.3f);
    } else {
      dl->AddCircleFilled(c, radius, color, 10);
    }
  }
}

// What `genus` gives in `tree` with its best item there at `grade` and the
// Raw effect at `level`: every grade up to `grade`.
void genusEffectLines(Genus genus, InfusionTree tree, int level, int grade) {
  for (int g = Arena::GRADE_RAW; g <= grade; g++) {
    const Arena::InfusionText& info = Arena::infusionText(genus, tree, g);
    if (g == Arena::GRADE_RAW) {
      ImGui::TextColored(genusColorV(genus, 1.0f), "%s  %s (level %d)",
                         Arena::treeName(tree), info.name, level);
    } else {
      ImGui::TextColored(genusColorV(genus, 1.0f), "%s  %s (%s)",
                         Arena::treeName(tree), info.name, Arena::gradeName(g));
    }
    ImGui::TextUnformatted(info.description);
    if (!info.built) {
      ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.7f, 1.0f),
                         "  (designed, not in the game yet)");
    }
  }
}

// Why the picked item cannot go into a node, in words.
std::string blockReason(Arena::SlotBlock block, const SuitView& suit, int node,
                        int grade) {
  switch (block) {
    case Arena::SlotBlock::None:
      return "";
    case Arena::SlotBlock::NodeNotOpen:
      return "This slot is not open.";
    case Arena::SlotBlock::GradeTooLow: {
      int need = Arena::tierMinGrade(Arena::suitNode(node).tier);
      return std::string("Tier ") + std::to_string(Arena::suitNode(node).tier) +
             " needs a " + Arena::gradeName(need) + " infusion; yours is " +
             Arena::gradeName(grade) +
             ". Combine two of a genus and grade in your pack to raise it.";
    }
    case Arena::SlotBlock::Attuned: {
      InfusionTree tree = Arena::suitNode(node).tree;
      return std::string(Arena::treeName(tree)) + " is attuned to " +
             Arena::genusName(suit.attuned[static_cast<int>(tree)]) +
             ": only that genus goes in it now.";
    }
    case Arena::SlotBlock::Ascended:
      return std::string("You are a ") + Arena::genusName(suit.ascended) +
             " Ascendant: no other genus can be Exalted in your suit.";
  }
  return "";
}

// A pack item being dragged onto another to combine them.
struct PackPayload {
  int genus, part, grade;
};

}  // namespace

void ArenaView::trackAscensions() {
  if (!arena->hasMatch()) return;
  for (const MatchPlayerState& p : arena->match().players) {
    Genus genusBuffer[Arena::MAX_SUIT_NODES];
    Genus now = Arena::ascendedGenus(p.suitSlots(genusBuffer));
    auto known = knownAscended.find(p.playerId);
    bool seen = known != knownAscended.end();
    uint8_t before = seen ? known->second : 0;
    knownAscended[p.playerId] = static_cast<uint8_t>(now);
    // A new match resets suits; joining mid-match is not an Ascension.
    if (!seen || now == Genus::None || before != 0) continue;
    std::string title = std::string(Arena::genusName(now)) + " ASCENDANT";
    for (char& c : title) c = static_cast<char>(std::toupper(c));
    bool mine = p.playerId == localId();
    std::string who = mine ? std::string("You are")
                           : std::string(characterName(p.characterId)) + " is";
    arena->pushBanner(title,
                      who + " Exalted in " + Arena::genusName(now) +
                          " for the rest of the match",
                      genusColor(now, 1.0f), 4000.0f);
    if (mine) {
      ascendFlashUntil = timeSeconds + 2.5f;
      ascendFlashGenus = now;
    }
  }
}

void ArenaView::drawSuitScreen(bool intermission) {
  const MatchPlayerState* me = arena->matchPlayer(localId());
  if (!me) return;
  const MatchStatePacket& match = arena->match();
  const SuitView suit(*me, match.round);

  auto sendSlot = [&](int genus, int part, int grade, int node) {
    InfusionSlotPacket packet;
    packet.genus = static_cast<uint8_t>(genus);
    packet.part = static_cast<uint8_t>(part);
    packet.grade = static_cast<uint8_t>(grade);
    packet.node = static_cast<uint8_t>(node);
    prediction->sendToServer(serialize(packet));
  };
  auto sendCombine = [&](int genus, int grade, int targetPart, int otherPart) {
    InfusionCombinePacket packet;
    packet.genus = static_cast<uint8_t>(genus);
    packet.grade = static_cast<uint8_t>(grade);
    packet.targetPart = static_cast<uint8_t>(targetPart);
    packet.otherPart = static_cast<uint8_t>(otherPart);
    prediction->sendToServer(serialize(packet));
  };

  // Pack counts by genus (1-based), part and grade.
  auto count = [&](int genus, int part, int grade) {
    if (genus < 1 || genus > Arena::GENUS_COUNT || !Arena::validGrade(grade)) {
      return 0;
    }
    return static_cast<int>(me->inventory[genus - 1][part][grade - 1]);
  };
  // Keep the selection on an item we still have.
  if (count(selectedGenus, selectedPart, selectedGrade) == 0) {
    selectedGenus = 0;
    for (int g = 1; g <= Arena::GENUS_COUNT && selectedGenus == 0; g++) {
      for (int p = 0; p < Arena::PART_COUNT && selectedGenus == 0; p++) {
        for (int grade = 1; grade <= Arena::GRADE_COUNT; grade++) {
          if (count(g, p, grade) > 0) {
            selectedGenus = g;
            selectedPart = p;
            selectedGrade = grade;
            break;
          }
        }
      }
    }
  }
  const bool havePick = selectedGenus != 0;
  const Genus picked = static_cast<Genus>(selectedGenus);
  const InfusionPart pickedPart = static_cast<InfusionPart>(selectedPart);
  const int pickedGrade = selectedGrade;

  // The selected item's combine partner: the same part if there are two,
  // else the first other part of the same genus and grade (-1 = none).
  int partner = -1;
  if (havePick && Arena::canCombine(suit.ascended, picked, pickedGrade)) {
    if (count(selectedGenus, selectedPart, pickedGrade) >= 2) {
      partner = selectedPart;
    } else {
      for (int p = 0; p < Arena::PART_COUNT && partner < 0; p++) {
        if (p != selectedPart && count(selectedGenus, p, pickedGrade) > 0) {
          partner = p;
        }
      }
    }
  }
  auto combineSelected = [&]() {
    if (partner < 0) return;
    sendCombine(selectedGenus, pickedGrade, selectedPart, partner);
    // Follow the item to its new grade.
    selectedGrade = pickedGrade + 1;
  };

  // Slot the picked item, asking first when it would Ascend the player.
  auto trySlot = [&](int node) {
    if (pickedGrade >= Arena::GRADE_EXALTED && suit.ascended == Genus::None) {
      pendingAscendNode = node;
      pendingAscendGenus = selectedGenus;
      pendingAscendPart = selectedPart;
      openAscendPopup = true;
      return;
    }
    sendSlot(selectedGenus, selectedPart, pickedGrade, node);
  };

  const float width = 1240.0f, height = 690.0f;
  ImGui::SetNextWindowPos(ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(1.0f);
  ImGui::Begin("Suit", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);

  if (ImGui::IsKeyPressed(ImGuiKey_C, false) && pendingAscendNode < 0) {
    combineSelected();
  }

  // --- header ---------------------------------------------------------------
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("SUIT INFUSIONS");
  ImGui::SetWindowFontScale(1.0f);
  if (intermission) {
    // The crew's scrap builds the ship's next tier: anyone can spend it.
    int tier =
        std::min<int>(match.shipTier, Arena::Tuning::SHIP_TIER_COUNT - 1);
    ImGui::SameLine(520.0f);
    if (tier + 1 < Arena::Tuning::SHIP_TIER_COUNT) {
      char label[64];
      std::snprintf(label, sizeof(label), "UPGRADE SHIP  %d/%d scrap  [U]",
                    match.scrapBanked, Arena::Tuning::SHIP_TIER_COST[tier + 1]);
      ImGui::BeginDisabled(!canUpgradeShip());
      if (ImGui::Button(label, ImVec2(260.0f, 28.0f))) {
        prediction->sendToServer(serialize(ShipUpgradePacket()));
      }
      ImGui::EndDisabled();
    } else {
      ImGui::TextDisabled("Ship fully upgraded  (scrap %d)", match.scrapBanked);
    }
  }
  ImGui::SameLine(860.0f);
  if (intermission) {
    ImGui::Text("Next round in %s", formatTime(match.timeRemaining).c_str());
    ImGui::SameLine(1100.0f);
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
      "Pick an item, then a glowing slot (the first is your CORE, on E). One "
      "per row. Tier 3 needs 2 dots, tier 4 needs 3: drag an item onto a twin "
      "(same genus and dots) or press C to combine.");
  ImGui::Separator();

  const float top = ImGui::GetCursorPosY();
  const float bodyHeight = height - top - 12.0f;

  // --- pack (left)
  // ------------------------------------------------------------
  ImGui::BeginChild("pack", ImVec2(230.0f, bodyHeight), true);
  ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "PACK");
  ImGui::Separator();
  float pulse =
      0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 5.0f);
  const ImGuiPayload* dragging = ImGui::GetDragDropPayload();
  const PackPayload* held =
      dragging && dragging->IsDataType("PACK_ITEM")
          ? static_cast<const PackPayload*>(dragging->Data)
          : nullptr;
  ImDrawList* packDl = ImGui::GetWindowDrawList();
  bool anyItem = false;
  for (int g = 0; g < Arena::GENUS_COUNT; g++) {
    Genus genus = static_cast<Genus>(g + 1);
    for (int grade = Arena::GRADE_RAW; grade <= Arena::GRADE_EXALTED; grade++) {
      for (int p = 0; p < Arena::PART_COUNT; p++) {
        int n = count(g + 1, p, grade);
        if (n == 0) continue;
        anyItem = true;
        InfusionPart part = static_cast<InfusionPart>(p);
        bool selected = selectedGenus == g + 1 && selectedPart == p &&
                        selectedGrade == grade;
        // Another genus's Exalted can never be slotted once Ascendant.
        bool deadWeight = grade >= Arena::GRADE_EXALTED &&
                          suit.ascended != Genus::None &&
                          suit.ascended != genus;
        ImGui::PushID((g * 16 + p) * 4 + grade);
        ImGui::PushStyleColor(ImGuiCol_Button,
                              genusColorV(genus, deadWeight ? 0.12f
                                                 : selected ? 0.8f
                                                            : 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, genusColorV(genus, 0.9f));
        std::string label = itemName(genus, part) + "  x" + std::to_string(n);
        if (ImGui::Button(label.c_str(), ImVec2(206.0f, 30.0f))) {
          selectedGenus = g + 1;
          selectedPart = p;
          selectedGrade = grade;
        }
        ImGui::PopStyleColor(2);
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        gradePips(packDl, ImVec2(b.x - 18.0f, (a.y + b.y) * 0.5f), grade,
                  IM_COL32(255, 255, 255, 235), false);

        // Drag one item onto another of the same genus and grade to combine;
        // the one dropped onto keeps its part.
        if (ImGui::BeginDragDropSource()) {
          PackPayload payload{g + 1, p, grade};
          ImGui::SetDragDropPayload("PACK_ITEM", &payload, sizeof(payload));
          ImGui::Text("Combine %s with...",
                      itemName(genus, part, grade).c_str());
          ImGui::EndDragDropSource();
        }
        bool combinable = held && held->genus == g + 1 &&
                          held->grade == grade &&
                          Arena::canCombine(suit.ascended, genus, grade) &&
                          (held->part != p || n >= 2);
        if (combinable) {
          packDl->AddRect(
              ImVec2(a.x - 2.0f, a.y - 2.0f), ImVec2(b.x + 2.0f, b.y + 2.0f),
              genusColor(genus, 0.5f + 0.5f * pulse), 4.0f, 0, 2.5f);
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* drop =
                    ImGui::AcceptDragDropPayload("PACK_ITEM")) {
              const PackPayload* from =
                  static_cast<const PackPayload*>(drop->Data);
              sendCombine(g + 1, grade, p, from->part);
              selectedGenus = g + 1;
              selectedPart = p;
              selectedGrade = grade + 1;
            }
            ImGui::EndDragDropTarget();
          }
        }

        if (ImGui::IsItemHovered() && !held) {
          // The tooltip from the doc: the item's own effect, then what its
          // genus does in each place it could go.
          ImGui::BeginTooltip();
          ImGui::PushTextWrapPos(380.0f);
          ImGui::TextColored(genusColorV(genus, 1.0f), "%s",
                             itemName(genus, part, grade).c_str());
          if (grade > Arena::GRADE_RAW) {
            ImGui::Text("Always: %s  (x%d: made of %d Raw)",
                        Arena::partInfo(part).description,
                        Arena::gradeRawCount(grade),
                        Arena::gradeRawCount(grade));
          } else {
            ImGui::Text("Always: %s", Arena::partInfo(part).description);
          }
          ImGui::Separator();
          if (suit.core == Genus::None) {
            const Arena::AbilityInfo& ability = Arena::coreAbility(genus);
            ImGui::TextColored(genusColorV(genus, 1.0f), "In the core: %s [E]",
                               ability.name);
            ImGui::TextUnformatted(ability.description);
          } else {
            ImGui::TextDisabled("In a tree, plus that slot's talent:");
            for (int t = 0; t < Arena::TREE_COUNT; t++) {
              InfusionTree tree = static_cast<InfusionTree>(t);
              Genus attuned = suit.attuned[t];
              if (attuned != Genus::None && attuned != genus) {
                ImGui::TextDisabled("%s: attuned to %s", Arena::treeName(tree),
                                    Arena::genusName(attuned));
                continue;
              }
              genusEffectLines(genus, tree, suit.level(genus, tree, 1),
                               std::max(grade, suit.grade(genus, tree)));
            }
          }
          if (deadWeight) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                               "You are a %s Ascendant: this can never be "
                               "slotted.",
                               Arena::genusName(suit.ascended));
          }
          if (grade < Arena::GRADE_EXALTED) {
            ImGui::Separator();
            if (Arena::canCombine(suit.ascended, genus, grade)) {
              ImGui::TextDisabled(
                  "Two of these (any parts) combine into a %s %s.",
                  Arena::gradeName(grade + 1), Arena::genusName(genus));
            } else {
              ImGui::TextDisabled(
                  "Cannot become Exalted: you are a %s Ascendant.",
                  Arena::genusName(suit.ascended));
            }
          }
          ImGui::PopTextWrapPos();
          ImGui::EndTooltip();
        }
        ImGui::PopID();
      }
    }
  }
  if (!anyItem) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled(
        "Empty. Kill aliens and finish objectives to find infusions; each "
        "territory drops its own genus.");
    ImGui::PopTextWrapPos();
  }
  if (havePick && pickedGrade < Arena::GRADE_EXALTED) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::BeginDisabled(partner < 0);
    char combineLabel[64];
    std::snprintf(combineLabel, sizeof(combineLabel), "COMBINE -> %s  [C]",
                  Arena::gradeName(pickedGrade + 1));
    if (ImGui::Button(combineLabel, ImVec2(206.0f, 28.0f))) combineSelected();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(340.0f);
      if (partner >= 0) {
        ImGui::Text(
            "Uses %s and %s; makes one %s.",
            itemName(picked, pickedPart, pickedGrade).c_str(),
            itemName(picked, static_cast<InfusionPart>(partner), pickedGrade)
                .c_str(),
            itemName(picked, pickedPart, pickedGrade + 1).c_str());
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                           "Permanent: it never splits back.");
      } else if (!Arena::canCombine(suit.ascended, picked, pickedGrade)) {
        ImGui::Text("You are a %s Ascendant: no other genus can be Exalted.",
                    Arena::genusName(suit.ascended));
      } else {
        ImGui::Text("Needs a second %s %s infusion.",
                    Arena::gradeName(pickedGrade), Arena::genusName(picked));
      }
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  }
  ImGui::EndChild();

  // --- suit (middle) -------------------------------------------------------
  ImGui::SameLine();
  const float suitWidth = 620.0f;
  ImGui::BeginChild("suit", ImVec2(suitWidth, bodyHeight), true,
                    ImGuiWindowFlags_NoScrollbar);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 centre(origin.x + suitWidth * 0.5f - 8.0f,
                      origin.y + bodyHeight * 0.5f - 8.0f);
  const float coreRadius = 38.0f, nodeRadius = 20.0f;
  const float firstStep = 92.0f, tierStep = 78.0f, optionGap = 92.0f;

  // Quadrant guides and tree names, as on the sketch.
  ImU32 guide = IM_COL32(255, 255, 255, 26);
  dl->AddLine(ImVec2(centre.x, origin.y),
              ImVec2(centre.x, origin.y + bodyHeight), guide);
  dl->AddLine(ImVec2(origin.x, centre.y),
              ImVec2(origin.x + suitWidth, centre.y), guide);
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    InfusionTree tree = static_cast<InfusionTree>(t);
    ImVec2 dir = treeDirection(tree);
    // In the quadrant's outer corner, clear of the branch.
    ImVec2 at(dir.x < 0.0f ? origin.x + 12.0f : origin.x + suitWidth - 112.0f,
              dir.y < 0.0f ? origin.y + 8.0f : origin.y + bodyHeight - 34.0f);
    // An attuned tree's name takes its genus colour (and so do its deep
    // slots, below).
    ImU32 nameColor = suit.attuned[t] != Genus::None
                          ? genusColor(suit.attuned[t], 1.0f)
                          : IM_COL32(255, 220, 140, 220);
    text(dl, at, nameColor, 17.0f, Arena::treeName(tree), false);
  }

  // Node positions: each tier is a row across the tree's branch.
  ImVec2 nodePos[Arena::MAX_SUIT_NODES];
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    InfusionTree tree = static_cast<InfusionTree>(t);
    ImVec2 dir = treeDirection(tree);
    ImVec2 side(-dir.y, dir.x);
    for (int tier = 1; tier <= Arena::treeTierCount(tree); tier++) {
      std::vector<int> row;
      for (int i = 0; i < Arena::suitNodeCount(); i++) {
        const Arena::SuitNode& node = Arena::suitNode(i);
        if (node.tree == tree && node.tier == tier) row.push_back(i);
      }
      float along = firstStep + tierStep * (tier - 1);
      for (size_t k = 0; k < row.size(); k++) {
        float across =
            (static_cast<float>(k) - (row.size() - 1) * 0.5f) * optionGap;
        nodePos[row[k]] = ImVec2(centre.x + dir.x * along + side.x * across,
                                 centre.y + dir.y * along + side.y * across);
      }
    }
  }

  // Links: a spine runs down each branch through the middle of every row,
  // and a stub joins each option to the spine. The spine lights up as far
  // as the tree is filled, in the colour of the deepest infusion; a stub
  // lights up when its node is filled.
  ImU32 dim = IM_COL32(255, 255, 255, 40);
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    InfusionTree tree = static_cast<InfusionTree>(t);
    ImVec2 dir = treeDirection(tree);
    ImVec2 from = centre;
    for (int tier = 1; tier <= Arena::treeTierCount(tree); tier++) {
      float along = firstStep + tierStep * (tier - 1);
      ImVec2 mid(centre.x + dir.x * along, centre.y + dir.y * along);
      Genus filledHere = Genus::None;
      for (int i = 0; i < Arena::suitNodeCount(); i++) {
        const Arena::SuitNode& node = Arena::suitNode(i);
        if (node.tree != tree || node.tier != tier) continue;
        if (suit.nodes[i] != Genus::None) filledHere = suit.nodes[i];
      }
      bool lit = filledHere != Genus::None;
      dl->AddLine(from, mid, lit ? genusColor(filledHere, 0.9f) : dim,
                  lit ? 3.0f : 1.5f);
      for (int i = 0; i < Arena::suitNodeCount(); i++) {
        const Arena::SuitNode& node = Arena::suitNode(i);
        if (node.tree != tree || node.tier != tier) continue;
        bool filled = suit.nodes[i] != Genus::None;
        dl->AddLine(mid, nodePos[i],
                    filled ? genusColor(suit.nodes[i], 0.9f) : dim,
                    filled ? 3.0f : 1.5f);
      }
      from = mid;
    }
  }

  // Core.
  {
    ImGui::SetCursorScreenPos(
        ImVec2(centre.x - coreRadius, centre.y - coreRadius));
    ImGui::InvisibleButton("core",
                           ImVec2(coreRadius * 2.0f, coreRadius * 2.0f));
    bool hovered = ImGui::IsItemHovered();
    bool canSlot =
        havePick && suit.block(Arena::CORE_NODE, picked, pickedGrade) ==
                        Arena::SlotBlock::None;
    if (ImGui::IsItemClicked() && canSlot) trySlot(Arena::CORE_NODE);
    if (suit.core != Genus::None) {
      dl->AddCircleFilled(centre, coreRadius, genusColor(suit.core, 0.85f), 40);
      text(dl, ImVec2(centre.x, centre.y - 14.0f), IM_COL32(255, 255, 255, 255),
           13.0f, "CORE");
      text(dl, ImVec2(centre.x, centre.y + 1.0f), IM_COL32(255, 255, 255, 255),
           11.0f, Arena::coreAbility(suit.core).name);
      gradePips(dl, ImVec2(centre.x, centre.y + 22.0f), suit.coreGrade,
                IM_COL32(255, 255, 255, 235), false);
    } else {
      dl->AddCircleFilled(centre, coreRadius, IM_COL32(24, 26, 38, 255), 40);
      text(dl, ImVec2(centre.x, centre.y - 7.0f), IM_COL32(255, 220, 140, 255),
           14.0f, "CORE");
      if (canSlot) {
        dl->AddCircle(centre, coreRadius + 3.0f + 3.0f * pulse,
                      genusColor(picked, 0.9f), 40, 3.0f);
      }
    }
    dl->AddCircle(centre, coreRadius, IM_COL32(255, 255, 255, 160), 40, 2.0f);
    if (hovered) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(380.0f);
      if (suit.core != Genus::None) {
        const Arena::AbilityInfo& ability = Arena::coreAbility(suit.core);
        ImGui::TextColored(genusColorV(suit.core, 1.0f), "CORE: %s  [E]",
                           ability.name);
        ImGui::TextUnformatted(ability.description);
        ImGui::TextDisabled(
            "%s: %s",
            itemName(suit.core, suit.corePart, suit.coreGrade).c_str(),
            Arena::partInfo(suit.corePart).description);
      } else if (havePick) {
        const Arena::AbilityInfo& ability = Arena::coreAbility(picked);
        ImGui::Text("Click to slot %s as your core:",
                    itemName(picked, pickedPart, pickedGrade).c_str());
        ImGui::TextColored(genusColorV(picked, 1.0f), "%s  [E]", ability.name);
        ImGui::TextUnformatted(ability.description);
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                           "The core is permanent for this match.");
        if (pickedGrade > Arena::GRADE_RAW) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                             "A higher grade gives the core ability nothing "
                             "extra (only its part stat).");
        }
      } else {
        ImGui::TextUnformatted("Your first infusion powers the core.");
      }
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  }

  // Tree nodes.
  for (int i = 0; i < Arena::suitNodeCount(); i++) {
    const Arena::SuitNode& node = Arena::suitNode(i);
    ImVec2 at = nodePos[i];
    NodeState state = suit.state(i);
    Arena::SlotBlock block =
        havePick ? suit.block(i, picked, pickedGrade) : Arena::SlotBlock::None;
    ImGui::PushID(1000 + i);
    ImGui::SetCursorScreenPos(ImVec2(at.x - nodeRadius, at.y - nodeRadius));
    ImGui::InvisibleButton("node",
                           ImVec2(nodeRadius * 2.0f, nodeRadius * 2.0f));
    bool hovered = ImGui::IsItemHovered();
    bool canSlot = havePick && block == Arena::SlotBlock::None;
    if (ImGui::IsItemClicked() && canSlot) trySlot(i);
    ImGui::PopID();

    ImU32 fill = IM_COL32(24, 26, 38, 255);
    ImU32 ring = IM_COL32(255, 255, 255, 60);
    ImU32 label = IM_COL32(150, 150, 165, 200);
    switch (state) {
      case NodeState::Filled:
        fill = genusColor(suit.nodes[i], 0.85f);
        ring = IM_COL32(255, 255, 255, 220);
        label = IM_COL32(255, 255, 255, 255);
        break;
      case NodeState::Open:
        ring = IM_COL32(255, 220, 140, 220);
        label = IM_COL32(255, 230, 180, 255);
        break;
      case NodeState::Closed:
        fill = IM_COL32(14, 15, 22, 255);
        ring = IM_COL32(255, 255, 255, 18);
        label = IM_COL32(110, 110, 120, 120);
        break;
      case NodeState::Locked:
        break;
      case NodeState::Later:
        fill = IM_COL32(18, 18, 28, 255);
        ring = IM_COL32(150, 170, 255, 70);
        label = IM_COL32(140, 150, 190, 170);
        break;
    }
    // An attuned tree's deep slots wear its genus.
    Genus attuned = suit.attuned[static_cast<int>(node.tree)];
    if (attuned != Genus::None && state != NodeState::Filled &&
        state != NodeState::Closed && node.tier >= 3) {
      ring = genusColor(attuned, 0.9f);
    }
    dl->AddCircleFilled(at, nodeRadius, fill, 28);
    dl->AddCircle(at, nodeRadius, ring, 28,
                  state == NodeState::Filled ? 2.5f : 1.5f);
    if (canSlot) {
      dl->AddCircle(at, nodeRadius + 2.0f + 3.0f * pulse,
                    genusColor(picked, 0.9f), 28, 2.5f);
    }
    if (state == NodeState::Filled) {
      // The part, as a letter: T(alon), H(ide), E(ye), S(inew).
      char letter[2] = {Arena::partInfo(suit.parts[i]).name[0], 0};
      text(dl, ImVec2(at.x, at.y - 10.0f), IM_COL32(255, 255, 255, 255), 14.0f,
           letter);
      gradePips(dl, ImVec2(at.x, at.y + 9.0f), suit.grades[i],
                IM_COL32(255, 255, 255, 235), false, 2.2f);
    } else if (state != NodeState::Closed && node.tier >= 3) {
      // What the slot needs: two hollow dots for Refined, three for
      // Exalted.
      gradePips(dl, at, Arena::tierMinGrade(node.tier),
                IM_COL32(255, 255, 255, 150), true, 2.4f);
    }
    text(dl, ImVec2(at.x, at.y + nodeRadius + 2.0f), label, 11.0f, node.name);

    if (hovered) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(380.0f);
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s  (%s, tier %d)",
                         node.name, Arena::treeName(node.tree), node.tier);
      ImGui::TextUnformatted(node.description);
      if (node.tier >= 3) {
        ImGui::TextDisabled("Needs a %s infusion or better.",
                            Arena::gradeName(Arena::tierMinGrade(node.tier)));
      }
      ImGui::Separator();
      if (state == NodeState::Filled) {
        Genus genus = suit.nodes[i];
        ImGui::Text("Holds %s (%s).",
                    itemName(genus, suit.parts[i], suit.grades[i]).c_str(),
                    Arena::partInfo(suit.parts[i]).description);
        genusEffectLines(genus, node.tree, suit.level(genus, node.tree),
                         suit.grades[i]);
      } else if (state == NodeState::Later) {
        ImGui::TextColored(ImVec4(0.65f, 0.72f, 1.0f, 1.0f),
                           "Tier %d opens on night %d.", node.tier,
                           node.tier == 3 ? 2 : 3);
      } else if (canSlot) {
        ImGui::Text("Click to slot %s here. You get:",
                    itemName(picked, pickedPart, pickedGrade).c_str());
        ImGui::BulletText("%s", node.name);
        ImGui::BulletText("%s", Arena::partInfo(pickedPart).description);
        genusEffectLines(picked, node.tree, suit.level(picked, node.tree, 1),
                         std::max(pickedGrade, suit.grade(picked, node.tree)));
        if (node.tier > 1) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                             "Closes the other options in this row.");
        }
        if (pickedGrade >= Arena::GRADE_REFINED && attuned == Genus::None) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                             "Attunes %s to %s: only %s goes in it after.",
                             Arena::treeName(node.tree),
                             Arena::genusName(picked),
                             Arena::genusName(picked));
        }
        if (pickedGrade >= Arena::GRADE_EXALTED &&
            suit.ascended == Genus::None) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                             "Makes you a %s Ascendant.",
                             Arena::genusName(picked));
        }
      } else if (state == NodeState::Open && havePick) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "%s",
                           blockReason(block, suit, i, pickedGrade).c_str());
      } else if (state == NodeState::Open) {
        ImGui::TextDisabled("Open. Pick an infusion from your pack.");
      } else if (state == NodeState::Closed) {
        ImGui::TextDisabled("Closed: another option in this row was taken.");
      } else {
        ImGui::TextDisabled(suit.core == Genus::None
                                ? "Fill the core first."
                                : "Fill a slot in the row before it first.");
      }
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  }
  ImGui::EndChild();

  // --- summary (right)
  // --------------------------------------------------------
  ImGui::SameLine();
  ImGui::BeginChild("summary", ImVec2(0.0f, bodyHeight), true);
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "YOUR SUIT");
  ImGui::Separator();
  if (suit.ascended != Genus::None) {
    ImGui::TextColored(genusColorV(suit.ascended, 1.0f), "%s Ascendant",
                       Arena::genusName(suit.ascended));
  }
  if (suit.core != Genus::None) {
    ImGui::TextColored(genusColorV(suit.core, 1.0f), "Core: %s [E]",
                       Arena::coreAbility(suit.core).name);
  } else {
    ImGui::TextDisabled("Core: empty");
  }
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    if (suit.attuned[t] == Genus::None) continue;
    ImGui::TextColored(genusColorV(suit.attuned[t], 1.0f), "%s attuned to %s",
                       Arena::treeName(static_cast<InfusionTree>(t)),
                       Arena::genusName(suit.attuned[t]));
  }
  ImGui::Spacing();
  ImGui::TextDisabled("Genus effects");
  bool anyEffect = false;
  for (int t = 0; t < Arena::TREE_COUNT; t++) {
    InfusionTree tree = static_cast<InfusionTree>(t);
    for (int g = 1; g <= Arena::GENUS_COUNT; g++) {
      Genus genus = static_cast<Genus>(g);
      int level = suit.level(genus, tree);
      if (level == 0) continue;
      int grade = suit.grade(genus, tree);
      anyEffect = true;
      ImGui::TextColored(genusColorV(genus, 1.0f), "%s %d",
                         Arena::infusionText(genus, tree).name, level);
      for (int up = Arena::GRADE_REFINED; up <= grade; up++) {
        const Arena::InfusionText& info = Arena::infusionText(genus, tree, up);
        ImGui::TextColored(genusColorV(genus, info.built ? 1.0f : 0.6f),
                           "  + %s%s", info.name, info.built ? "" : " (soon)");
      }
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(380.0f);
        genusEffectLines(genus, tree, level, grade);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
      }
    }
  }
  if (!anyEffect) ImGui::TextDisabled("  none yet");
  ImGui::Spacing();
  ImGui::TextDisabled("Talents");
  bool anyTalent = false;
  for (int i = 0; i < Arena::suitNodeCount(); i++) {
    if (suit.nodes[i] == Genus::None) continue;
    anyTalent = true;
    const Arena::SuitNode& node = Arena::suitNode(i);
    ImGui::Text("%s", node.name);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", node.description);
  }
  if (!anyTalent) ImGui::TextDisabled("  none yet");
  ImGui::Spacing();
  ImGui::TextDisabled("From the items themselves");
  bool anyPart = false;
  for (int p = 0; p < Arena::PART_COUNT; p++) {
    InfusionPart part = static_cast<InfusionPart>(p);
    int n = suit.partCount(part);
    if (n == 0) continue;
    anyPart = true;
    ImGui::Text("%d x %s: %s each", n, Arena::partInfo(part).name,
                Arena::partInfo(part).description);
  }
  if (!anyPart) ImGui::TextDisabled("  none yet");
  ImGui::PopTextWrapPos();
  ImGui::EndChild();

  // --- Ascension: confirm before the one-way door
  // -----------------------------
  if (openAscendPopup) {
    ImGui::OpenPopup("Ascend?");
    openAscendPopup = false;
  }
  ImGui::SetNextWindowPos(ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(
          "Ascend?", nullptr,
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
    Genus genus = static_cast<Genus>(pendingAscendGenus);
    ImGui::PushTextWrapPos(420.0f);
    if (genus != Genus::None) {
      ImGui::SetWindowFontScale(1.3f);
      ImGui::TextColored(genusColorV(genus, 1.0f), "Ascend as %s?",
                         Arena::genusName(genus));
      ImGui::SetWindowFontScale(1.0f);
      ImGui::Text(
          "Your first Exalted infusion makes you a %s Ascendant for the rest "
          "of the match. No other genus can ever be Exalted in your suit.",
          Arena::genusName(genus));
    }
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    bool still =
        genus != Genus::None &&
        count(pendingAscendGenus, pendingAscendPart, Arena::GRADE_EXALTED) > 0;
    ImGui::BeginDisabled(!still);
    if (ImGui::Button("ASCEND", ImVec2(140.0f, 30.0f))) {
      sendSlot(pendingAscendGenus, pendingAscendPart, Arena::GRADE_EXALTED,
               pendingAscendNode);
      pendingAscendNode = -1;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Not yet", ImVec2(140.0f, 30.0f)) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      pendingAscendNode = -1;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  } else {
    pendingAscendNode = -1;
  }

  // The moment of Ascension: the suit flashes the genus colour.
  if (timeSeconds < ascendFlashUntil && ascendFlashGenus != Genus::None) {
    float left = (ascendFlashUntil - timeSeconds) / 2.5f;
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 wa = ImGui::GetWindowPos();
    ImVec2 wb(wa.x + width, wa.y + height);
    fg->AddRectFilled(wa, wb, genusColor(ascendFlashGenus, 0.35f * left));
    std::string title =
        std::string(Arena::genusName(ascendFlashGenus)) + " ASCENDANT";
    for (char& c : title) c = static_cast<char>(std::toupper(c));
    text(fg, ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.5f - 30.0f),
         withAlpha(genusColor(ascendFlashGenus, 1.0f),
                   std::min(1.0f, left * 2.0f)),
         54.0f, title.c_str());
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
  static char line[160];
  ImU32 colour = IM_COL32(255, 235, 150, 255);
  const ImU32 todo = IM_COL32(255, 190, 110, 255);
  const ImU32 working = IM_COL32(150, 220, 255, 255);
  for (const auto& [id, objective] : prediction->getObjectives()) {
    float dx = px - objective.x, dy = py - objective.y;
    bool inRange = dx * dx + dy * dy <= objective.radius * objective.radius;
    const ObjectiveState state = objective.state;
    const int done = objective.enemiesKilled, need = objective.enemiesRequired;
    if (state == ObjectiveState::Completed) continue;

    // Each objective says what it wants in its own words (Clarity): a
    // generic "start objective" left players stuck at camps, gates and frogs.
    switch (objective.type) {
      case ObjectiveType::CaptureOutpost:
        if (!inRange) break;
        std::snprintf(line, sizeof line,
                      "Outpost: kill every guard in the camp  (%d/%d down)",
                      done, need);
        prompt = line;
        colour = todo;
        continue;
      case ObjectiveType::SalvageMedpacks:
        if (!inRange) break;
        if (done >= need && state == ObjectiveState::InProgress) {
          prompt = "[F] Open the med pod: healing for the crew";
        } else {
          std::snprintf(line, sizeof line,
                        "Med pod is locked: kill its guards first  (%d/%d down)",
                        done, need);
          prompt = line;
          colour = todo;
        }
        continue;
      case ObjectiveType::LittleJohn:
        if (!inRange) break;
        prompt = "[F] Open the gate now (wakes its guardian in a rage), "
                 "or kill the guardian to open it";
        continue;
      case ObjectiveType::SaveTheFrogs:
        if (state == ObjectiveState::ReadyToDeposit) {
          float sx = px - objective.depositX, sy = py - objective.depositY;
          if (sx * sx + sy * sy <= objective.radius * objective.radius) {
            std::snprintf(line, sizeof line,
                          "[F] Release the frog into the pond  (%d/%d saved)",
                          done, need);
            prompt = line;
          } else if (!prompt) {
            std::snprintf(line, sizeof line,
                          "Carrying a frog: take it to the POND marker and "
                          "press [F]  (%d/%d saved)",
                          done, need);
            prompt = line;
            colour = todo;
          }
        } else if (inRange) {
          // progress is frogs saved, not the catch timer, so one line
          // covers both catching and idle.
          std::snprintf(line, sizeof line,
                        "[F] Catch a frog (stay close), then carry it to "
                        "the pond  (%d/%d saved)",
                        done, need);
          prompt = line;
        }
        continue;
      default:
        break;
    }

    bool scrapyard = objective.type == ObjectiveType::AlienScrapyard;
    bool probe = objective.type == ObjectiveType::RecoverProbe;
    bool turret = objective.type == ObjectiveType::DerelictTurret;
    if (state == ObjectiveState::Inactive && inRange) {
      int carried = me ? me->scrapCarried : 0;
      if (scrapyard) {
        std::snprintf(line, sizeof line,
                      "[F] Salvage a bundle of scrap (+%d)  (stay close "
                      "while you work)",
                      Arena::Tuning::SCRAP_BUNDLE);
        prompt = line;
      } else if (turret && carried < Arena::Tuning::TURRET_REPAIR_SCRAP) {
        std::snprintf(line, sizeof line,
                      "Repairing this turret takes %d scrap: you carry %d. "
                      "Kill aliens for scrap",
                      Arena::Tuning::TURRET_REPAIR_SCRAP, carried);
        prompt = line;
        colour = todo;
      } else if (turret) {
        std::snprintf(line, sizeof line,
                      "[F] Repair the turret for %d scrap  (you carry %d)",
                      Arena::Tuning::TURRET_REPAIR_SCRAP, carried);
        prompt = line;
      } else if (probe) {
        prompt = "[F] Dig out the probe and boot it: its scan maps the "
                 "land around it  (stay close)";
      } else {
        prompt = "[F] Start objective  (stay close while you work)";
      }
    } else if (state == ObjectiveState::InProgress && inRange) {
      prompt = "Working... stay close";
      colour = working;
    } else if (state == ObjectiveState::ReadyToDeposit && probe) {
      // Delivered by walking into the ship's ring.
      if (!prompt) {
        prompt = "Probe recovered: carry it into the ring around the ship";
        colour = todo;
      }
    } else if (state == ObjectiveState::ReadyToDeposit) {
      float sx = px - objective.depositX, sy = py - objective.depositY;
      if (sx * sx + sy * sy <= objective.radius * objective.radius) {
        prompt = "[F] Drop it off";
      } else if (!prompt) {
        prompt = "Carry it to the drop-off and press [F]";
        colour = todo;
      }
    }
  }
  if (!prompt && me && me->scrapCarried > 0) {
    // What you carry, and what it buys once banked.
    const MatchStatePacket& match = arena->match();
    int tier =
        std::min<int>(match.shipTier, Arena::Tuning::SHIP_TIER_COUNT - 1);
    bool heavy = me->scrapCarried >= Arena::Tuning::SCRAP_HEAVY_LOAD;
    if (tier + 1 < Arena::Tuning::SHIP_TIER_COUNT) {
      std::snprintf(line, sizeof line,
                    "Carrying %d scrap%s: bank it in the ring around the "
                    "ship  (ship %d/%d to the next tier)",
                    me->scrapCarried, heavy ? " (heavy)" : "",
                    match.scrapBanked + me->scrapCarried,
                    Arena::Tuning::SHIP_TIER_COST[tier + 1]);
    } else {
      std::snprintf(line, sizeof line,
                    "Carrying %d scrap%s: bank it at the ship to patch the "
                    "hull",
                    me->scrapCarried, heavy ? " (heavy)" : "");
    }
    prompt = line;
    colour =
        heavy ? IM_COL32(255, 190, 110, 255) : IM_COL32(225, 220, 205, 230);
  }
  if (!prompt && me && me->coreGenus == 0) {
    int unslotted = 0;
    for (const auto& genusRow : me->inventory) {
      for (const auto& partRow : genusRow) {
        for (uint8_t n : partRow) unslotted += n;
      }
    }
    float dx = px - match.shipX, dy = py - match.shipY;
    if (unslotted > 0 &&
        dx * dx + dy * dy <= shipSafeRadius() * shipSafeRadius()) {
      prompt = "[B] You have an infusion: open your suit beside the ship";
      colour = IM_COL32(150, 255, 190, 255);
    }
  }
  if (prompt) text(dl, ImVec2(SCREEN_W * 0.5f, y), colour, 16.0f, prompt);
}

bool ArenaView::localAtShip() const {
  if (!arena->hasMatch()) return true;
  const MatchStatePacket& match = arena->match();
  float px, py;
  // localPosition is not const; the predicted player is the same thing.
  const Player& local = prediction->getLocalPlayer();
  px = local.x;
  py = local.y;
  float dx = px - match.shipX, dy = py - match.shipY;
  float r = shipSafeRadius();
  return dx * dx + dy * dy <= r * r;
}

void ArenaView::drawCampPanel() {
  const MatchStatePacket& match = arena->match();
  const MatchPlayerState* me = arena->matchPlayer(localId());
  if (!me) return;
  const Player& local = prediction->getLocalPlayer();

  // Who shares this camp (the server uses the same radius).
  int allies = 0;
  for (const auto& [id, view] : arena->players()) {
    if (id == localId() || view.state.characterId == 0) continue;
    if (view.state.health <= 0.0f) continue;
    float dx = view.x - local.x, dy = view.y - local.y;
    float r = Arena::Tuning::CAMP_SHARE_RADIUS;
    if (dx * dx + dy * dy <= r * r) allies++;
  }

  ImGui::SetNextWindowPos(ImVec2(SCREEN_W * 0.5f, SCREEN_H * 0.30f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.88f);
  ImGui::Begin("Camp", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  ImGui::SetWindowFontScale(1.5f);
  ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.4f, 1.0f), "CAMPED IN THE WILD");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Separator();
  if (allies > 0) {
    ImGui::Text("%d crewmate%s by your fire: a better night's rest.", allies,
                allies == 1 ? "" : "s");
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.5f, 1.0f),
                       "Alone out here. Rest is thin.");
  }
  ImGui::BulletText("Healed %.0f%% of your health tonight (the ship heals fully).",
                    100.0f * std::min(1.0f, Arena::Tuning::CAMP_HEAL_FRACTION +
                                                Arena::Tuning::CAMP_HEAL_PER_ALLY *
                                                    static_cast<float>(allies)));
  ImGui::BulletText("Your suit opens only at the ship.");
  if (me->scrapCarried > 0) {
    ImGui::BulletText("Your %d scrap stays in your hands until you bank it.",
                      me->scrapCarried);
  }
  int round = std::min<int>(match.round + 1, Arena::Tuning::ROUND_COUNT);
  if (match.round < Arena::Tuning::ROUND_COUNT &&
      Arena::Tuning::DAWN_AMBUSH_SIZE[round - 1] > 0) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f),
                       "Something is circling the camp. Expect it at dawn.");
  } else if (match.round >= Arena::Tuning::ROUND_COUNT) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f),
                       "The finale is at the ship: you will be called home.");
  }
  ImGui::Spacing();
  ImGui::Text("Dawn in %s", formatTime(match.timeRemaining).c_str());
  ImGui::SameLine(400.0f);
  bool ready = me->ready != 0;
  if (ImGui::Button(ready ? "READY  [R]" : "READY UP  [R]", ImVec2(140.0f, 28.0f))) {
    PlayerReadyPacket packet;
    packet.ready = ready ? 0 : 1;
    prediction->sendToServer(serialize(packet));
  }
  ImGui::End();
}

bool ArenaView::canUpgradeShip() const {
  if (!arena->hasMatch()) return false;
  const MatchStatePacket& match = arena->match();
  int tier = match.shipTier;
  return localAtShip() &&
         match.phase == static_cast<uint8_t>(Arena::MatchPhase::Intermission) &&
         tier + 1 < Arena::Tuning::SHIP_TIER_COUNT &&
         match.scrapBanked >= Arena::Tuning::SHIP_TIER_COST[tier + 1];
}

float ArenaView::shipSafeRadius() const {
  int tier = arena->hasMatch() ? arena->match().shipTier : 0;
  return Arena::Tuning::SHIP_TIER_SAFE_RADIUS[std::min<int>(
      tier, Arena::Tuning::SHIP_TIER_COUNT - 1)];
}
