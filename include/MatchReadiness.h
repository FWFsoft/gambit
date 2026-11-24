#pragma once

#include "NetworkProtocol.h"

// MatchDirector counts selected characters, not spectators still picking.
struct MatchReadiness {
  int ready = 0;
  int participants = 0;
};

inline MatchReadiness matchReadiness(const MatchStatePacket& match) {
  MatchReadiness count;
  for (const auto& player : match.players) {
    if (player.characterId == 0) continue;
    count.participants++;
    if (player.ready) count.ready++;
  }
  return count;
}
