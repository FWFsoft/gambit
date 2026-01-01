# Wave Function Collapse Map Generator

This tool generates procedural isometric maps using the Wave Function Collapse algorithm.

## Overview

The WFC algorithm generates maps by:
1. Reading tile definitions with adjacency rules from `tiles.json`
2. Iteratively placing tiles that satisfy neighbor constraints
3. Outputting valid `.tmx` files for use with the Gambit engine

## Files

- `tiles.json` - Tile definitions and adjacency rules
- `tile_analyzer.py` - Tool to help extract tile info from PNG files
- `wfc_generator.py` - Main WFC algorithm implementation
- `tmx_generator.py` - Converts WFC output to Tiled .tmx format

## Usage

### 1. Define Tiles

Edit `tiles.json` to define your tiles and their edge compatibility rules.

### 2. Generate a Map

```bash
python wfc_generator.py --width 20 --height 20 --output assets/generated_map.tmx
```

### 3. Load in Game

The generated `.tmx` file can be loaded with the existing `TiledMap` class:

```cpp
TiledMap map;
map.load("assets/generated_map.tmx");
```

## Tile Definition Format

See `tiles.json` for the schema. Each tile needs:
- Unique ID and name
- Edge labels for all 4 sides (N/S/E/W)
- Optional: rotation variants, weight for frequency control
- Reference to collision shape
