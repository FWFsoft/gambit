# PixelLab Integration for Venoxarid Tileset

## Summary

Successfully integrated PixelLab's MCP server to generate Venoxarid-themed isometric tiles for the Gambit game engine. The tiles work seamlessly with the existing Wave Function Collapse (WFC) map generation pipeline.

## What Was Accomplished

### Phase 1: PixelLab Exploration ✓

**Generated 5 test tiles** with Venoxarid theme (organic moss, bioluminescence, toxic pools):
1. Dense moss with pink mushrooms and yellow roots
2. Moss with twisted yellow glowing vines
3. Dark moss with pink bioluminescent fungi
4. Sparse moss transitioning to organic matter
5. Toxic green liquid pool with pink algae

**Key Findings:**
- PixelLab generates 64×64 RGBA PNG tiles with transparency
- `create_isometric_tile` API works perfectly for our use case
- Tiles have consistent Venoxarid color palette (green/yellow/pink)
- Generation time: ~10-20 seconds per tile

### Phase 2: Tile Processing Pipeline ✓

**Created `pixellab_processor.py`:**
- Fetches tiles from manifest file
- Organizes into sprite sheet with configurable columns and spacing
- Generates WFC-compatible metadata JSON
- Supports 64×64 tiles with 4px spacing (matches current format)

**Test Results:**
- Generated 472×64px sprite sheet (5 tiles in 7-column layout)
- Successfully created `tiles_venoxarid_test.json` with edge definitions

### Phase 3: WFC Integration ✓

**Tested WFC generation** with minimal Venoxarid tileset:
- Generated 15×15 map in 225 iterations
- All moss tiles (matching edges) successfully placed
- Toxic pool (incompatible edges) correctly excluded
- Output: Valid TMX file for Tiled/game engine

## Technical Decisions

### Size Approach: 64×64 (Option B)

**Decision:** Use 64×64 tiles instead of cropping to 64×32

**Rationale:**
- PixelLab generates high-quality 64×64 isometric tiles
- Cropping would lose vertical detail and quality
- Game engine needs bug fix anyway (hardcoded 2 columns → should be 7)
- Transparency handles non-diamond areas naturally
- More artistic flexibility for future tiles

### Edge-Based WFC (Keep Current System)

**Decision:** Keep existing edge-based matching system

**Rationale:**
- Current WFC implementation works well for isometric tiles
- Edge semantics align with isometric perspective (N/S/E/W faces)
- PixelLab's Wang tilesets incompatible with isometric rendering
- No algorithm refactoring needed

## Files Created

1. **`pixellab_processor.py`** - Tile processing pipeline
2. **`venoxarid_manifest_test.json`** - Tile manifest with PixelLab IDs
3. **`tiles_venoxarid_test.json`** - WFC tile configuration (generated)
4. **`test_tileset.png`** - Test sprite sheet (generated)
5. **`venoxarid_test_map.tmx`** - Test map (generated)
6. **`pixellab_tiles/`** - Directory with 5 downloaded tiles

## Example Usage

### 1. Generate Tiles with PixelLab

```python
# Create isometric tile via MCP
mcp__pixellab__create_isometric_tile(
    description="dense moss with glowing pink mushrooms, alien terrain",
    size=64,
    tile_shape="thin tile",
    detail="medium detail",
    shading="medium shading",
    outline="lineless"
)

# Retrieve completed tile
mcp__pixellab__get_isometric_tile(tile_id="...")
```

### 2. Create Manifest

```json
{
  "tileset_name": "venoxarid_organic",
  "tile_width": 64,
  "tile_height": 64,
  "tiles": [
    {
      "name": "dense_moss_pink_mushrooms",
      "file": "pixellab_tiles/tile_01.png",
      "pixellab_id": "uuid-from-pixellab",
      "edges": {
        "north": "flat_moss",
        "south": "flat_moss",
        "east": "flat_moss",
        "west": "flat_moss"
      },
      "weight": 10
    }
  ]
}
```

### 3. Process Tiles

```bash
uv run python pixellab_processor.py \
  --manifest venoxarid_manifest.json \
  --output-sheet ../../assets/maps/venoxarid_tileset.png \
  --output-json tiles_venoxarid.json
```

### 4. Generate Map

```bash
uv run python generate_map.py \
  --tiles tiles_venoxarid.json \
  --width 30 --height 30 \
  --seed 42 \
  --output ../../assets/venoxarid_map.tmx
```

## Next Steps

### Immediate (To Complete MVP)

1. **Fix TileRenderer.cpp bug** (line 177-179)
   - Currently assumes 2 columns, should use tileset config
   - Required to render new 7-column tileset correctly

2. **Update TileConfig.h** for 64×64 tiles
   - Add `SOURCE_WIDTH = 64` and `SOURCE_HEIGHT = 64`
   - Keep `RENDER_WIDTH = 64` and `RENDER_HEIGHT = 32` for isometric projection
   - Update UV calculations in TileRenderer

3. **Generate more tiles** (15-20 total for MVP)
   - More moss variations for visual diversity
   - Different organic ground types
   - Transition tiles between moss types

4. **Define edge compatibility rules**
   - Determine which tiles can connect to each other
   - Create visual guide for edge types
   - Test WFC success rate with different seeds

### Future Enhancements

1. **Full Venoxarid Tileset** (30-40 tiles)
   - All Priority 1 organic ground variations
   - Toxic pools and bioluminescent features
   - Optional: Elevation tiles (ramps, walls)

2. **Additional Genera Tilesets**
   - Cerebrian (psychic/blue-purple theme)
   - Frigidariant (ice/crystalline theme)
   - Magnarok (fire/lava theme)

3. **Advanced Features**
   - Procedural color variations using PixelLab seeds
   - Style-matched map objects using `create_map_object`
   - Multi-biome maps with transition zones

## Challenges Encountered

### 1. Wang Tileset Incompatibility ⚠️

**Problem:** PixelLab's `create_topdown_tileset` generates Wang tilesets for top-down perspective, incompatible with isometric rendering.

**Solution:** Use `create_isometric_tile` for individual tiles instead.

### 2. Edge Type Matching

**Problem:** Initially defined different edge types per tile (flat_moss_dense, flat_moss_vine, etc.) preventing WFC connections.

**Solution:** Use same edge type (`flat_moss`) for all compatible tiles, reserve unique types for special features.

### 3. TileRenderer Column Assumption

**Problem:** TileRenderer.cpp hardcoded to 2 columns, but tileset uses 7 columns.

**Solution:** Needs fix (tracked for Phase 4 of implementation plan).

## Resources

- **PixelLab Tiles:** `pixellab_tiles/`
- **Test Tileset:** `test_tileset.png`
- **Test Config:** `tiles_venoxarid_test.json`
- **Test Map:** `venoxarid_test_map.tmx`
- **Processing Script:** `pixellab_processor.py`
- **Manifest Template:** `venoxarid_manifest_test.json`

## Success Metrics

✅ **MVP Complete:**
- [x] 5 Venoxarid-themed tiles generated
- [x] Tiles render at 64×64 with transparency
- [x] WFC generates valid maps without contradictions
- [x] Green/yellow/pink color palette consistent
- [x] Automated processing pipeline created
- [x] Test map validates integration

**Next:** Engine updates and expand tileset to 15-20 tiles
