#include "EffectTracker.h"

#include "Logger.h"
#include "NetworkProtocol.h"

const std::vector<EffectInstance> EffectTracker::emptyEffects;

EffectTracker::EffectTracker() {
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) {
        onNetworkPacketReceived(e);
      });

  Logger::info("EffectTracker initialized");
}

const std::vector<EffectInstance>& EffectTracker::getEffects(
    uint32_t entityId, bool isEnemy) const {
  const auto& effectMap = isEnemy ? enemyEffects : playerEffects;
  auto it = effectMap.find(entityId);
  return it != effectMap.end() ? it->second : emptyEffects;
}

bool EffectTracker::hasEffects(uint32_t entityId, bool isEnemy) const {
  const auto& effectMap = isEnemy ? enemyEffects : playerEffects;
  auto it = effectMap.find(entityId);
  return it != effectMap.end() && !it->second.empty();
}

void EffectTracker::onNetworkPacketReceived(
    const NetworkPacketReceivedEvent& e) {
  if (e.size == 0) return;

  PacketType type = static_cast<PacketType>(e.data[0]);

  if (type == PacketType::EffectUpdate) {
    EffectUpdatePacket packet = deserializeEffectUpdate(e.data, e.size);

    auto& effectMap = packet.isEnemy ? enemyEffects : playerEffects;
    auto& effects = effectMap[packet.targetId];
    effects.clear();

    // Convert network effects to EffectInstances
    for (const auto& netEffect : packet.effects) {
      EffectInstance instance;
      instance.type = static_cast<EffectType>(netEffect.effectType);
      instance.stacks = netEffect.stacks;
      instance.remainingDuration = netEffect.remainingDuration;
      instance.sourceId = 0;  // Not sent over network
      instance.lastTickTime = 0.0f;
      effects.push_back(instance);
    }

    // If no effects, remove from map
    if (effects.empty()) {
      effectMap.erase(packet.targetId);
    }
  }
}
