#pragma once

#include <deque>
#include <string>

#include "EventBus.h"

// Forward declarations
class Camera;
class SpriteRenderer;

// Events published when damage occurs
struct DamageDealtEvent {
  float x, y;          // World position where damage occurred
  float damageAmount;  // Amount of damage
  bool isCritical;     // If true, show in yellow
};

struct DamageReceivedEvent {
  float x, y;          // World position where damage occurred
  float damageAmount;  // Amount of damage
};

struct HealingEvent {
  float x, y;        // World position where healing occurred
  float healAmount;  // Amount healed
};

// System for rendering floating damage numbers
class DamageNumberSystem {
 public:
  DamageNumberSystem(Camera* camera, SpriteRenderer* spriteRenderer);

  void render();

 private:
  struct DamageNumber {
    float worldX, worldY;  // World position
    float damageAmount;
    float creationTime;  // currentTime when created
    enum class Type { Damage, Critical, PlayerDamage, Healing } type;
  };

  Camera* camera;
  SpriteRenderer* spriteRenderer;
  std::deque<DamageNumber> activeNumbers;
  float currentTime;

  static constexpr float DISPLAY_DURATION = 1.0f;  // Seconds to show
  static constexpr float RISE_SPEED = 30.0f;       // Pixels per second upward
  static constexpr float FADE_START = 0.5f;  // When to start fading (seconds)

  void onDamageDealt(const DamageDealtEvent& e);
  void onDamageReceived(const DamageReceivedEvent& e);
  void onHealing(const HealingEvent& e);
  void onUpdate(const UpdateEvent& e);

  void renderDamageNumber(const DamageNumber& num);
};
