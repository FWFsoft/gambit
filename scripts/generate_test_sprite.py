#!/usr/bin/env python3
"""
Generate a test sprite sheet for player animations.
Creates a 256x256 PNG with colored rectangles for each animation frame.

Layout (32x32 frames):
- Row 0: Idle (0,0), Walk Northwest frames (128,0), (160,0), (192,0), (224,0)
- Row 1: Walk North (4 frames)
- Row 2: Walk Northeast (4 frames)
- Row 3: Walk East (4 frames)
- Row 4: Walk Southeast (4 frames)
- Row 5: Walk South (4 frames)
- Row 6: Walk Southwest (4 frames)
- Row 7: Walk West (4 frames)
"""

from PIL import Image, ImageDraw, ImageFont
import os

# Create output directory if it doesn't exist
os.makedirs("assets", exist_ok=True)

# Sprite sheet configuration
SHEET_SIZE = 256
FRAME_SIZE = 32
FRAMES_PER_ROW = SHEET_SIZE // FRAME_SIZE

# Color palette for each direction (RGB)
COLORS = {
    "idle": (100, 100, 255),         # Blue
    "walk_north": (255, 100, 100),   # Red
    "walk_northeast": (255, 150, 100), # Orange
    "walk_east": (255, 255, 100),    # Yellow
    "walk_southeast": (150, 255, 100), # Yellow-green
    "walk_south": (100, 255, 100),   # Green
    "walk_southwest": (100, 255, 200), # Cyan-green
    "walk_west": (100, 200, 255),    # Light blue
    "walk_northwest": (200, 100, 255), # Purple
}

def draw_frame(draw, x, y, color, frame_index):
    """Draw a single 32x32 frame with a simple character sprite."""
    center_x = x + FRAME_SIZE // 2
    center_y = y + FRAME_SIZE // 2

    # Add subtle vertical bobbing for walk animation (odd frames higher)
    bob_offset = -1 if frame_index % 2 == 1 else 0

    # Draw a simple character as a colored oval (body)
    body_radius = 10
    draw.ellipse(
        [center_x - body_radius, center_y - body_radius + bob_offset,
         center_x + body_radius, center_y + body_radius + bob_offset],
        fill=color,
        outline=(0, 0, 0),
        width=1
    )

    # Add a small head (lighter shade)
    head_radius = 5
    head_color = tuple(min(c + 40, 255) for c in color)
    draw.ellipse(
        [center_x - head_radius, center_y - body_radius - head_radius + bob_offset,
         center_x + head_radius, center_y - body_radius + head_radius + bob_offset],
        fill=head_color,
        outline=(0, 0, 0),
        width=1
    )

def main():
    # Create transparent sprite sheet
    sprite_sheet = Image.new('RGBA', (SHEET_SIZE, SHEET_SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(sprite_sheet)

    # Row 0: Idle at (0, 0)
    draw_frame(draw, 0, 0, COLORS["idle"], 0)

    # Row 0: Walk Northwest at (128, 0) - 4 frames
    for i in range(4):
        x = 128 + (i * FRAME_SIZE)
        draw_frame(draw, x, 0, COLORS["walk_northwest"], i)

    # Row 1: Walk North - 4 frames starting at y=32
    for i in range(4):
        x = i * FRAME_SIZE
        draw_frame(draw, x, 32, COLORS["walk_north"], i)

    # Row 2: Walk Northeast - 4 frames starting at y=64
    for i in range(4):
        x = i * FRAME_SIZE
        draw_frame(draw, x, 64, COLORS["walk_northeast"], i)

    # Row 3: Walk East - 4 frames starting at y=96
    for i in range(4):
        x = i * FRAME_SIZE
        draw_frame(draw, x, 96, COLORS["walk_east"], i)

    # Row 4: Walk Southeast - 4 frames starting at y=128
    for i in range(4):
        x = i * FRAME_SIZE
        draw_frame(draw, x, 128, COLORS["walk_southeast"], i)

    # Row 5: Walk South - 4 frames starting at y=160
    for i in range(4):
        x = i * FRAME_SIZE
        draw_frame(draw, x, 160, COLORS["walk_south"], i)

    # Row 6: Walk Southwest - 4 frames starting at y=192
    for i in range(4):
        x = i * FRAME_SIZE
        draw_frame(draw, x, 192, COLORS["walk_southwest"], i)

    # Row 7: Walk West - 4 frames starting at y=224
    for i in range(4):
        x = i * FRAME_SIZE
        draw_frame(draw, x, 224, COLORS["walk_west"], i)

    # Save sprite sheet
    output_path = "assets/player_animated.png"
    sprite_sheet.save(output_path)
    print(f"Generated sprite sheet: {output_path}")
    print(f"Size: {SHEET_SIZE}x{SHEET_SIZE} ({FRAMES_PER_ROW}x{FRAMES_PER_ROW} grid of {FRAME_SIZE}x{FRAME_SIZE} frames)")
    print("\nLayout:")
    print("  Row 0: Idle (1 frame) + Walk Northwest (4 frames)")
    print("  Row 1: Walk North (4 frames) - Red")
    print("  Row 2: Walk Northeast (4 frames) - Orange")
    print("  Row 3: Walk East (4 frames) - Yellow")
    print("  Row 4: Walk Southeast (4 frames) - Yellow-green")
    print("  Row 5: Walk South (4 frames) - Green")
    print("  Row 6: Walk Southwest (4 frames) - Cyan-green")
    print("  Row 7: Walk West (4 frames) - Light blue")
    print("  Row 0 (right): Walk Northwest (4 frames) - Purple")

if __name__ == "__main__":
    main()
