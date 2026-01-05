# WFC Map Generator - Quick Start Guide

## What You Now Have

A complete **Wave Function Collapse (WFC)** map generation system that:
- Procedurally generates isometric maps from your tileset
- Respects adjacency rules (which tiles can connect to which)
- Outputs `.tmx` files compatible with your existing `TiledMap` loader
- Works seamlessly with your Gambit game engine

## Quick Usage

### Generate a Map

```bash
cd tools/map_generator
uv run --with Pillow python generate_map.py \
  --width 30 \
  --height 30 \
  --output ../../assets/my_generated_map.tmx
```

### Options

- `--tiles <file>` - Tile definitions (default: `tiles_starter.json`)
- `--width <n>` - Map width in tiles (default: 20)
- `--height <n>` - Map height in tiles (default: 20)
- `--seed <n>` - Random seed for reproducibility
- `--save-grid` - Also save raw grid JSON for debugging

### Load in Game

```cpp
TiledMap map;
map.load("assets/my_generated_map.tmx");
// Map is now ready to use!
```

## Files Overview

### Core Tools

1. **`generate_map.py`** - Main tool (WFC + TMX export pipeline)
2. **`tile_analyzer.py`** - Analyzes your tileset PNG, creates visualizations
3. **`wfc_generator.py`** - Wave Function Collapse algorithm implementation
4. **`tmx_generator.py`** - Converts WFC output to Tiled `.tmx` format

### Configuration

- **`tiles_starter.json`** - Starter config with 10 flat block tiles
- **`tiles.json`** - Full tile definitions (ready for expansion)
- **`tiles_skeleton.json`** - Auto-generated template for all 90 tiles

### Documentation

- **`EDGE_GUIDE.md`** - How to define edge types and adjacency rules
- **`README.md`** - Full documentation
- **`QUICKSTART.md`** - This file

### Generated Files

- **`tileset_grid.png`** - Your tileset with tile IDs overlaid (for reference)
- **`tileset_comparison.png`** - Side-by-side view of tiles and collision maps
- **`test_generated.tmx`** - Small test map (10x10)

## Current Status

### ✅ What Works Now

- WFC algorithm generates valid maps
- 10 flat block tiles defined and working
- Maps load correctly in your TiledMap system
- Tile weights control frequency (common vs rare tiles)
- Reproducible generation via seeds

### 🚧 What's Next (Optional)

To use your full tileset (90 tiles with walls, stairs, ramps):

1. **Define edge types** for remaining tiles
   - Open `EDGE_GUIDE.md` for instructions
   - Edit `tiles_skeleton.json` or create new definitions
   - Define which tiles can connect (adjacency rules)

2. **Test incrementally**
   - Start with just flat tiles + walls
   - Add stairs and ramps later
   - Test each addition with small maps

3. **Add collision generation** (optional)
   - Currently maps generate without collision objects
   - Can be manually added in Tiled editor
   - Or extend `tmx_generator.py` to auto-generate from collision_map.png

4. **Tune generation parameters**
   - Adjust tile weights for better variety
   - Add constraints (e.g., "always flat tiles at edges")
   - Experiment with different seeds

## Example Workflow

### 1. Analyze Your Tileset

```bash
uv run --with Pillow python tile_analyzer.py \
  ../../assets/maps/all_tiles.png \
  ../../assets/maps/collison_map.png
```

This creates visualizations to help you identify tiles.

### 2. Define Tiles

Edit `tiles.json` to add more tiles with their edge types:

```json
{
  "id": 72,
  "name": "wall_south_facing",
  "row": 8,
  "col": 0,
  "edges": {
    "north": "flat_ground",
    "south": "wall_face",
    "east": "wall_side",
    "west": "wall_side"
  },
  "weight": 3
}
```

### 3. Generate Maps

```bash
uv run --with Pillow python generate_map.py \
  --tiles tiles.json \
  --width 40 \
  --height 40 \
  --seed 42 \
  --output ../../assets/generated_map_001.tmx
```

```bash
uv run python generate_map.py \
 --tiles tiles_venoxarid_test.json \
 --width 15 --height 15 \
 --seed 42 \
 --output ../../assets/test_map.tmx
```

### 4. Test in Game

```bash
cd ../..
./build/Client  # or /dev to run full environment
```

Load the map in your game code:

```cpp
map.load("assets/generated_map_001.tmx");
```

## Tips

### Getting Good Results

1. **Start small**: Test with 10x10 maps first
2. **Few tiles first**: Get 5-10 tiles working before adding more
3. **Simple edges**: Begin with just "flat" and "void" edge types
4. **Check visualizations**: Use the grid overlay to identify tiles correctly
5. **Iterate quickly**: Generate, test, tweak, repeat

### Debugging

If generation fails:
- Map is too constrained (edges don't have enough compatible neighbors)
- Try larger map size
- Add more tile variety
- Simplify edge rules temporarily

If map looks weird:
- Check edge types match visually
- Verify tile IDs in `tileset_grid.png`
- Test with `--save-grid` to see raw tile IDs

## Advanced: Runtime Generation

Want to generate maps at game startup instead of pre-generating?

You could:
1. Port `wfc_generator.py` to C++
2. Call Python from C++ (embed Python interpreter)
3. Pre-generate a pool of maps and randomly select one

For now, pre-generating `.tmx` files is the simplest approach.

## Next Steps

The system is ready to use! You can:

1. **Use it now** with the 10 flat tiles for simple test maps
2. **Expand gradually** by adding walls, stairs, and ramps
3. **Iterate on gameplay** - see what maps work well for 4-player co-op

Happy generating! 🎮
