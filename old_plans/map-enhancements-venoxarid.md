# Venoxarid Tileset Expansion & Enemy Spawn System

## Overview
Expand the Venoxarid tileset from 5 test tiles to 20-30 production tiles using PixelLab, and create a larger test map (30×30) with enemy spawn points for gameplay testing.

## Current State Analysis

### Existing Tiles (tiles_venoxarid_test.json)
Currently have **5 tiles only**:
1. Dense moss with pink mushrooms (weight: 10)
2. Moss with yellow vines (weight: 8)
3. Dark moss with pink fungi (weight: 10)
4. Sparse moss with spores (weight: 6)
5. Toxic pool (weight: 2, isolated edge type)

**Problem:** Only 5 tiles creates repetitive, boring maps. Need 20-30 for visual variety.

### Existing Edge Types
- `flat_moss` - Used by all 4 ground tiles (too homogeneous)
- `toxic_pool` - Only toxic pool tile uses this (creates isolated pools)
- `void` - Defined but unused

**Problem:** With only one main edge type (`flat_moss`), all tiles can connect to each other uniformly, reducing interesting terrain features.

### Map Generation Tested
- Successfully generated 15×15 test map
- Uses `generate_map.py` with WFC algorithm
- Working TMX export and game rendering
- **No enemy spawns** - not implemented in pipeline

## Goals

### Primary Goals
1. **Expand tileset to 20-30 tiles** with PixelLab
2. **Define 5-7 edge types** for interesting terrain variation
3. **Generate 30×30 test map** for larger gameplay area
4. **Add enemy spawn system** to place enemies in maps

### Success Metrics
- [ ] 20-30 Venoxarid tiles generated and integrated
- [ ] Visually diverse maps with distinct terrain zones
- [ ] 30×30 map generates without WFC contradictions
- [ ] Enemy spawns placed strategically on map
- [ ] Test map loads and plays smoothly in game

## Implementation Plan

### Phase 1: Design Expanded Tileset (Planning)

**1.1 Define Edge Type Strategy**

Expand from 2 edge types → **6-7 edge types** for terrain variety:

| Edge Type | Description | Tile Examples | Design Goal |
|-----------|-------------|---------------|-------------|
| `moss_light` | Sparse, light green moss | Sparse moss, moss patches | Common, walkable ground |
| `moss_dense` | Dense, dark green moss | Dense moss, thick vegetation | Medium density ground |
| `moss_biolum` | Glowing pink/yellow features | Mushroom clusters, glowing vines | Visual interest, rare |
| `moss_toxic` | Yellow-green transition | Pool edges, contaminated ground | Transition to toxic |
| `toxic_pool` | Liquid surface | Pool center tiles | Isolated, impassable |
| `moss_root` | Organic root systems | Root networks, vine tangles | Medium density ground |
| `void` | Map boundary | Empty/black | For map edges only |

**Edge Compatibility Matrix:**
```
           light  dense  biolum  toxic   pool   root
light       ✓      ✓      ✓       ✓      ✗      ✓
dense       ✓      ✓      ✓       ✓      ✗      ✓
biolum      ✓      ✓      ✓       ✓      ✗      ✓
moss_toxic  ✓      ✓      ✓       ✓      ✓      ✓
toxic_pool  ✗      ✗      ✗       ✓      ✓      ✗
moss_root   ✓      ✓      ✓       ✓      ✗      ✓
```

**Key Constraint:** Toxic pools only connect to themselves and pool edge transitions.

**1.2 Plan Tile Distribution (Target: 25-30 tiles)**

Organize into **themed groups**:

| Group | Count | Edge Type Mix | PixelLab Descriptions |
|-------|-------|---------------|----------------------|
| **Light Moss** | 5 | All `moss_light` | "sparse moss", "light green moss patches", "thin moss covering" |
| **Dense Moss** | 5 | All `moss_dense` | "dense dark moss", "thick moss carpet", "heavy moss coverage" |
| **Bioluminescent** | 6 | Mixed `moss_biolum` + others | "glowing pink mushrooms", "yellow bioluminescent vines", "pink spores" |
| **Root Systems** | 4 | Mixed `moss_root` + others | "twisted yellow roots", "organic root networks", "tangled vines" |
| **Toxic Transitions** | 3 | Mixed `moss_toxic` edges | "moss to pool edge", "contaminated moss", "toxic seepage" |
| **Toxic Pools** | 2-3 | All `toxic_pool` | "bubbling green liquid", "toxic pool surface", "acidic sludge" |

**Total: 25-28 tiles** (7 columns × 4 rows in sprite sheet)

**1.3 Weight Distribution Strategy**

Balance common vs rare tiles:

```python
# Light moss - Common (50% of map)
moss_light tiles: weight 10-12

# Dense moss - Common (25% of map)
moss_dense tiles: weight 8-10

# Bioluminescent - Uncommon (15% of map)
moss_biolum tiles: weight 4-6

# Root systems - Uncommon (8% of map)
moss_root tiles: weight 3-5

# Toxic transitions - Rare (1.5% of map)
moss_toxic tiles: weight 1-2

# Toxic pools - Very rare (0.5% of map)
toxic_pool tiles: weight 1
```

This ensures visual variety while maintaining coherent terrain.

### Phase 2: Generate PixelLab Tiles (Execution)

**2.1 Batch Generation Script**

Create `tools/map_generator/generate_venoxarid_batch.py`:

```python
# Workflow:
# 1. Define 25 tile descriptions (organized by theme)
# 2. Call mcp__pixellab__create_isometric_tile for each
# 3. Poll mcp__pixellab__get_isometric_tile until complete
# 4. Download tiles to tools/map_generator/pixellab_tiles/
# 5. Generate venoxarid_manifest_full.json

# Parameters for consistency:
{
  "size": 64,
  "view": "high top-down",
  "tile_shape": "thin tile",
  "detail": "medium detail",
  "shading": "medium shading",
  "outline": "lineless"
}
```

**2.2 Tile Descriptions (Examples)**

Light Moss Group (5 tiles):
```
1. "sparse light green moss on dark organic ground"
2. "light moss patches with small yellow roots"
3. "thin moss covering with scattered pink spores"
4. "light green moss with small bioluminescent spots"
5. "sparse moss transitioning to darker moss"
```

Dense Moss Group (5 tiles):
```
6. "dense dark green moss carpet"
7. "thick moss coverage with small mushrooms"
8. "heavy dark moss with pink bioluminescent fungi"
9. "dense moss with twisted yellow root systems"
10. "dark green moss with scattered pink flowers"
```

Bioluminescent Group (6 tiles):
```
11. "moss with large glowing pink mushroom clusters"
12. "dark moss with bright yellow glowing vines"
13. "moss ground with pink bioluminescent spore clouds"
14. "thick moss with glowing yellow root networks"
15. "moss with pink and yellow glowing fungi mix"
16. "dark ground with dense pink bioluminescent mushrooms"
```

Root Systems Group (4 tiles):
```
17. "moss with twisted yellow organic root systems"
18. "tangled glowing yellow vine networks on moss"
19. "moss with thick organic yellow roots and pink spores"
20. "yellow root clusters breaking through moss"
```

Toxic Transition Group (3 tiles):
```
21. "moss edge transitioning to toxic green liquid"
22. "contaminated moss with yellow-green toxic seepage"
23. "moss ground with toxic green pool forming"
```

Toxic Pool Group (2-3 tiles):
```
24. "bubbling toxic green liquid pool surface"
25. "toxic green acidic sludge with yellow foam"
(26. "toxic pool with pink bioluminescent algae" - optional)
```

**2.3 Processing Pipeline**

Run existing `pixellab_processor.py` with new manifest:

```bash
cd tools/map_generator
uv run python pixellab_processor.py \
  --manifest venoxarid_manifest_full.json \
  --output-sheet ../../assets/maps/venoxarid_tileset_full.png \
  --output-json tiles_venoxarid_full.json \
  --columns 7 \
  --spacing 4 \
  --tile-size 64
```

Output:
- `venoxarid_tileset_full.png` - 7 columns × 4 rows sprite sheet
- `tiles_venoxarid_full.json` - WFC configuration with 25+ tiles

### Phase 3: Define Edge Types & Weights (Execution)

**3.1 Manual Edge Assignment**

Edit `tiles_venoxarid_full.json` to assign edge types based on Phase 1 plan:

```json
{
  "tileset": {
    "name": "venoxarid_organic_full",
    "image": "../../assets/maps/venoxarid_tileset_full.png",
    "tile_width": 64,
    "tile_height": 64,
    "spacing": 4,
    "columns": 7,
    "firstgid": 1
  },
  "edge_types": [
    "moss_light",
    "moss_dense",
    "moss_biolum",
    "moss_root",
    "moss_toxic",
    "toxic_pool",
    "void"
  ],
  "tiles": [
    {
      "id": 0,
      "name": "sparse_light_moss",
      "row": 0,
      "col": 0,
      "edges": {
        "north": "moss_light",
        "south": "moss_light",
        "east": "moss_light",
        "west": "moss_light"
      },
      "weight": 12,
      "pixellab_tile_id": "..."
    },
    // ... repeat for all 25 tiles
  ]
}
```

**3.2 Validation Testing**

Test WFC generation with small maps first:

```bash
# Test 10×10 (quick validation)
uv run python generate_map.py \
  --tiles tiles_venoxarid_full.json \
  --width 10 --height 10 \
  --seed 42 \
  --output ../../assets/test_10x10.tmx

# Test 20×20 (medium validation)
uv run python generate_map.py \
  --tiles tiles_venoxarid_full.json \
  --width 20 --height 20 \
  --seed 123 \
  --output ../../assets/test_20x20.tmx
```

**Expected Issues:**
- WFC contradictions if edge types incompatible
- Toxic pools might not spawn if weights too low
- Need to adjust weights based on visual results

### Phase 4: Generate 30×30 Test Map (Execution)

**4.1 Create Large Test Map**

```bash
cd tools/map_generator
uv run python generate_map.py \
  --tiles tiles_venoxarid_full.json \
  --width 30 --height 30 \
  --seed 999 \
  --output ../../assets/venoxarid_30x30.tmx \
  --save-grid
```

**Expected Challenges:**
- Larger maps = more WFC iterations (potentially 30×30×100 = 90,000 max)
- Higher chance of contradictions
- May need to tweak edge compatibility if generation fails

**Fallback:** If 30×30 fails, try:
- Different seeds (42, 123, 456, 789, 999)
- Simplify edge types (merge `moss_light` and `moss_dense`)
- Increase toxic pool weight slightly to give WFC more options

**4.2 Load in Game & Test**

Update `assets/test_map.tmx` path or game code to load new map:

```cpp
// In client_main.cpp or wherever map is loaded
tiledMap.load("assets/venoxarid_30x30.tmx");
```

Test:
- Map renders correctly with all 25+ tiles
- Tile variations visible across map
- Toxic pool zones form naturally
- Performance is acceptable (30×30 = 900 tiles vs 15×15 = 225)

### Phase 5: Enemy Spawn System (Implementation)

**5.1 Design Spawn System**

Two approaches:

**Option A: TMX Object Layer (Recommended)**
- Manually place spawn points in Tiled editor
- Add `<objectgroup>` layer with spawn points
- Tag objects with `enemy_type` property
- Engine already has `extractEnemySpawns()` method

**Option B: Procedural Spawns**
- Auto-generate spawns during map generation
- Add to TMX programmatically or separate JSON
- More complex but fully automated

**Recommendation:** Use Option A initially - manually place ~20-30 spawn points in Tiled for the 30×30 map.

**5.2 Define Spawn Rules**

Spawn density for 30×30 map (900 tiles):
- **Slimes:** 15-20 spawns (common, grass areas)
- **Goblins:** 8-12 spawns (medium, near toxic zones)
- **Skeletons:** 5-8 spawns (rare, dense vegetation)

**Total: 28-40 enemy spawns**

**Spawn Placement Guidelines:**
- Cluster spawns in groups of 2-4 (create encounter zones)
- Avoid toxic pool tiles (impassable)
- Prefer bioluminescent and dense moss areas
- Space clusters 5-8 tiles apart

**5.3 Manual Placement in Tiled**

Using Tiled editor:
1. Open `assets/venoxarid_30x30.tmx`
2. Create new Object Layer: "EnemySpawns"
3. Insert Point objects at desired spawn locations
4. Set custom properties:
   - `enemy_type` = "slime" | "goblin" | "skeleton"
   - `name` = "Spawn_Slime_01" (for debugging)
5. Save TMX

**TiledMap.cpp** already extracts these automatically (line 171-238).

**5.4 Alternative: Auto-Generate Spawns**

If manual placement too tedious, create `tools/map_generator/add_spawns.py`:

```python
# Workflow:
# 1. Load existing TMX file
# 2. Parse tile grid to find valid spawn locations
# 3. Reject toxic pool tiles
# 4. Prefer tiles with "moss_dense" or "moss_biolum" edges
# 5. Cluster spawns with Poisson disc sampling
# 6. Add <objectgroup> to TMX with spawn points
# 7. Save updated TMX

# Parameters:
--input venoxarid_30x30.tmx
--slimes 18
--goblins 10
--skeletons 6
--cluster-size 3
--min-distance 6.0
--output venoxarid_30x30_spawns.tmx
```

This would be ~100-150 lines of Python using xml.etree.ElementTree.

### Phase 6: Testing & Iteration (Validation)

**6.1 Visual Testing**

Load 30×30 map in game:
- [ ] All 25+ tile variants visible
- [ ] Terrain zones form naturally (moss → biolum → toxic)
- [ ] Toxic pools isolated and visually distinct
- [ ] No repetitive patterns or visual artifacts

**6.2 Gameplay Testing**

Test with enemy spawns:
- [ ] Enemies spawn at defined points
- [ ] Spawn density feels balanced
- [ ] Encounter zones create interesting combat scenarios
- [ ] Map navigable without getting stuck

**6.3 Performance Testing**

Monitor FPS:
- [ ] 30×30 map renders at 60 FPS
- [ ] Tile batching working efficiently (900 tiles in one draw call)
- [ ] Enemy spawns don't cause frame drops

**6.4 Iteration**

Based on testing results:
- Adjust tile weights if some variants too common/rare
- Add more bioluminescent tiles if map feels dull
- Rebalance enemy spawns if too easy/hard
- Generate multiple 30×30 maps with different seeds to validate variety

## Critical Files

### New Files to Create
1. `tools/map_generator/generate_venoxarid_batch.py` - Batch PixelLab generation
2. `tools/map_generator/venoxarid_manifest_full.json` - Full 25-tile manifest
3. `tools/map_generator/add_spawns.py` - Optional auto-spawn placement
4. `assets/maps/venoxarid_tileset_full.png` - 7×4 sprite sheet
5. `tools/map_generator/tiles_venoxarid_full.json` - Full WFC config
6. `assets/venoxarid_30x30.tmx` - Large test map

### Files to Modify
1. `assets/test_map.tmx` - Update to use new tileset (or load venoxarid_30x30.tmx)
2. Possibly `src/client_main.cpp` - Update map path if needed

### Reference Files (No Changes)
- `tools/map_generator/pixellab_processor.py` - Already works
- `tools/map_generator/generate_map.py` - Already works
- `src/TiledMap.cpp` - extractEnemySpawns() already implemented

## Potential Challenges

### Challenge 1: WFC Contradictions on 30×30 Map
**Impact:** High
**Probability:** Medium (larger maps = more constraints)
**Mitigation:**
- Test with multiple seeds (42, 123, 456, 789, 999)
- Ensure edge types are flexible (most types can connect)
- Keep toxic_pool weight very low (1) to reduce over-isolation
- If persistent failures, reduce to 25×25 or simplify edge types

### Challenge 2: Visual Consistency Across 25 Tiles
**Impact:** Medium
**Probability:** Medium
**Mitigation:**
- Use identical PixelLab parameters for all tiles
- Generate themed groups in same batch session
- Keep color palette consistent (green/yellow/pink)
- Review tiles visually before finalizing manifest

### Challenge 3: Enemy Spawn Balance
**Impact:** Low
**Probability:** Low
**Mitigation:**
- Start conservative (28 spawns for 900 tiles = 3% density)
- Playtest and iterate
- Easy to adjust in Tiled editor

### Challenge 4: Performance on 30×30 Map
**Impact:** Medium
**Probability:** Low (tile batching already optimized)
**Mitigation:**
- Profile with F-keys in game
- Monitor draw calls (should still be 1 for all tiles)
- If needed, implement LOD or culling (but unlikely)

## Timeline Estimate

- **Phase 1 (Design):** 1-2 hours - Plan edge types, tile distribution, weights
- **Phase 2 (Generation):** 2-3 hours - Batch generate 25 PixelLab tiles, process
- **Phase 3 (Edge Assignment):** 1-2 hours - Manually assign edges, validate WFC
- **Phase 4 (30×30 Map):** 0.5-1 hour - Generate and test loading
- **Phase 5 (Enemy Spawns):** 1-2 hours - Manual placement or auto-script
- **Phase 6 (Testing):** 1-2 hours - Gameplay validation and iteration

**Total: 6.5-12 hours** (depends on iteration and polish)

## Success Criteria

### Must Have
- [ ] 20+ unique Venoxarid tiles generated with PixelLab
- [ ] 5+ edge types defined for terrain variety
- [ ] 30×30 map generates successfully without WFC errors
- [ ] Map loads and renders correctly in game
- [ ] 20+ enemy spawns placed on map

### Should Have
- [ ] 25-28 tiles for maximum visual variety
- [ ] 6-7 edge types with clear terrain zones
- [ ] Multiple 30×30 maps with different seeds
- [ ] Clustered enemy spawns for encounter design
- [ ] Auto-spawn placement script for future maps

### Nice to Have
- [ ] 30+ tiles including edge transition variations
- [ ] Toxic pool zones form interesting patterns
- [ ] Enemy density feels balanced for 4-player co-op
- [ ] Map generation documented for future biomes

## Next Steps

1. **Approve this plan** - Review and confirm approach
2. **Phase 1: Design edge types** - Define 6-7 edge types and tile distribution
3. **Phase 2: Batch generate tiles** - Create PixelLab batch script and generate 25 tiles
4. **Phase 3: Configure WFC** - Assign edges and weights, test small maps
5. **Phase 4: Generate 30×30** - Create large test map
6. **Phase 5: Add spawns** - Place enemy spawn points
7. **Phase 6: Test & iterate** - Validate and polish
