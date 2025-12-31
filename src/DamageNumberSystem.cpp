#include "DamageNumberSystem.h"

#include <imgui.h>

#include <cmath>
#include <string>

#include "Camera.h"
#include "Logger.h"
#include "SpriteRenderer.h"

DamageNumberSystem::DamageNumberSystem(Camera* camera,
                                       SpriteRenderer* spriteRenderer)
    : camera(camera), spriteRenderer(spriteRenderer), currentTime(0.0f) {
  EventBus::instance().subscribe<DamageDealtEvent>(
      [this](const DamageDealtEvent& e) { onDamageDealt(e); });
  EventBus::instance().subscribe<DamageReceivedEvent>(
      [this](const DamageReceivedEvent& e) { onDamageReceived(e); });
  EventBus::instance().subscribe<HealingEvent>(
      [this](const HealingEvent& e) { onHealing(e); });
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });

  Logger::info("DamageNumberSystem initialized");
}

void DamageNumberSystem::onDamageDealt(const DamageDealtEvent& e) {
  DamageNumber num;
  num.worldX = e.x;
  num.worldY = e.y;
  num.damageAmount = e.damageAmount;
  num.creationTime = currentTime;
  num.type =
      e.isCritical ? DamageNumber::Type::Critical : DamageNumber::Type::Damage;

  activeNumbers.push_back(num);
}

void DamageNumberSystem::onDamageReceived(const DamageReceivedEvent& e) {
  DamageNumber num;
  num.worldX = e.x;
  num.worldY = e.y;
  num.damageAmount = e.damageAmount;
  num.creationTime = currentTime;
  num.type = DamageNumber::Type::PlayerDamage;

  activeNumbers.push_back(num);
}

void DamageNumberSystem::onHealing(const HealingEvent& e) {
  DamageNumber num;
  num.worldX = e.x;
  num.worldY = e.y;
  num.damageAmount = e.healAmount;
  num.creationTime = currentTime;
  num.type = DamageNumber::Type::Healing;

  activeNumbers.push_back(num);
}

void DamageNumberSystem::onUpdate(const UpdateEvent& e) {
  currentTime += e.deltaTime / 1000.0f;  // Convert ms to seconds

  // Remove old damage numbers
  while (!activeNumbers.empty()) {
    float age = currentTime - activeNumbers.front().creationTime;
    if (age >= DISPLAY_DURATION) {
      activeNumbers.pop_front();
    } else {
      break;  // Rest are newer
    }
  }
}

void DamageNumberSystem::render() {
  for (const auto& num : activeNumbers) {
    renderDamageNumber(num);
  }
}

void DamageNumberSystem::renderDamageNumber(const DamageNumber& num) {
  float age = currentTime - num.creationTime;
  if (age >= DISPLAY_DURATION) return;

  // Calculate rise animation (move upward over time)
  float yOffset = age * RISE_SPEED;

  // Convert world position to screen position
  int screenX, screenY;
  camera->worldToScreen(num.worldX, num.worldY - yOffset, screenX, screenY);

  // Calculate fade (fade out in the last part of animation)
  float alpha = 1.0f;
  if (age > FADE_START) {
    alpha = 1.0f - ((age - FADE_START) / (DISPLAY_DURATION - FADE_START));
  }

  // Color based on type
  ImVec4 color;
  switch (num.type) {
    case DamageNumber::Type::Damage:
      color = ImVec4(1.0f, 1.0f, 1.0f, alpha);  // White
      break;
    case DamageNumber::Type::Critical:
      color = ImVec4(1.0f, 1.0f, 0.0f, alpha);  // Yellow
      break;
    case DamageNumber::Type::PlayerDamage:
      color = ImVec4(1.0f, 0.3f, 0.3f, alpha);  // Red
      break;
    case DamageNumber::Type::Healing:
      color = ImVec4(0.3f, 1.0f, 0.3f, alpha);  // Green
      break;
  }

  // Render using ImGui (drawn in screen space, doesn't move with camera)
  ImGui::SetNextWindowPos(ImVec2(screenX, screenY), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));  // Center on position
  ImGui::SetNextWindowBgAlpha(0.0f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin(
      ("##DamageNum" + std::to_string((uint64_t)&num)).c_str(), nullptr,
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration |
          ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);

  ImGui::PushStyleColor(ImGuiCol_Text, color);

  // Format damage number
  std::string text = std::to_string((int)num.damageAmount);
  if (num.type == DamageNumber::Type::Critical) {
    text += "!";  // Add exclamation for crits
  }

  // Larger text for better visibility
  ImGui::SetWindowFontScale(1.5f);
  ImGui::Text("%s", text.c_str());
  ImGui::SetWindowFontScale(1.0f);

  ImGui::PopStyleColor();
  ImGui::End();
  ImGui::PopStyleVar(2);
}
