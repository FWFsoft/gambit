#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

// Represents a single frame in an animation
struct AnimationFrame {
  int srcX;        // Pixel X in sprite sheet
  int srcY;        // Pixel Y in sprite sheet
  int srcW;        // Width in pixels
  int srcH;        // Height in pixels
  float duration;  // Frame duration in milliseconds
};

// Represents a complete animation sequence (e.g., "walk_north", "idle")
struct AnimationClip {
  std::string name;
  std::vector<AnimationFrame> frames;
  bool loop;  // Should animation loop?

  AnimationClip() : name(""), loop(true) {}

  AnimationClip(const std::string& name, bool loop = true)
      : name(name), loop(loop) {}

  // Tiger Style: Assert if frame index invalid
  const AnimationFrame& getFrame(int index) const {
    assert(index >= 0 && index < (int)frames.size() &&
           "Animation frame index out of bounds");
    return frames[index];
  }

  int getFrameCount() const { return (int)frames.size(); }

  // Total duration of animation in milliseconds
  float getTotalDuration() const {
    float total = 0.0f;
    for (const auto& frame : frames) {
      total += frame.duration;
    }
    return total;
  }
};
