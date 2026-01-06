#include "MapSelectionState.h"

MapSelectionState& MapSelectionState::instance() {
  static MapSelectionState instance;
  return instance;
}
