#include <cassert>
#include <iostream>

#include "MatchReadiness.h"

int main() {
  static_assert(Arena::Tuning::INTERMISSION_SECONDS == 90.0f);
  MatchStatePacket match;
  assert(matchReadiness(match).participants == 0);
  for (int i = 0; i < 4; i++) {
    MatchPlayerState player;
    player.playerId = i + 1;
    player.characterId = 1;
    player.ready = i < 2;
    match.players.push_back(player);
  }
  MatchPlayerState spectator;
  spectator.ready = 1;  // still picking; never part of the ready quorum
  match.players.push_back(spectator);
  auto count = matchReadiness(match);
  assert(count.ready == 2 && count.participants == 4);
  match.players[2].ready = match.players[3].ready = 1;
  count = matchReadiness(match);
  assert(count.ready == 4 && count.participants == 4);
  match.players[0].ready = 0;
  count = matchReadiness(match);
  assert(count.ready == 3 && count.participants == 4);
  match.players.erase(match.players.begin());
  count = matchReadiness(match);
  assert(count.ready == 3 && count.participants == 3);
  std::cout << "MatchReadiness: all tests passed\n";
}
