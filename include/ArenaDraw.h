#pragma once

// Small drawing helpers shared by ArenaView's world and HUD code.
// Header-only; include after <imgui.h>.

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ArenaData.h"
#include "NetworkProtocol.h"

namespace ArenaDraw {

constexpr float PI = 3.14159265f;

struct Rgb {
  float r, g, b;
};

// Core colour of a VFX theme.
inline Rgb themeRgb(VfxTheme theme) {
  switch (theme) {
    case VfxTheme::Brew:
      return {1.00f, 0.74f, 0.22f};
    case VfxTheme::Steel:
      return {0.82f, 0.90f, 1.00f};
    case VfxTheme::Holy:
      return {1.00f, 0.92f, 0.52f};
    case VfxTheme::Wind:
      return {0.50f, 0.95f, 1.00f};
    case VfxTheme::Poison:
      return {0.50f, 0.95f, 0.30f};
    case VfxTheme::Psychic:
      return {0.95f, 0.40f, 0.95f};
    case VfxTheme::Ice:
      return {0.62f, 0.86f, 1.00f};
    case VfxTheme::Fire:
      return {1.00f, 0.52f, 0.14f};
    case VfxTheme::Blood:
      return {0.95f, 0.22f, 0.26f};
    case VfxTheme::Shadow:
      return {0.68f, 0.38f, 1.00f};
    case VfxTheme::Heal:
      return {0.50f, 1.00f, 0.70f};
    case VfxTheme::Danger:
      return {1.00f, 0.20f, 0.16f};
    default:
      return {1.00f, 1.00f, 1.00f};
  }
}

inline ImU32 rgba(Rgb c, float alpha) {
  alpha = std::max(0.0f, std::min(1.0f, alpha));
  return IM_COL32(static_cast<int>(c.r * 255), static_cast<int>(c.g * 255),
                  static_cast<int>(c.b * 255), static_cast<int>(alpha * 255));
}

inline ImU32 themeColor(VfxTheme theme, float alpha) {
  return rgba(themeRgb(theme), alpha);
}

// Lighter version of a theme colour, for hot cores and highlights.
inline ImU32 themeHot(VfxTheme theme, float alpha) {
  Rgb c = themeRgb(theme);
  c.r = c.r + (1.0f - c.r) * 0.65f;
  c.g = c.g + (1.0f - c.g) * 0.65f;
  c.b = c.b + (1.0f - c.b) * 0.65f;
  return rgba(c, alpha);
}

inline VfxTheme genusTheme(Arena::Genus genus) {
  switch (genus) {
    case Arena::Genus::Venoxarid:
      return VfxTheme::Poison;
    case Arena::Genus::Cerebrian:
      return VfxTheme::Psychic;
    case Arena::Genus::Frigidariant:
      return VfxTheme::Ice;
    case Arena::Genus::Magnarok:
      return VfxTheme::Fire;
    default:
      return VfxTheme::Neutral;
  }
}

inline ImU32 withAlpha(ImU32 color, float alpha) {
  alpha = std::max(0.0f, std::min(1.0f, alpha));
  ImU32 a = static_cast<ImU32>(((color >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
  return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

// Text with a dark outline so it stays readable over any ground.
inline void text(ImDrawList* dl, ImVec2 pos, ImU32 color, float size,
                 const char* str, bool centered = true) {
  ImFont* font = ImGui::GetFont();
  ImVec2 extent = font->CalcTextSizeA(size, 10000.0f, 0.0f, str);
  if (centered) pos.x -= extent.x * 0.5f;
  pos.x = std::floor(pos.x);
  pos.y = std::floor(pos.y);
  float alpha = ((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
  ImU32 shadow = IM_COL32(0, 0, 0, static_cast<int>(210 * alpha));
  dl->AddText(font, size, ImVec2(pos.x + 1, pos.y + 1), shadow, str);
  dl->AddText(font, size, ImVec2(pos.x - 1, pos.y), shadow, str);
  dl->AddText(font, size, ImVec2(pos.x, pos.y - 1), shadow, str);
  dl->AddText(font, size, pos, color, str);
}

// Filled circular sector (pie slice). Angles in radians, screen space.
inline void sector(ImDrawList* dl, ImVec2 centre, float radius, float angle,
                   float halfArc, ImU32 fill, ImU32 outline,
                   float outlineThickness = 2.0f) {
  if (radius <= 1.0f) return;
  int segments = std::max(6, static_cast<int>(halfArc * 2.0f * radius / 14.0f));
  if ((fill & IM_COL32_A_MASK) != 0) {
    dl->PathLineTo(centre);
    dl->PathArcTo(centre, radius, angle - halfArc, angle + halfArc, segments);
    dl->PathFillConvex(fill);
  }
  if ((outline & IM_COL32_A_MASK) != 0) {
    dl->PathLineTo(centre);
    dl->PathArcTo(centre, radius, angle - halfArc, angle + halfArc, segments);
    dl->PathStroke(outline, ImDrawFlags_Closed, outlineThickness);
  }
}

// Horizontal bar with a dark backing.
inline void bar(ImDrawList* dl, ImVec2 topLeft, float width, float height,
                float fraction, ImU32 fill, ImU32 back = IM_COL32(0, 0, 0, 190)) {
  fraction = std::max(0.0f, std::min(1.0f, fraction));
  dl->AddRectFilled(ImVec2(topLeft.x - 1, topLeft.y - 1),
                    ImVec2(topLeft.x + width + 1, topLeft.y + height + 1), back);
  if (fraction > 0.0f) {
    dl->AddRectFilled(topLeft,
                      ImVec2(topLeft.x + width * fraction, topLeft.y + height),
                      fill);
  }
}

inline float ease(float t) {
  t = std::max(0.0f, std::min(1.0f, t));
  return 1.0f - (1.0f - t) * (1.0f - t);
}

}  // namespace ArenaDraw
