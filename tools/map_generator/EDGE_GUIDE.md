# Edge Type Definition Guide

## Understanding Isometric Tile Edges

For Wave Function Collapse to work, we need to label each side of every tile with an "edge type". Two tiles can be placed adjacent to each other only if their touching edges have **matching edge types**.

## Edge Philosophy

Think of edge types as "connectors" or "sockets":
- A tile with a `flat_top` edge on the north side can connect to another tile with a `flat_top` edge on its south side
- A `wall_north` edge cannot connect to a `flat_top` edge (would look visually wrong)
- Edges must match **visually and geometrically** at the seam

## Defining Edge Types for Your Tileset

### Step 1: Identify Visual Categories

Look at your tileset and group tiles by their edge characteristics:

1. **Flat surfaces** (horizontal ground level)
2. **Walls** (vertical faces)
3. **Ramps** (slanted surfaces)
4. **Stairs** (stepped surfaces)
5. **Empty/void** (transparent areas where nothing is rendered)

### Step 2: Consider Height Levels

Isometric tiles often have different heights. Edges need to specify height:
- `flat_high` vs `flat_low` - tiles at different elevations
- `wall_top` vs `wall_bottom` - top edge vs bottom edge of a wall

### Step 3: Consider Orientation

Walls and ramps have direction:
- `wall_facing_north` - a wall face pointing north
- `ramp_ascending_east` - a ramp going up toward the east
- `stairs_up_south` - stairs going up to the south

### Step 4: Label Each Tile's Edges

For each tile, identify what's visible on each of its 4 edges:

```
        NORTH
         ___
  WEST  |   |  EAST
        |___|
        SOUTH
```

Example: Flat Block (ID 0, 2, etc.)
```json
{
  "id": 0,
  "name": "flat_block",
  "edges": {
    "north": "flat_ground",
    "south": "flat_ground",
    "east": "flat_ground",
    "west": "flat_ground"
  }
}
```

Example: Wall (hypothetical)
```json
{
  "id": 72,
  "name": "wall_north_facing",
  "edges": {
    "north": "wall_face",      // The wall face itself
    "south": "flat_ground",     // Behind the wall is ground
    "east": "wall_side_left",   // Side of the wall
    "west": "wall_side_right"   // Side of the wall
  }
}
```

Example: Stairs Going Up North
```json
{
  "id": 66,
  "name": "stairs_up_north",
  "edges": {
    "north": "stairs_top",      // Top of stairs
    "south": "stairs_bottom",   // Bottom of stairs
    "east": "stairs_side",      // Side view of steps
    "west": "stairs_side"       // Side view of steps
  }
}
```

## Practical Tips

### Start Simple

Begin by defining just a few edge types:
- `flat` - flat ground level surfaces
- `wall` - wall faces
- `void` - empty/transparent

Get those working in WFC first, then refine.

### Use Symmetry

Many tiles are rotations of each other:
- A wall facing north is the same tile rotated 90° for east/south/west
- Instead of defining 4 separate tiles, define one and add rotation rules

### Test Compatibility

Two edges are compatible if they would look correct when placed next to each other:
- `flat` + `flat` = ✓ (two flat grounds meet seamlessly)
- `wall_face` + `wall_face` = ✗ (two wall faces touching looks wrong)
- `wall_face` + `void` = ✓ (wall facing empty space - edge of map)
- `stairs_bottom` + `flat` = ✓ (stairs start from flat ground)
- `stairs_top` + `flat_high` = ✓ (stairs lead to elevated ground)

### Visual Inspection

When in doubt, look at your collision map! The outlines show exactly what geometry exists at each edge.

## Recommended Edge Types for Your Tileset

Based on your tiles, I suggest starting with these edge types:

### Core Types
- `flat_ground` - Standard flat surface
- `void` - Empty space (transparent)

### Walls
- `wall_face_n` - Wall facing north
- `wall_face_s` - Wall facing south
- `wall_face_e` - Wall facing east
- `wall_face_w` - Wall facing west
- `wall_side` - Side edge of a wall (seen from perpendicular angle)

### Vertical Surfaces
- `ramp_up_n` - Ramp ascending toward north
- `ramp_up_s` - Ramp ascending toward south
- `ramp_up_e` - Ramp ascending toward east
- `ramp_up_w` - Ramp ascending toward west
- `ramp_down_*` - Mirror of ramp_up for descending

### Stairs
- `stairs_base` - Bottom of stairs (where you start climbing)
- `stairs_top` - Top of stairs (where you finish climbing)
- `stairs_side` - Side view of stairs
- `stairs_step` - Individual step edge (for multi-directional stairs)

### Special
- `corner_inner` - Inside corner piece
- `corner_outer` - Outside corner piece
- `thin_wall` - Thin wall/pillar edge

## Next Steps

1. **Open `tileset_grid.png`** and study each tile
2. **Look at the collision map** to see the actual geometry
3. **Start filling out edge types** in `tiles_skeleton.json`
4. **Begin with just the flat blocks** (tiles 0-26) to get a feel for it
5. **Test with WFC** on a small 5x5 grid
6. **Iterate and refine** edge types based on visual results
