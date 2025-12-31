#include "CharacterSelectionState.h"

CharacterSelectionState& CharacterSelectionState::instance() {
  static CharacterSelectionState instance;
  return instance;
}
