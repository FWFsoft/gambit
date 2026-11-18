# Design decision log

Every decision that changes how the game plays, looks or feels gets an entry
here: mechanics, tuning, UI, content, art direction. This is how humans who
did not see the work can review it. Pure engine or tooling changes do not
need an entry.

Newest first. Use this format:

```
## YYYY-MM-DD: <short title>
- **Decision:** what was decided (and the numbers, if any)
- **Pillars:** which pillars it serves, and any it trades against
- **Why:** the reasoning, including alternatives rejected
- **Status:** Proposed | Built, needs review | Accepted (by whom) | Reverted
- **Where:** files, PR
```

Leave the status at **Built, needs review** until a human accepts it. When a
decision goes against a pillar or a design doc, say so plainly in **Why**.

---

## 2026-11-18: Isaac's Lifeline anchors to Kade's mirrors
- **Decision:** Kade's mirrors (anyone's) count as terrain for Lifeline.
  Isaac bonds to whichever rock, hull face or mirror is nearest the
  cursor (an ally near the cursor still comes first), heals as he would
  from a rock, and on release is pulled to the mirror's glass, stopping
  against it. If the mirror goes while he is bonded (Kade sets a fourth
  one down, or the round ends), the bond breaks and there is no pull.
- **Pillars:**
  - Creative Builds: serves. A cross-kit combo: Kade places anchors
    where Isaac needs them, in open ground with no rock.
  - Comradery in Competition: serves, with a small risk. It rewards the
    two players working together, but Kade replacing his oldest mirror
    can break Isaac's bond mid-cast. That is a side effect, not a way to
    grief, but watch for it in playtests.
  - Clarity: slightly weaker. As with the ship, mirrors in reach are not
    rimmed for Isaac the way rocks are.
- **Status:** Accepted (requested by a human)
- **Where:** `src/CharacterKits.cpp` (IsaacKit), `tests/test_isaac_kit.cpp`

## 2026-11-18: Isaac's Lifeline anchors to the ship
- **Decision:** The ship's hull counts as terrain for Lifeline: aim at
  the ship and Isaac bonds to the nearest face of its current tier's
  footprint, heals, and is pulled there on release, exactly as with a
  rock. Only the current tier counts.
- **Pillars:**
  - Strategic Crunch: serves. A Lifeline to the ship is a fast way home
    at the end of a round, and cuts dead time walking back.
  - Creative Builds: serves. Isaac's mobility now works with the match's
    home base, not just with rocks.
  - Clarity: slightly weaker. Rocks in reach are rimmed for Isaac; the
    ship is not. Its size makes it an obvious anchor, but add a hint if
    playtesters miss it.
- **Status:** Accepted (requested by a human)
- **Where:** `src/CollisionSystem.cpp` (terrainNear),
  `tests/test_isaac_kit.cpp`

## 2026-11-18: Scrap upgrades the ship: dug out, then a fort
- **Decision:** Scrap delivered to the ship still repairs 180 hull, and
  now also goes into the crew's bank. Between rounds (intermission only)
  anyone can spend the bank on the ship's next tier: the UPGRADE SHIP
  button on the suit screen, or U. Tier 1, "dug out and loosely patched"
  (`art/models/ShipPatched.glb`), costs 3 scrap; tier 2, the fort
  (`art/models/ShipFort.glb`), costs 5 more. Each tier:
  - raises max hull 1500 / 2000 / 2600 and repairs it fully;
  - widens the safe zone 420 / 480 / 560 and its healing 4% / 5% / 6% of
    max health a second;
  - makes the roof turrets hit harder and reach further (75 / 95 / 115
    damage, range 560 / 620 / 680);
  - swaps the ship's look and its collision footprint (Tiled types
    `hull`, `hull1`, `hull2`; only the current tier's is solid). The new
    hull stands on the crash site, level, facing the same way. Anyone
    left standing inside it is moved out in front.
  After an upgrade the suit screen stands aside for 3.5 s and a "SHIP
  UPGRADED" banner plays, so the crew sees what it built. A new tier
  is repaired in full, so its fires are out until raiders hurt it again.
  The old 3-scrap reward (an invisible 14-damage gun at the ship's
  centre, "SHIP TURRETS ONLINE") is removed.
- **Pillars:**
  - Strategic Crunch: serves. Every haul home is now worth twice
    (repair now, upgrade later), which makes the "one more objective or
    head back" choice weightier. Higher tiers ease late pressure (more
    hull, bigger safe zone), which the later rounds' bigger raids are
    meant to offset; watch whether a fort makes round 3 too safe.
  - Comradery in Competition: serves. The bank is shared and anyone
    can spend it, and the hauler still scores 100 per scrap.
  - Clarity: serves. The ship's look says its tier at a glance, the HUD
    shows "Scrap n/cost" in green with [U] when an upgrade is affordable,
    and the removed invisible gun was the least readable thing about the
    ship.
  - Creative Builds: neutral. Upgrades are a fixed path, not a choice of
    build.
- **Why:** Requested by a human, who supplied both models and picked the
  rules (upgrades at intermission, all four kinds of perk, costs 3 then 5,
  remove the old scrap gun). The numbers for each perk are first guesses.
  The fort's look hints that the crew may never leave the planet; how
  that plays into a match ending is an open question.
- **Status:** Accepted (requested by a human); the per-tier numbers need
  playtesting
- **Where:** `include/ArenaData.h` (SHIP_TIER_*), `src/MatchDirector.cpp`
  (onShipUpgrade), `src/CollisionSystem.cpp` (hull tiers),
  `assets/maps/arena.tmx`, `src/ArenaView.cpp` (SHIP_TIER_LOOK,
  WRECK_SPOTS), `src/ArenaHud.cpp`,
  `tools/sprite_forge/actors/ship_patched.py`, `ship_fort.py`,
  `tests/test_roof_turrets.cpp`

## 2026-11-18: The match opens just after the crash, with the hull at 35%
- **Decision:** The ship starts every match at 35% hull (525 of 1500),
  heavily damaged and burning, instead of at full health. Each scrap
  banked still repairs 180, so about five hauls bring it back to full.
- **Pillars:**
  - Strategic Crunch: serves. The hull is in danger from the first raid,
    so hauling scrap home competes with exploring from the start.
  - Clarity: serves. The opening shot is a smoking, burning wreck, which
    tells the story, and the fires die down as the crew repairs it.
  - Comradery: neutral. Creative Builds: neutral.
- **Why:** Requested by a human ("the ship has just crashed and is
  heavily damaged"). 35% is a first guess. Risk: a raid early in round 1
  can now end the match (the ship being destroyed is a loss). The bot
  simulator cannot judge this, because its bots neither bank scrap nor let
  raiders reach the hull; 5 seeds all ended in victory with the hull
  untouched. Playtest the first raid.
- **Status:** Accepted (requested by a human); the 35% needs playtesting
- **Where:** `include/ArenaData.h` (SHIP_START_HEALTH),
  `src/MatchDirector.cpp` (resetMatch)

## 2026-11-18: Repaired Derelict Turrets become roof turrets on the ship
- **Decision:** Each Derelict Turret objective the crew repairs stands up
  one cannon on the ship's roof (two mounts; the human's Meshy model
  "RV Spaceship Roof Cannon", `art/models/ShipTurret.glb`). Nothing stays
  at the objective; the old 3-second Wound aura there is gone. A roof
  turret swivels on its round base to the nearest enemy within 560 of its
  mount (300 deg/s, 16 drawn headings), fires once it is within 12 deg,
  and deals 75 a shot every 1.1 s (a grunt, 70 health, in one shot). It
  kicks back and flashes at the muzzle. A kill goes to the player who last
  hit the enemy (score, kill count, loot, on-kill infusions); an enemy no
  player touched dies with no credit. Turrets fire during rounds and the
  boss fight and come down with the next match. Banner: "ROOF TURRET
  ONLINE".
- **Pillars:**
  - Comradery in Competition: the turret never steals a kill. It finishes
    what a player started and the credit is theirs, so it backs up fights
    near home instead of competing with the crew.
  - Clarity: the guns are visible on the roof, turn toward their target
    before firing and recoil with a flash; the hit shows a damage number.
  - Strategic Crunch, slightly weakened: two guns at 68 DPS each make
    the ship much safer against raids and make fighting near home easier.
    The trade-off is the time spent repairing them instead of scavenging.
    Watch whether players camp by the ship once both are up.
- **Why:** Requested by a human ("high damage", kill credit to the last
  player to hit). 60 damage was tried first and does not one-shot the
  grunts on the arena map, so it went to 75. The older scrap reward
  ("SHIP TURRETS ONLINE" after 3 scrap) was removed on 2026-11-18 when
  scrap started buying ship tiers.
- **Status:** Accepted (requested by a human); the numbers (75 damage,
  1.1 s, range 560, turn rate) are first guesses and need review
- **Where:** `src/MatchDirector.cpp` (onTurretRepaired, updateRoofTurrets),
  `include/ArenaData.h` (ROOF_TURRET_*), `src/ArenaView.cpp`
  (drawRoofTurrets), `tools/sprite_forge/actors/ship_turret*.py`,
  `tests/test_roof_turrets.cpp`. Playtest cheat: F2.

## 2026-11-18: The crashed ship is solid, and it burns
- **Decision:** The hull has a collision footprint: 11 stepped boxes
  ("ShipHull_*", Tiled type `hull`) following the lower body of the
  hull as drawn, from the rubble at the nose to the rockets. Players and
  enemies are blocked; raiders now attack the nearest face of the hull
  instead of walking to its centre. Anything standing south of the
  footprint is drawn in front of the whole ship. Spawns and the "bring
  everyone home" spot moved to open ground in front of the hull. The
  wreck smokes and burns: the cab and a rocket nozzle always smoulder
  (it crashed), and as hull health drops the smoke thickens and up to four
  more spots catch fire (at 25%, 45%, 65% and 80% damage).
- **Pillars:**
  - Clarity: the ship's health can be read at a glance from how much of it
    is on fire, and you can no longer walk over the hull.
  - Strategic Crunch, slightly weakened: going around the 11 m hull is
    a few seconds' walk. The drop-off and safe rings still reach past it,
    so delivering scrap is no slower.
- **Why:** Requested by a human; the previous entry flagged the missing
  collision as the next fix. The footprint follows the hull image rather
  than the model's true ground contact: the rockets point up into the air,
  so the true contact is only the nose and a strip of empty-looking
  ground below the hull, which would block where nothing is drawn.
- **Status:** Accepted (requested by a human); the footprint shape and
  the fire thresholds need review
- **Where:** `assets/maps/arena.tmx`, `include/CollisionSystem.h`,
  `src/EnemySystem.cpp` (shipPoint), `src/ArenaView.cpp`
  (drawShipWreckage, depth sort), `src/MatchDirector.cpp`
  (bringPlayersHome)

## 2026-11-17: The ship is a crashed RV with rockets, at true scale
- **Decision:** The ship is now the human's model (`art/models/Ship.glb`, a
  crashed RV-turned-spaceship nose down in rubble). It is baked by the
  sprite forge at the heroes' scale: about 11 m bumper to nozzles, some
  510 px on screen, against heroes under 2 m. The old flat sprite was a
  192 px sailing ship and stays only as a fallback. The ship's radii grow
  to reach past the bigger hull:
  - enemies attack it within 240 (was 110);
  - drop-off within 300 (was 190);
  - "inside for the night" within 420 (was 300).
- **Pillars:**
  - Clarity: home base is unmistakable and in the same inked style as the
    cast.
  - Strategic Crunch, slightly weakened: the bigger safe and drop-off
    circles make the trip home a little shorter. Revisit the numbers if
    rounds feel loose.
- **Why:** The human said the ship was far too small and the wrong kind of
  ship, and supplied the model. The radii had to grow, or drop-off would
  sit inside the hull's image. Nothing blocks movement yet: players and
  enemies walk over the hull, which reads worse now that it is big. That
  needs a collision footprint next.
- **Status:** Built, needs review
- **Where:** `tools/sprite_forge/actors/ship.py`,
  `assets/sprites/actors/Ship.png`, `src/ArenaView.cpp` (drawShip),
  `include/ArenaData.h` (SHIP_* radii), `tools/sprite_forge/forge.py`
  (one-row sheets for big props)

## 2026-11-17: Kade's mirrors: held placement, out-of-the-glass reflection, Full Release, Mirror Step
- **Decision:** The rest of Kade's kit.
  - **Mirrors (passive):** person-high (1.9 m, his height) panes that stand
    until replaced, 3 at most; a 4th replaces the oldest. Each faces one
    of 8 ways. Anything that strikes one leaves **straight out of its
    glass**: his beams (RMB and Q) and allies' projectiles. Each mirror
    turns a beam once.
  - **LMB, Refraction Wall:** held. The press pins a blue hologram of the
    mirror at the cursor (reach 240), and circling the cursor around it
    turns the glass, snapped to 8 ways. A floor arrow shows where struck
    things will go. On release he dashes (150 ms) to arm's length, slaps
    the floor, and the mirror swings up out of it.
    - The new mirror joins his previous one with a beam (push 160) that
      stays as a barrier between consecutive mirrors, oldest to newest.
      Crossing it gives Knockback 120 and +1 Expose, at most once per
      enemy every 0.9 s.
    - **Charges, not a cooldown** (the human's call): 3 charges, one per
      mirror, with a 350 ms lockout between drops, so three can go down
      almost at once. A spent charge comes back after 5 s, one at a time.
      With 3 mirrors standing, the next drop replaces the oldest, but only
      once a charge is back. He moves slowly while aiming.
  - **Q, Full Release:** a 3 s beam that follows the cursor at 200°/s,
    reach 620. After each mirror it turns, at least 320 is left. Stuns
    whatever it touches (0.8 s, refreshed every 0.5 s). He moves at 30%
    while it runs and can cast nothing.
    - Then he is **spent** for 1.2 s: rooted, hunched over his knees,
      violet smoke curling off him. Cooldown 16 s.
  - **LSHIFT, Mirror Step:** needs a mirror. He leaves a mirror where he
    stands (facing the way he went) and comes out just past the mirror
    nearest the cursor. That mirror is never the one replaced. Cooldown
    8 s.
  - **HUD:** three charge pips on the LMB box. A lit pip is a charge
    ready, and the next charge fills from the bottom as it comes back. With
    none left, the box greys out with a countdown. While he aims a 4th
    mirror, the one it will replace flickers in the world.
  - A mirror Kade is standing in does not catch his own beam as it leaves
    him; the human's playtest showed that confusing.
- **Pillars:**
  - Creative Builds: mirror angles make a geometry puzzle. Beams can be
    routed round corners and allies' shots bounced. The barrier, the Q and
    the step all depend on where the mirrors stand.
  - Comradery: teammates' projectiles use his mirrors too, so placing them
    for the team is "when I win, we win".
  - Clarity: the hologram, the facing arrows, the slot pips and the
    "next to go" flicker. The Q telegraphs its whole bent path.
  - Strategic Crunch: the spent window is a real cost after the Q, and
    Mirror Step spends a mirror slot.
- **Why:**
  - **Pinned by the human:** reflection out of the glass, the held
    hologram with 8 facings, the slot indicator, the dash-and-slap, the
    mirror's height, and the Q's recovery with smoke.
  - **Trade-off, flagged:** the slap puts the mirror where his hand lands,
    so placement reach dropped from 380 to 240 and placing moves him
    toward it. That is less safe and less far, and takes him toward
    danger. Raise MIRROR_RANGE in KadeKit if it feels cramped; the dash
    lengthens to match.
  - **My calls, not asked for:**
    - the barrier joins consecutive mirrors whatever their facing;
    - enemy projectiles pass through mirrors (only the players' turn);
    - mirrors cannot be destroyed yet, although the passive says "until
      destroyed";
    - a mirror whose Kade is down keeps standing, but its barrier stops.
  - **Replaced:** my first Q relayed light through every mirror,
    nearest-next. The human's facing rule is predictable and gives the
    player control, so it replaced that.
  - Numbers are prototype tuning.
  - **Checks:**
    - `silhouette.py --legacy` flags no look-alike for the mirror.
    - It does flag Kade himself against VenoxaridGrunt (0.85), Wade and
      old Presidente. That comes from his idle pose, not this change, and
      weakens Clarity until his idle gets a stronger shape.
    - ArenaSim's bots do not play Kade, so there is no balance run.
- **Status:** Built, needs review
- **Where:**
  - `src/CharacterKits.cpp` (KadeKit), `src/AbilitySystem.cpp`
    (findZone/endZone, mirror-turned projectiles),
    `include/ArenaData.h` (KadeMode, mirrorFacing), `src/ArenaData.cpp`;
  - `src/ArenaView.cpp` (mirrors, hologram, barrier, smoke, clips),
    `src/ArenaHud.cpp` (pips), `src/ArenaClient.cpp`;
  - `tests/test_kade_kit.cpp`, including the human's mirror-to-mirror
    bounce;
  - sprites from `tools/sprite_forge/actors/kade.py` (slap, release_hold,
    spent, step, all code-posed, for the human to polish) and
    `kade_mirror.py` (model `art/models/KadeMirror.glb`);
  - `docs/prototype/kade_kit.gif`.

## 2026-11-17: Kade joins the roster with Focused Blast (RMB) only
- **Decision:** Kade (the Prism) is playable as a **Playmaker** (240 HP,
  255 speed), with the human's kit text on the HUD. Only **Focused Blast**
  works: hold up to 1.5s, release an optic beam from the visor along the
  aim. Reach 200 to 430, half-width 16 to 46, push 70 to 240, all growing
  with the charge; a full charge also Stuns for 1s. Cooldown 6s. No
  damage (Playmakers deal none); hits still count for on-hit infusions.
  Refraction Wall, Full Release and Mirror Step are listed but refuse to
  cast ("Not in the game yet"), as is the passive (mirrors). (Since built:
  see "Kade's mirrors" above.)
- **Pillars:** Clarity: the charge reads at a glance (his pose follows
  the charge level, a dashed ring builds round his head, the visor
  brightens, arcs crackle) and the beam shows its reach and width before
  it pushes. Strategic Crunch: a 6s cooldown and a 1.5s hold make the
  full-charge stun a commitment. Creative Builds / Comradery are **weak
  until the mirrors exist**: the kit's identity (beams reflecting off
  mirrors, barriers) is not built, so today he is a pusher with a stun.
- **Why:** The human asked for the RMB in the game first. "Blast radius"
  follows the KADE RMB reference sheet: a wider, longer beam, not an
  explosion at the cursor. Playmaker because the kit is all crowd control
  (knockback, stun, barriers), per ARCHETYPES.md. The numbers are
  prototype tuning. A new effect colour, Optic (VHS blue, magenta
  streaks), keeps his light distinct from Wind and Ice.
- **Status:** Built, needs review
- **Where:** `src/CharacterKits.cpp` (KadeKit), `src/ArenaData.cpp`,
  `src/ArenaView.cpp` (clips, charge effects, optic beam),
  `include/NetworkProtocol.h`, `include/ArenaDraw.h`; sprite
  `assets/sprites/actors/Kade.png` (poses posed by hand in the pose
  editor; run, hit and death are code-built stand-ins);
  `docs/prototype/kade_focused_blast.gif`

## 2026-11-17: Kade's rig gets collarbones and closing fists (opt-in)
- **Decision:** The sprite forge's skin can add finger joints (hands that
  close into fists) and collarbones (shoulders that shrug and roll) for an
  actor that asks for them. Only Kade uses them; no other actor changes.
- **Pillars:** Clarity: fists and shoulder posture read in the silhouette
  (a clenched charge, a hunched brace).
- **Why:** The human could not pose Kade's release key without moving the
  shoulders, and asked for fists that close as the move charges.
- **Status:** Built, needs review
- **Where:** `tools/sprite_forge/skin.py`, `tools/sprite_forge/actors/_rig.py`,
  `tools/sprite_forge/pose_editor.html`

## 2026-11-17: Infusions are items; the suit is a core plus four tiered trees
- **Decision** (requested by a human, from the Genera and Infusion doc):
  - **Items.** An infusion is a genus plus a body part: Talon (+6%
    damage), Hide (+12 max health), Eye (abilities recharge 6% faster),
    Sinew (+4% move speed). Drops roll the part at random; the genus still
    follows the territory. The part's effect applies wherever the item is
    slotted (core or tree), per copy. Items in the pack do nothing yet.
  - **Core.** The first infusion must go in the core and gives that
    genus's core ability (E), as before. Permanent for the match.
  - **Trees.** Offense, Defense, Mobility and Genealogy are each four tiers
    deep. Tier 1 is one slot (the "leg" out of the core); tiers 2 to 4 are
    two options each, and taking one closes the other. A tier opens once
    the tier above is filled. Every slot gives its **talent** plus the
    infusion's **genus effect for that tree**; each infusion of the same
    genus in the same tree raises that effect's level. Different genera can
    now share a tree (both effects run); nothing is ever overwritten.
    Slots are permanent for the match (no respec).
  - **28 talents** (`SUIT_NODES` in `src/ArenaData.cpp`): Offense: Killer
    Instinct; Opening Strike / Relentless; Affliction / Executioner;
    Bloodlust / Focus Fire. Defense: Plating; Second Wind / Regrowth;
    Guardian / Thorns; Last Stand / Share the Load. Mobility: Light Frame;
    Quick Recovery / Slipstream; Hit and Run / Tailwind; Blitz / Escape
    Artist. Genealogy: Bloodline; Purebred / Hybrid Vigor; Overcharge /
    Shared Gift; Apex Mutation / Feedback Loop. Numbers are first guesses.
  - **Built to grow.** The layout is one data table; adding options to a
    tier (width) or tiers to a tree (depth) is a table row plus the
    talent's code. Up to 64 nodes fit without changing the network format.
  - **Suit screen** follows the doc's sketch: core in the middle, each tree
    growing into its own corner, pack on the left, a "your suit" summary on
    the right. Hovering a pack item shows the doc's tooltip (the item's own
    effect, then the genus effect for each tree with its next level).
- **Pillars:**
  - Crunch: serves. Each tier is a permanent either/or, and with about 0
    to 6 tree slots filled per match in the bot sims, where to spend
    scarce infusions is a real choice.
  - Comradery: serves. Five talents pay off for the team (Focus Fire,
    Guardian, Share the Load, Tailwind, Shared Gift); none can hurt an
    ally. Share the Load can bring its owner down, by their own choice.
  - Creative Builds: serves, with a caveat. Genera now mix inside a tree,
    and Purebred vs Hybrid Vigor makes mono-genus vs mixed suits a real
    fork. But a third of the talents are flat numbers (Plating, Light
    Frame, Killer Instinct, Bloodline...), and the doc asks for talents
    "really impactful like League runes": those are the first to replace.
  - Clarity: mostly serves on the suit screen (open slots pulse in the
    selected item's colour; closed options grey out). In combat, the
    damage talents (Opening Strike, Executioner, Affliction, Relentless,
    Blitz) have no hit feedback of their own yet, and Share the Load's
    beam borrows the ice colour.
- **Why:** The doc decided "BOTH" (talent plus genus effect per slot) and
  showed per-item stat lines; the earlier build had neither. Against the
  doc: (1) the doc's example items carry specials (Talon: on-hit
  Lacerate; Eye: reveal a key location; Hide: on-hit 5% effective health)
  that are not built; parts are stats only. (2) Two options per tier, as
  in the doc's "pick 1 of 2" diagram (the paper sketch shows three).
  (3) The doc does not say whether items help while still in the pack; we
  chose slotted-only so the ship trip stays the moment you power up.
  Rejected: one talent tree per genus (too many trees to read); free
  respec (removes the weight of each pick).
- **Status:** Accepted (requested by a human); talent list and numbers
  Built, needs review
- **Where:** `include/ArenaData.h`, `src/ArenaData.cpp` (items, layout),
  `src/Infusions.cpp` (rules, talents), `src/ArenaHud.cpp` (suit screen),
  `tests/test_infusions.cpp`

## 2026-11-17: Terrain on the arena; Isaac's Lifeline anchors to it
- **Decision** (requested by Noah, who picked Isaac to force terrain in):
  the arena has terrain: 51 rock outcrops (rectangles of Tiled type
  `terrain` in a new "Terrain" layer of `arena.tmx`, 70 to 150 by 45 to 95
  units). They are about 560 apart, inside the round-3 zone, clear of the
  ship's ring, every objective circle and every spawn; 6 sit inside the
  boss ring. `tools/maps/place_terrain.py` places them from a seed.
  Players collide with them (they load as collision, so dashes and
  prediction agree), and enemies slide off them. Lifeline now needs an
  anchor: an ally within 120 of the cursor, else the rock nearest the
  cursor (within 140), and either must be within 520 of Isaac (to the
  rock's nearest face). Release pulls him to the ally or against the rock.
  A rock still heals Isaac more than an ally bond (7 vs 4 per pulse). With
  no anchor the hold does nothing, a small spark marks the cursor, and the
  cooldown drops to 1.5 s instead of 8 s. The "anywhere on the ground"
  anchor is gone. Rocks are drawn in code as inked stone; when you play
  Isaac, rocks in reach get a faint amber rim and the one you bond to
  pulses.
- **Pillars:** Strategic Crunch serves: crossing explored open ground is
  dead time, and Isaac now chains rock to rock across it. The 560 spacing
  is just under Lifeline's reach on purpose, so each pull is a choice of
  route, not a free teleport. The rocks also give everyone cover and
  corners to fight around. Creative Builds serves: terrain is a new system
  other kits can use later (knock foes into rocks, perch on them). Clarity
  serves: the reach rim and the bond pulse show what Lifeline will do
  before you commit. Small trade against Crunch: the miss refund takes the
  sting out of a wasted Lifeline; without it, misreading the rim would cost
  8 s.
- **Why:** The kit says "an ally or terrain", and the map had none. Rocks
  are collision rectangles because the map, the server, the client
  prediction and the dashes already share that system: no network changes.
  Simplifications to review:
  - Rocks block bodies but not projectiles or sight.
  - Enemies slide off them with no pathfinding. Rocks are kept small and
    apart so nothing gets trapped.
  - The rocks are code-drawn stand-ins, against ART_DIRECTION's rule that
    drawing comes from concept art. They need terrain concept art.

  Bots (Wade, Nolan, Nyx, Isaac; seeds 1 to 3) won every full match with
  and without terrain (559 to 566 s with rocks, 573 to 601 s without), so
  the rocks do not stall the enemy flow or the match. The sim bot now aims
  Lifeline at the rock nearest its fight, else an ally.
- **Status:** Accepted (requested by a human); placement and tuning need
  review
- **Where:** `assets/maps/arena.tmx` (Terrain layer),
  `tools/maps/place_terrain.py`, `src/CollisionSystem.cpp` (`terrainNear`,
  `pushOutOfTerrain`), `src/EnemySystem.cpp`, `src/CharacterKits.cpp`
  (`IsaacKit`), `src/ArenaView.cpp` (`drawRock`), `include/ArenaData.h`
  (`LIFELINE_RANGE`), `tests/test_isaac_kit.cpp`, `tests/arena_sim.cpp`,
  `docs/prototype/isaac_lifeline_rock.png`

## 2026-11-17: Isaac's body and motion
- **Decision:** Isaac's in-game sprite comes from his generated model
  (`art/models/Isaac.glb`, shrunk from 3.2M to 40k triangles for free), on
  joints Noah placed. His motion is a brawler's body that never makes a
  fist (shape brief in ART_DIRECTION.md):
  - idle: an open-palmed stance;
  - Ray: a two-handed push from the chest;
  - Lifeline: a hand out on the rope while he leans back, then pulled
    off his feet on release;
  - Blessing: a hand raised, then brought down toward the target;
  - Flash: hands meet at the heart, then he arrives arms flung wide.
  The run turns his legs in so the toes point where he goes (Noah's
  correction; the rule is now in ART_DIRECTION.md). His cell is 184 by 184
  with the feet higher in it, so his fall stays inside it.
- **Pillars:** Clarity serves: each ability has its own outline. The
  canister rack and cape make him lopsided from every side, and the
  open-handed pose reads as a healer at a glance. Silhouette check:
  Isaac/Suds was flagged at 0.73 facing south-east. Opening his stance
  brought it to 0.72, exactly at the limit, so it passes, but only just.
- **Why:** He needed real art to be playtested. His sheet is 4968 px
  square, over the ~4096 guideline but smaller than Wade's and Gravon's.
  Model-level problem: the weights use the distance fallback, because the
  mesh's holes defeat automatic weighting; the cape follows his left
  shoulder and hip and has not torn in any preview.
- **Status:** Built, needs review
- **Where:** `tools/sprite_forge/actors/isaac.py`, `art/models/Isaac.*`,
  `assets/sprites/actors/Isaac.png`, `src/ArenaView.cpp` (`actionClip`,
  `lifeline_charge`), `tools/sprite_forge/skin.py` (`centre_x`)

## 2026-11-17: Isaac's Ray of Benevolence deals no damage; archetype rules checked in
- **Decision** (requested by Noah): Ray of Benevolence no longer deals 9
  damage per enemy. It still slows (+1) and heals allies per enemy hit, and
  each enemy it touches still counts as a hit for on-hit infusions, through
  a new zero-damage `AbilitySystem::touchEnemy`. The conclusions of
  DD-AA-003 (character archetypes) are now in `docs/design/ARCHETYPES.md`.
- **Pillars:** Creative Builds serves: Wingers have no base damage, so an
  on-hit damage infusion on a slowing cone cannot steamroll from minute
  one, and Isaac's damage now comes only from his build. Comradery, mixed:
  Isaac's score in ArenaSim fell from about 820 to 275 (Wade, Nolan, Nyx,
  Isaac; 8 minutes; seeds 1 to 3), because healing scores far less than
  damage. The team result did not change (round 3 in every run). A Winger
  who heals well should not be last on the scoreboard. That is a scoring
  question for all Wingers, left open. Clarity, slight trade: Ray hits
  no longer pop a damage number. The light cone and the Slow remain the
  feedback.
- **Why:** The kit broke the Winger rule "no DMG in the base kit". Going
  through the damage path with 0 damage would have skipped infusions, which
  only fire on damage above 0. Zephyr and El Presidente (Playmakers) also
  deal base damage, which goes against the same doc; they are not changed
  here.
- **Status:** Accepted (requested by a human)
- **Where:** `src/CharacterKits.cpp` (`IsaacKit`), `src/AbilitySystem.cpp`,
  `include/AbilitySystem.h` (`touchEnemy`), `docs/design/ARCHETYPES.md`

## 2026-11-16: Heroes face where they walk, and turn to the cursor to cast
- **Decision** (requested by Noah): a hero's sprite faces their movement
  direction in 8 ways (W = N, W+A = NW, A = W, ...). While an ability is
  casting or charging they face the cursor instead. Standing still keeps the
  last facing. Before, heroes always faced the cursor. Visual only: aim and
  ability direction on the server are unchanged.
- **Pillars:** Clarity serves: you read where a hero is going from the
  sprite, and the turn to the cursor at cast time shows what an attack is
  aimed at. Small trade against Clarity: an idle hero no longer shows where
  their player is aiming between casts.
- **Why:** Human direction. Movement and aim are separate inputs, and the
  body following the cursor while walking the other way read as sliding.
- **Status:** Accepted (requested by a human)
- **Where:** `src/ArenaView.cpp` (`drawPlayer`), `include/ArenaView.h`

## 2026-11-16: Main menu lets you pick a new character mid-match
- **Decision:** Pause → Main Menu now takes you out of the fight on the
  server (character 0). Your next pick is accepted and you drop in at the
  ship like a late joiner. Leaving drops any scrap you carry and clears
  your effects. Picking again in the same round starts the new kit with
  every ability on full cooldown, and if you left while down you come back
  down where you fell, waiting for a revive (the death is not counted
  twice). Picks are still locked in the fight
  otherwise. Before this, the server ignored the new pick and snapped you
  back to your old character (a bug).
- **Pillars:** Creative Builds (you can try a different hero without
  restarting the match). Trades a little against Strategic Crunch: a
  living player can still use the menu to leave a bad spot and reappear at
  the ship. The price is full cooldowns and the walk back out; the
  down-stays-down rule closes the worst escape (skipping a revive).
- **Why:** Requested fix: the menu offered a character select that did
  nothing. Rejected: only allowing swaps in the lobby/intermission (the
  menu would still lie mid-round).
- **Status:** Built, needs review
- **Where:** `src/MatchDirector.cpp` (`leaveToMenu`, `onCharacterSelected`),
  `src/ClientPrediction.cpp` (`resetCharacterSelection`),
  `tests/test_character_swap.cpp`

## 2026-11-16: Hitstop on every melee hit, and Titan's Slash sweeps through a crowd
- **Decision** (requested by Noah; numbers mine):
  - **Hitstop:** every melee hit (DamageFlag::Melee, not procs or damage
    over time) holds both sides for a beat:
    `min(250, 60 + 2 × power) / (1 + 0.3 × Haste stacks)` ms, where power
    is the hit's damage after crit and Empowered, before armour. Wade's
    punch (15) holds 90 ms, 36 ms at 5 Haste; Gravon's slash (38) 136 ms,
    heave (46) 152 ms, a crit slash 192 ms; capped at 250 ms. (First
    built at roughly half this; Noah could barely see it in play.)
  - **The attacker:** cooldowns, the action lock and channel rhythm stand
    still, the attack clip freezes on the client, and so does the swing's
    slash trail (which also now sweeps the way the blade does). Your own
    hits kick the camera a little (2-3.5 px). Movement does not
    stop (stopping it would fight client prediction). Several targets hit
    in the same instant hold once, not once each.
  - **The target:** frozen in place: AI, windup, knockback and air time all
    wait, then resume. On the client it freezes, shudders and stays lit
    for the whole hold. The boss
    holds a quarter as long, so four players cannot pin it.
  - **Titan's Slash** no longer hits the whole cone at once. The blade
    crosses the half-circle in 120 ms, centred on the old impact time,
    and meets each enemy in order (forehand and run heave right to left,
    backhand and back heave left to right), holding on each: a crowd goes
    boom, boom, boom. The swing's slow lasts as long as the holds.
- **Pillars:**
  - Clarity serves: every hit lands with weight, and heavy hits read as
    heavier than light ones.
  - Creative Builds serves: Haste now changes the feel of a melee kit,
    not just its numbers (Wade accelerates from boom-pause to
    bap-bap-bap as he stacks it).
  - Crunch serves slightly: a sweep through a crowd is a longer
    commitment.
  - Comradery: neutral to watch. Enemy hold is short (max 250 ms, and 25%
    on the boss), but four melee players chain-holding regular enemies
    could stun-lock them.
- **Why:** Gravon lacked hit feedback. Holds make melee DPS fall
  slightly, because cooldowns wait out each hold. A crowd costs Gravon
  most (a 3-enemy slash adds ~450 ms). Wade's rhythm early in a fight
  slows ~24% before Haste. ArenaSim (at the first, shorter numbers) (Gravon, Wade, Suds, Zephyr, seeds
  1-3) won all runs before and after. Wade's score fell (avg 1639 → 986)
  but the sim varies a lot between runs, so it needs more runs before
  anyone tunes on it. Not yet checked in a running client.
  Rejected: holding per target at the same instant (stacks into
  long freezes on wide hits); freezing the attacker's movement (rubber
  bands against prediction).
- **Status:** Built, needs review (playtest the feel; watch Wade's damage
  and enemy stun-lock with several melee players)
- **Where:** `include/ArenaData.h` (HITSTOP_*), `src/AbilitySystem.cpp`
  (`hitStop`, `damageEnemy`), `src/EnemySystem.cpp`,
  `src/CharacterKits.cpp` (Gravon sweep), `src/ArenaClient.cpp`,
  `src/ArenaView.cpp`

## 2026-11-16: Gravon made heavy: the blade out, a flat half-circle, a slam that throws him
- **Decision** (requested by Noah; numbers mine):
  - **Slow:** base speed 205 (it was 255; the cast is 250-260), and 15%
    slower again with the blade out (174). 35% speed through every swing.
  - **Blade out or on his back:** any attack draws it; after 4 s without
    one (counted once a swing or landing ends) he puts it back. Out, he
    stands braced over it with both hands on the grip and the point on
    the floor ("guard"), and runs dragging it behind him in one hand, the
    point scraping sparks ("drag_run"). The game plays "sheathe" when it
    goes back.
  - **Titan's Slash** is one flat 180-degree arc (half-angle 90, range
    135), the blade parallel to the floor, 1 s cooldown. The first swing
    depends on where the blade is: from his back, he drops his left
    shoulder and heaves it off one-handed into a backhand (420 ms, 46
    damage); out of a run (0.25 s on the move with it out), he heaves it
    up from the drag into a forehand (380 ms, 46). After that, two hands,
    back and forth (300 ms, 38), each swing the other way from the last.
  - **Ravager's Edge** release is two motions: he throws it over with his
    feet planted, then its weight pulls him over his planted left foot,
    head below his waist and right leg high, as it slams down flat ahead.
    The server moves him a 15 px lurch (after playtest round 2: 70 px read
    as jumping), the slam landing 300 ms after release on a spot 110 px
    ahead (shown with a violet circle). Then he is rooted and cannot act until 1.2 s after
    release, on his knees behind it, hauling it out of the floor and onto
    his shoulder. Damage unchanged.
- **Pillars:** Clarity and the character's identity (a hero who looks and
  plays heavy; the slam lands where the circle says, and the 1.2 s
  recovery is a window enemies and allies can read). Crunch on the
  attacks: every swing and slam is a commitment. **Trades against
  Crunch's "dead time":** 205 speed makes crossing explored ground slower
  for him than anyone, and every swing slows him to a crawl. Blade Rush
  is his only fast move. If it feels bad in a playtest, the first lever
  is the 0.85 drag multiplier, not the swings.
- **Why:** asked for directly. The heave-first combo makes sheathing a
  real state (opening from the back is slower than opening drawn), so
  keeping the blade out has a cost (speed) and a benefit (quicker swings).
  **Balance check** (ArenaSim, Gravon + Wade + Nyx + Suds, seeds 1-3):
  Gravon 1,575 to 2,230, second in two of three, 1 to 5 deaths. The last
  version was first in two of three, with 1 death each.
- **Art:** the actor now solves the arms with two-bone IK from where the
  hands and blade should be, so both hands are on the grip in every
  two-handed pose (within 8 cm; 10 cm in the stab's chamber). After
  Noah's first look ("he's gesturing with his hand"), the body drives every
  move: in a swing the hips and chest turn first and the blade trails
  them, he coils low on the back leg, lunges and leans into the cut, his
  arms pulled straight by the blade, and the follow-through drags him on
  past it. In the slam he swings about the grip, not his feet, so the blade
  pulls him off the floor. The stab chambers at the hip during the slide,
  then drives through in a deep lunge. The guard hunches lower and heaves;
  the drag run hauls. Swing keys sit on frame times, so no frame blends two
  blade angles. Playtest round 2: the idles are a still power stance that
  only breathes (they swayed like a bowling pin); at rest with the blade
  out, it stands point-down beside him under his right hand. Everything he
  swings he holds in both hands, the heave from a run included; the swings
  start lower and drive from the legs, the back leg straightening, the
  side crunching into the pull. Blade Rush slides two-handed; in the beat
  he sinks into a low stance with the blade raised level with his
  shoulders, hands by his right cheek, and thrusts it straight out at
  shoulder height (round 3). Round 3 also: running (sheathed or with the
  blade out) the legs turn in at the hip so his toes point where he runs
  (the model's feet turn out about 17 degrees, right for the stance only);
  the sheathed run is the same hard, leaning run as the drag, arms
  pumping clear of his sides.
  **Bug fixed (round 3):** the host dying to damage over time ended a
  solo match with the blade still whole: the match director judged the
  team before the kit turned him into the blade. AbilitySystem now settles
  deaths (every kit's lethal hook) at the start of each server tick, before
  the director runs; a match test covers it.
  New clips: guard, drag_run, sheathe,
  heave, heave_run, slash_f, slash_b, hit_drawn, and a 12-frame ravager.
  The sheet bakes at pixel scale 1.55 in a taller cell, to fit the slam's
  reach toward the camera.
- **Status:** Built, needs review
- **Where:** `src/CharacterKits.cpp`, `include/ArenaData.h`
  (GravonMode::DrawnBit, GravonForm), `src/ArenaData.cpp`,
  `src/ArenaView.cpp`, `tests/test_gravon_kit.cpp`,
  `tools/sprite_forge/actors/gravon.py`

## 2026-11-16: Gravon rebuilt: the blade is the hero, the host is its body
- **Decision:** Gravon (id 3, Striker, 240 HP, 255 speed) rebuilt from the
  kit sheet; the earlier prototype is replaced, not extended.
  - **Soulblade (passive):** the blade and the host have separate health.
    The host has the kit's 240; the blade 120, kept between falls and shown
    as his meter while the host stands. When the host falls the player *is*
    the blade: it stands point-first in the floor where he fell, exposed
    (enemies target and hit it), and cannot move or act. After 15 s, 1 s
    less per enemy that dies within 450 px, it raises the host where it is,
    at 60% health, and becomes untargetable again. Any ally who walks over
    it (55 px, after 0.6 s) picks it up and carries it on their back, where
    nothing can touch it; if the carrier falls it drops. If the blade
    breaks, Gravon is down like anyone else and is revived as the host.
    **Interpretations:** no once-per-round limit (the sheet gives none), so
    every fall is a blade phase; carrying costs the carrier nothing; a
    death from damage over time also leaves the blade (the death hook now
    fires for every kit, not only on hits).
  - **LMB Titan's Slash** (0.95 s): a 260 ms wind-up (he moves at 35%
    speed through the swing), then 38 damage and a short knockback in a
    150-degree arc out to 135 px. Alternates backhand and forehand clips.
    Cancelled if the host falls mid-swing.
  - **RMB Ravager's Edge** (hold up to 1.5 s, 5 s): **roots him** while it
    builds. On release the blade slams down 95 px ahead: 35 to 150 damage
    in a 110 to 160 px circle. At full charge, +1 Sharpened and a
    guaranteed crit (so 300 at full, about 8 times a slash).
  - **Q Fury of the Blade** (14 s): +1 Empowered and +1 Haste for 6 s.
    While either is on him, each hit takes 0.7 s off Blade Rush. Dark
    violet ooze runs off him while it lasts.
  - **Shift Blade Rush** (7 s): two beats. A 190 px slide (170 ms,
    through enemies), a 90 ms beat, then a 45 px lunge and a stab of 62 to
    the first enemy within 140 px ahead.
- **Pillars:** Comradery (the carry: picking him up is a way to help that
  only an ally can give, and it is drawn on them for everyone to see);
  Crunch (a heavy slash and a rooted charge are commitments; the slash's
  wind-up and slow make positioning matter); Creative Builds (Fury's
  refunds and the full-charge Sharpened feed crit and Haste builds);
  Clarity (the blade is the tallest thing on the field, with a ring that
  fills as the host returns). **Trades against Crunch:** the 15 s blade
  phase is waiting with little to decide for Gravon's own player. Kills
  nearby shorten it and being carried keeps him in the fight, but it is
  dead time; watch it in playtests.
- **Why:** built from the kit sheet and the request ("slow weighty arcing
  strike", "a charge that locks him in place... a large slam", purple ooze
  on Q, "two distinct chunky motions" on Shift). The blade as its own
  sheet follows Nyx's CRT. Rejected: drawing the host's body at the fall
  spot for the whole phase (he would then rise somewhere else when the
  blade has been carried away); the body sinks into violet smoke instead
  and the host re-forms at the blade.
  **Balance check** (ArenaSim, Gravon + Wade + Nyx + Suds, seeds 1-3):
  Gravon scored 2,762 to 3,296, first in two of three, with 1 death each
  game. Host falls are not counted as deaths, so that number hides how
  often he goes down. He may be strong; the open questions are the
  missing once-per-round limit and the free carry.
- **Art:** body from the Meshy host model, on the joints Noah placed in the
  fit editor. The model was shrunk locally (1.24M to 40k triangles), and
  the wrist chains were cut out (`tools/sprite_forge/cut.py`): they hang
  straight down in the A-pose and the forearms swung them through his legs.
  The blade (Meshy, 20k triangles) is 2.4 m long against his 2.0 m, worn on a
  30-degree diagonal per Noah's reference. The sheet bakes at pixel scale
  1.25 to fit the reach of the blade in a reasonable sheet.
- **Status:** Built, needs review
- **Where:** `src/CharacterKits.cpp` (GravonKit), `include/ArenaData.h`
  (GravonMode, GravonForm), `src/ArenaData.cpp`, `src/AbilitySystem.cpp`
  (the death hook), `src/ArenaView.cpp`, `tests/test_gravon_kit.cpp`,
  `tools/sprite_forge/actors/gravon.py`, `gravon_blade.py`,
  `tools/sprite_forge/cut.py`, `art/models/Gravon*`

## 2026-11-16: Nyx, the Hive Mother: a nanite swarm, a CRT she sets down
- **Decision:** Nyx (id 13) is playable as a Playmaker, 230 HP, 258 speed,
  built from Noah's ability sheet. Her body and CRT are Noah's Meshy models
  (`art/models/Nyx.glb`, `NyxCRT.glb`, shrunk locally for free).
  - **Hive Mother (passive):** her swarm rides one target. On an enemy it
    reapplies Silenced each second; Silenced's existing secondaries add +1
    Dulled and +1 Weakened. On an ally it reapplies Focused, whose
    secondary adds +1 Sharpened. The swarm holds the status (1.4 s) and
    does not bank time on it. If the status is gone (cleansed, purged, or
    blocked by Inspired or Cursed), or the host dies, the swarm flies home.
    It also flies home if Nyx is more than 760 px from it. Area abilities
    go off from both Nyx and the swarm, and area core abilities (Miasma,
    Mind Flay, Cauterize) go off a second time at the swarm. Diamond
    Fortress is armour on her, so it does not repeat.
  - **LMB Nanite Swarm** (0.45 s): sends the swarm up to 560 px, at
    900 px/s, to the cursor. It attaches to the enemy or ally whose body
    is within 70 px of the cursor. Enemies entering it gain +1 Slow, once
    per pass.
  - **RMB Plague Bringer** (hold up to 0.9 s, 6 s): a wave of 150-260 px
    from Nyx and from the swarm. Enemies gain +2 Weakened, allies
    (Nyx included) +1 Empowered. Up to 3 debuffs on each swarm host, the
    largest stacks first, are **contaminated** onto every enemy hit: they
    are copied, with an Effect Spread bolt from host to victim. On an enemy
    host this is the swarm's own Silence, Dulled and Weakened, so Plague
    Bringer spreads the infection. **Interpretation:** an ally host loses
    the debuffs it passes on; an enemy host keeps them.
  - **Q Nanite Cloud** (9 s): every target a swarm rides becomes Marked
    (enemy) or gains Stealth (ally). Refused, with no cooldown spent, if
    nothing is attached.
  - **Shift Become the Swarm** (8 s): she picks an enemy or ally within
    460 px of her and 70 px of the cursor. She sets the CRT down (0.28 s),
    flies there as a cloud (1500 px/s, untouchable), and rides the target.
    While riding she can still cast but cannot be hit directly, and she
    takes half of every hit on her host: an ally is spared that half, an
    enemy still takes all of it. Recast, host death, or a cleanse sends
    her streaming back to the CRT, where she reforms and lifts it (0.45 s).
    If she dies riding, she falls beside the CRT. **Interpretation:** she
    counts as a swarm on her host, so the passive applies to her too.
  - **Not built:** the Miles clauses (Plague Bringer contaminating every
    debuff off a possessed target, Nanite Cloud's Stun), because Miles has
    no kit yet.
  - **Look:** a forge actor from her model on a joint fit Noah placed in
    the fit editor (`art/models/Nyx.fit.json`). The CRT is a rigid prop on
    two mounts (shoulder, hand); the carry and heave arm poses are solved
    numerically for that fit, and the drop's crouch keeps her feet on the
    floor. Clips: idle, run, swarm, plague_charge,
    plague, cloud, drop, lift, hit, death. A NyxCRT sheet shows the set on
    the floor where she left it. At runtime she frays into square pixels in
    her palette (coat magenta, screen cyan, skin orange) from the coat's
    edges and hem. Swarms are pixel clouds, magenta on a foe and cyan on a
    friend, joined to her by a thread of pixels. As a cloud, she draws no
    body. New VfxTheme `Nanite`.
- **Pillars:**
  - **Creative Builds** serves: the kit is all interactions. It spreads
    and stacks existing effects, combos with any Cleanse, Purge or
    Contaminate in other kits, and echoes infusion cores from a second
    origin. It plays like no other kit.
  - **Comradery in Competition** serves: Focus, Sharpened, Empowered and
    Stealth land on allies, and riding an ally soaks half their damage.
    **Trades against it:** she deals no damage, and score here comes from
    damage and heals. In an 8-minute ArenaSim (Nyx, Wade, Nolan, Isaac)
    she scored 45 against 728-1195. Her help is invisible on the board
    until support contribution scores. Needs a decision.
  - **Strategic Crunch** serves: the CRT is a real anchor. Riding far from
    it costs a long, untouchable trip home, and the swarm's leash forces a
    choice between staying near the fight and covering the host.
  - **Clarity** serves, with a risk: the swarm's colour says enemy or
    ally, the thread says whose it is, and the CRT on the floor says where
    she will reappear. The steady pixel dissolve adds screen noise. Watch
    it in a crowded fight.
- **Conflicts with the sheet (not changed, need a human):**
  - Dulled and Sharpened are on/off crit modifiers engine-wide (max 1
    stack), so "up to 5 stacks" of each cannot happen. Weakened does reach
    5. Raising their caps changes every character's crits.
  - Marked on an enemy and Silenced on an enemy do little today. Enemies
    have no abilities to silence, and per the effects spec Marked means
    "nearby enemies will attack this target", which is not built for
    enemies. The Dulled and Weakened from Silence do work.
- **Known art issue:** automatic (bone-heat) weighting fails on this mesh,
  so it falls back to distance weights and the long coat's hem follows
  her legs. The fix, if it shows in play, is a model without the long coat,
  with the coat added in code on springs (a new Meshy generation: credits).
- **Why:** built as the sheet says where it is unambiguous. The two
  interpretations above are the readings that keep the ally side helpful
  (Comradery). Prototype numbers. Covered by `tests/test_nyx_kit.cpp`
  (13 cases) and an 8-minute ArenaSim match with no asserts. **Not
  playtested by a human; the client effects are checked by build only.**
- **Status:** Built, needs review
- **Where:** `src/CharacterKits.cpp` (`NyxKit`), `src/ArenaData.cpp`,
  `include/ArenaData.h` (`NyxMode`), `src/AbilitySystem.cpp`
  (`shareDamage`, `onCoreCast`, `kitAnchored`), `include/NetworkProtocol.h`
  (`PlayerState` kit point, prop, mode; `VfxTheme::Nanite`),
  `src/ArenaView.cpp` (Nyx drawing), `tools/sprite_forge/actors/nyx.py`,
  `nyx_crt.py`, `docs/design/ART_DIRECTION.md`

## 2026-11-15: The scrap haul reads, and banking it pays off
- **Decision:** Salvaging a scrapyard finishes the objective and puts the
  scrap in your hands; you bank it by walking into a ring around the ship.
  - **Carrying:** a scrap crate bobs over the carrier's head (all players
    see it), and the carrier is Slowed until the scrap is banked. A
    "SCRAP SALVAGED" banner says to haul it home. You cannot salvage a
    second piece with your hands full, and a downed carrier drops it.
  - **Getting home:** while you carry anything, the drop-off ring around
    the ship (190 px) glows and pulses with ticks sweeping inward, and a
    chevron at your feet points to the ship.
  - **Delivering:** step inside the ring: no button. The scrap is lobbed
    onto the hull, which flares (burst and rings), a green "+180" repair
    number rises over the ship, and the "SCRAP DELIVERED" banner shows.
    Crashed probes deliver the same way.
  - **Bug fixed:** the old drop-off was a 70 px circle around the ship's
    centre, mostly inside the solid hull, and needed a button press there.
- **Pillars:**
  - **Clarity** decided it: requested by Noah after a playtest found no cue
    for hauling, for being in range or for success. Every step now has a
    cue that reads at a glance, for teammates too.
  - **Strategic Crunch** serves: the Slow makes the haul a real cost (a
    slowed player far out is exposed), and choosing who carries is a team
    decision.
  - **Comradery in Competition:** serves slightly; teammates can see who
    is carrying and escort them.
- **Why:** The haul is the scrapyard's whole gameplay, so it has to read.
  Scrap uses the match director's carry state (as scrap dropped by elites
  already did) instead of the objective's separate deposit step, so there
  is one way to carry scrap. Covered by `tests/test_points_of_interest.cpp`;
  **not playtested**.
- **Status:** Built, needs review
- **Where:** `src/MatchDirector.cpp` (`startCarryingScrap`, `bankScrap`),
  `src/ObjectiveSystem.cpp` (`deliverOnArrival`), `src/ServerGameState.cpp`,
  `src/TiledMap.cpp`, `src/ArenaView.cpp`, `src/ArenaHud.cpp`,
  `src/ArenaClient.cpp`

## 2026-11-15: Points of interest replace the horde; the zone holds until late
- **Decision:** A round is now about choosing where to go, not surviving
  a stream of enemies.
  - **No roaming spawns in a round.** Enemies live in camps at points of
    interest and stay dead once cleared, for the whole match. Raids on the
    ship (every 45 s) are the only other pressure. The boss fight keeps
    its top-up.
  - **21 points of interest in three rings** around the ship (about 900,
    1,550 and 2,350 px out), each on a pale clearing that stands out of the
    dark ground: scrapyards, med pods and outposts behind camps, crashed
    probes, frogs, toxic spores, and gates watched by a sleeping elite
    (Little John). The far ring has bigger camps with elites, and any
    objective 1,900 px or more from the ship pays two infusions. 44 camp
    guards in all.
  - **The map is bigger** (176 tiles, from 120) and the **zone is wider
    and holds longer:** start radius 2,000 / 2,500 / 2,900 by round (was
    1,150 / 1,500 / 1,850), shrinking only after 75% of the round (was
    35%).
  - **Discovery:** an objective's label and minimap marker only appear
    once the crew has seen it; before, every objective showed on the
    minimap from the start.
  - **Fixes this needed:** a camp's kills count for its objective even if
    it was cleared before anyone pressed interact; a Little John guardian
    actually waits until its gate is touched or it is hit (the flag was
    set but ignored); and a new match brings camps and objectives back.
- **Pillars:**
  - **Strategic Crunch** decided it. Requested by Noah after a playtest:
    the constant horde left no choices, just "do the nearest objective
    while fighting". Now each trip is a trade: near and cheap or far and
    rich, this camp or that one, keep exploring or go defend the ship from
    a raid, and when to turn back before the zone closes.
  - **Clarity** serves: fights happen where you chose to go, and clearings
    mark places worth visiting.
  - **Creative Builds & Dynamic Play:** neutral. Every objective type the
    code had is now on the map, but the layout is the same every match.
  - **Comradery in Competition:** neutral to serves; splitting up to cover
    more of the map is now a real option.
  - **Trades against Strategic Crunch:** walking between cleared places
    is dead time. Points of interest are about 600 to 900 px apart (3 to 4
    seconds of walking) to keep that short, but there are no patrols on
    the way, so the quiet stretches may feel empty.
- **Why:** Matches far fewer kills: about 90 for a 4-bot ArenaSim match,
  against about 560 before. The bots do not seek out objectives, so the
  simulation says little about pacing; **not playtested**. Also: the
  server now sends 44 more enemies all match (about 80 KB/s to four
  clients in the sim, from about 30), the layout is fixed rather than
  randomised per match, and clearings are ground colour only: no
  landmark props exist yet.
- **Status:** Built, needs review (open: patrols between points of
  interest, randomised layouts, landmark art, camp health scaling by
  round)
- **Where:** `tools/arena_map/generate_arena.py`, `assets/maps/arena.tmx`,
  `include/ArenaData.h` (zone tuning), `src/MatchDirector.cpp`,
  `src/EnemySystem.cpp`, `src/ObjectiveSystem.cpp`,
  `src/ServerGameState.cpp`, `src/TiledMap.cpp`, `src/ArenaView.cpp`,
  `src/ArenaHud.cpp`

## 2026-11-15: Only Venoxarid thugs spawn until the other genera have models
- **Decision:** Every pack, in every direction from the ship, is Venoxarid
  grunts and elites. Cerebrian, Frigidariant and Magnarok aliens no longer
  spawn in a match (the boss's brood is thugs too); they come back one
  genus at a time as each gets its real models. Their code, stats and old
  sheets are kept, and the playtest cheat keys can still call them up.
  - **Infusions still follow the map.** A kill drops the infusion of the
    territory it happened in (south Venoxarid, west Cerebrian, north
    Frigidariant, east Magnarok), not of the body.
  - **Drop rates re-fitted for all-thug packs:** 16% per grunt (a mixed
    pack was 7% / 24% / 75% by tier), an elite always drops, the pity drop
    comes every 4 kills instead of 6, and an elite has the brute's 50%
    chance of scrap.
- **Pillars:**
  - **Creative Builds & Dynamic Play** decided the infusion rule: with one
    genus on the field, tying drops to the body would leave only Venoxarid
    infusions and one build. Tying them to territory keeps all four.
  - **Trades against Creative Builds & Dynamic Play** all the same: every
    fight is now the same enemy with the same Wound on hit. Slow, Snare,
    Silence and Weakened from enemies are gone, and so are ranged shots
    and slams, so runs are more alike. This is temporary and was asked for.
  - **Trades against Clarity, slightly:** nothing on screen now says which
    territory you are in except the ground and the drop itself.
  - **Strategic Crunch:** neutral. Where to fight still decides what you
    can build.
- **Why:** Requested by Noah: pare back to just the Venoxarids rather than
  four genera of placeholder aliens, and bring the others back with their
  own models. Drop numbers keep a pack's expected infusions about where
  they were; **not playtested**. A full ArenaSim match and the unit tests
  pass.
- **Status:** Built, needs review
- **Where:** `fieldedGenus()` in `src/ArenaData.cpp`,
  `src/MatchDirector.cpp` (`spawnPack`, `dropLoot`), `src/EnemySystem.cpp`

## 2026-11-15: Venoxarid simplified to street thugs: a grunt and an elite
- **Decision:** The Venoxarid genus fields one enemy, the **grunt**, a
  hero-sized street thug (think a beat-'em-up's back-alley muscle), plus an
  **elite** enforcer one and a half times his size. Both are the team's
  generated models. The Toxlith swarmer, Venomire caster and Serpentide
  brute no longer spawn. The other three genera were unchanged by this entry (the entry above
  then took them off the field).
  - **Grunt:** 70 health, speed 150, radius 16, 9 damage, reach 48, a
    450 ms wind-up and 1.5 s between attacks, 25 points. (Toxlith was 28
    health, speed 195, radius 12, 5 damage, 350 ms, 10 points.) He still
    Wounds on hit.
  - **Elite:** 4x the grunt's health (280), 2x damage (18), 1.5x radius and
    sprite height, 0.85x speed, 1.3x reach. Double score and a guaranteed
    drop, as for any elite.
  - **Packs:** about two thirds as many bodies as before (minimum 2). At
    most one elite per pack, 5% per round for each member, never in a ship
    raid or the cheat crowd.
  - **One attack:** a big overhand right. The wind-up rears the fist far
    back and is held for the whole telegraph.
- **Pillars:**
  - **Clarity** decided the size and the attack: a body as big as a hero
    with one large, slow tell is easier to read than a carpet of
    knee-high biters, and the elite reads as "the dangerous one" by size
    and by the glowing tank on his back, not only by a ring.
  - **Strategic Crunch:** each kill is now a small decision (who do I
    commit a cooldown to) instead of mowing.
  - **Trades against Creative Builds & Dynamic Play:** Venoxarid fights
    lose the ranged caster and the slam brute, so they have less variety
    than the other genera, and area-damage builds have fewer bodies to
    hit. This was asked for ("simplify the enemies down"); the elite takes
    the brute's place as the pack's heavy, but nothing replaces the
    caster.
  - **Trades against Clarity, in crowds:** hero-sized enemies hide players
    more than small ones did. The per-player rim that ART_DIRECTION asks
    for is still not built.
- **Why:** Requested by Noah: fewer, bigger, more characterful enemies,
  with the real models in the game. Numbers were set so a round-1 pack has
  about the same total health and score as before (3 grunts at 70 against
  4 mixed aliens averaging about 50). They are **not playtested**.
  Simplifications to know about: the boss's Venoxarid brood and the map's
  camp guards are also grunts now; the old kind slots stay in the enum so
  network values do not move; the grunt and elite share one set of
  animations; both skeletons were placed by Noah in the joint editor; both models look slightly
  to their left in the A-pose, which every pose turns back (`HEAD_TURN`).
  **Checks:** unit tests and a full ArenaSim match pass. The silhouette
  check flags the grunt against Wade and Gravon (as black outlines a
  hero-sized humanoid looks like a hero; only colour separates them) and
  the grunt against the elite (same shape, told apart by size). Seen in
  forge previews and a size line-up only, **not in a running fight**.
- **Status:** Built, needs review (open: tuning after a playtest; whether
  Venoxarid wants a second ranged unit back)
- **Where:** `src/ArenaData.cpp`, `src/EnemySystem.cpp`,
  `src/MatchDirector.cpp`, `src/ArenaView.cpp`,
  `tools/sprite_forge/actors/_venox_thug.py`, `venoxarid_grunt.py`,
  `venoxarid_elite.py`, `art/models/VenoxaridGrunt.glb`,
  `art/models/VenoxaridElite.glb`

## 2026-11-15: Enable Nolan for in-game animation playtests
- **Decision:** Register Nolan as a playable Striker with 250 HP and 262
  movement speed. Add a clearly labeled prototype kit: Sweep (26 damage,
  600 ms cooldown), charged Lotus (35–90 damage, 4 s cooldown), Blackout
  (55 damage, 6 s cooldown), and Blink (up to 240 units, 26 nearby damage,
  5 s cooldown). Kicks hit on frame 2 at 24 fps. No passive yet.
- **Why:** The user requested making Nolan selectable to test the new art.
  His final combat design is not implemented; these temporary moves let
  every existing animation be exercised with the real combat system.
- **Pillars:** Clarity serves: slots play their matching kick clips and the
  charge pose plays while holding Lotus. Strategic Crunch and Creative
  Builds are provisional: damage and cooldowns are playtest values, not
  balance decisions. Comradery in Competition is neutral.
- **Status:** Built, needs review
- **Where:** `src/ArenaData.cpp`, `src/CharacterKits.cpp`, `src/ArenaView.cpp`,
  `include/ArenaData.h`, `tests/test_nolan_kit.cpp`.

## 2026-11-15: Nolan's kit is playable, and enemies can be knocked into the air
- **Decision:** Nolan's designed kit replaces the placeholder playtest kit.
  - **Airborne** is a new enemy state (`Enemy::airMs`,
    `AbilitySystem::knockUpEnemy`): it cannot act, its windup is cancelled,
    and only a knockback moves it. Brutes stay up 60% as long, the boss 25%.
    Clients draw it rising about 70 px above its shadow, which stays where
    it will land.
  - **Passive:** knock-ups and knockbacks last 35% longer per Expose stack
    on the target, read before the hit consumes one.
  - **LMB, Lower Limb Devastation** (650 ms): an 80-degree half-angle sweep,
    120 px reach, 16 damage, 650 ms knock-up, +1 Expose, then an 80 px hop
    back.
  - **RMB, Sky Splitter** (hold up to 1 s, 5 s cooldown): no damage;
    knockback of 90 to 260 px with charge, all around him within 150 px,
    +1 Expose.
  - **Q, Black-out Kick** (8 s): only on an airborne foe within 230 px. He
    steps in, kicks it down for 48 damage and Stuns it and everything within
    140 px for 1.4 s. Every living Wade on the team gains 1 Resonance for 8 s
    (his Roll already spends it).
  - **Shift, Anime Shit** (4 s): only on an airborne foe within 520 px;
    teleports to where it will land and gives it +1 Expose.
  - A kit can now refuse a press with nothing to act on (`Kit::canCast`):
    the cooldown is not spent, and Nolan shows his reach as a ring.
  - The HUD button shows an ability's short name when the full one does not
    fit ("Lower Limb", "Sky Splitter", "Black-out Kick"); the passive line is
    cut with an ellipsis; the tooltip and character select show the names in
    full.
- **Pillars:**
  - Strategic Crunch serves: Q and Shift work only while a foe is in the air
    (about 0.65 s without Expose), so the combo is a timing decision.
  - Comradery serves: every Expose he stacks is +15% damage on a teammate's
    next hit, and Black-out Kick charges Wade's Roll. It also trades against
    Comradery in one place: the full-charge Lotus can throw enemies 260 px
    out of a melee teammate's reach.
  - Creative Builds serves: airborne is a state any later kit or infusion
    can create or read.
  - Clarity serves: the foe rises off its shadow, Stun shows, and a refused
    press shows why. It is weakened slightly by the long names on the HUD,
    and by bars and numbers piling up when several foes are in the air.
- **Why:** The kit text is the design lead's. Interpretations that are mine,
  to check:
  - "If Wade is an ally" is read as any living Wade in the match, at any
    distance.
  - "End location" is the end of the foe's knockback, and he arrives just
    short of its body.
  - Q also steps him in beside the target, since the kit does not say how a
    kick reaches it.
  - Only enemies can be airborne, so Shift cannot follow an ally.
  - RMB and Shift deal no damage, because the kit text gives them none.
    That is also what lets Expose build: his own damage consumes a stack as
    it adds one.
  - The title "the Prodigy" is mine.
  - Balance from `ArenaSim` (3 seeds each): in a mixed team Nolan's kills are
    51 to 101 against Wade's 33 to 118. An all-Nolan team lasts about as
    long as an all-Wade team (120 to 151 s against 79 to 138 s). The bots
    press Q and Shift blindly, so most presses were refused.
  - Checked in the running game: selectable, the kit text on character
    select, sweep, knock-up above shadows, the Stun from Black-out Kick, the
    Lotus ring. The HUD fixes were built after that run and are not yet seen
    in game.
- **Status:** Built, needs review
- **Where:** `src/CharacterKits.cpp` (NolanKit), `src/ArenaData.cpp`,
  `include/Enemy.h`, `src/EnemySystem.cpp`, `src/AbilitySystem.cpp`,
  `include/AbilitySystem.h`, `include/NetworkProtocol.h`, `src/ArenaClient.cpp`,
  `src/ArenaView.cpp`, `src/ArenaHud.cpp`, `src/UISystem.cpp`,
  `tests/test_nolan_kit.cpp`, `tests/arena_sim.cpp`

## 2026-11-15: Nolan's body and a kick-only starting moveset
- **Decision:** Nolan's body is the fitted `art/models/Nolan.glb` (Meshy,
  shrunk locally to 40,000 triangles), 1.9 units to the top of his hair, on
  joints a human placed. His motion is themed on firebending as Zuko does
  it: a low, wide, bladed stance (lead palm out, rear hand chambered), a
  hand planted on the floor, and the legs drawing big arcs. Every attack is
  a kick. Starting clips, named for his kit: `sweep` (LMB: floor sweep, then
  a hop back), `lotus_charge` and `lotus` (RMB hold and release: a coil,
  then a full airborne turn with the leg out flat), `blackout` (Q: an axe
  kick down through an airborne target), `blink` (Shift: fold up, arrive
  mid-air knee first), plus `idle`, `run` (chest forward, arms trailing),
  `hit` and `death`. Kicks land on frame 2, as Wade's punches do.
- **Pillars:** Clarity: his low stance, flat-out leg and blue hair give him
  an outline no one else has, and each ability has its own shape (floor
  arc, flat spin, vertical leg). Creative Builds: he is the hype foil to
  Wade's plain boxing, so the two do not move alike. Strategic Crunch and
  Comradery in Competition are neutral: no kit rule or timing is built yet.
- **Why:** The first pass put Wade's boxing clips on Nolan's body as a
  placeholder; the design lead rejected that (he is kick oriented, anime,
  Zuko-like), so they were replaced. `hit` and `death` still reuse Wade's
  poses. These are starting poses for the pose editor, not final
  animation: the design lead opened the pose editor and chose to bake
  them unchanged. Baked to `assets/sprites/actors/Nolan.png` (9 clips, 8
  directions, 4576 x 4524, a little over the 4096 guide). Checked in
  previews and the silhouette lineup only: Nolan is not flagged against
  anyone (closest is Wade, shape 0.72). **Clarity is weakened in one
  place:** at game size his face does not read from the front, because the
  hair crest covers it; he is recognised by hair, bare chest and white
  trousers. The model's hands are open, so he never shows a fist. Nolan's
  kit is not in the game, so the clip names and the frame-2 hit timing are
  not checked against code, and he has not been seen in a fight.
- **Status:** Built, needs review
- **Where:** `art/models/Nolan.glb`, `art/models/Nolan.fit.json`,
  `tools/sprite_forge/actors/nolan.py`, `assets/sprites/actors/Nolan.png`,
  `assets/sprites/actors/manifest.txt`, `docs/design/ART_DIRECTION.md`

## 2026-11-15: Ship Wade's generated body
- **Decision:** Make the fitted `art/models/Wade.glb` actor the default for
  `build.py wade`, and bake it into the sprite sheet the game already loads.
  Keep the original boxing controls in `_wade_boxing.py` for the mesh adapter.
- **Pillars:** Clarity serves: Wade uses the textured character model and
  fitted guard already prepared for him. Strategic Crunch, Comradery in
  Competition, and Creative Builds are neutral: kit rules and timing stay the same.
- **Why:** The generated body was only available as an excluded trial actor;
  compiling the client continued to show the older procedural sprite sheet.
  The user requested promoting the model to the in-game actor.
- **Validation:** Baked all 15 clips in eight directions; inspected sampled
  front, side, and back frames. The silhouette check flags six existing
  pairs, none involving the new Wade. The native client loaded the new sheet;
  its scripted match selected Gravon, so Wade combat was checked in previews.
  Before/after: `docs/prototype/wade_mesh_before_after.png`.
- **Status:** Built, needs review
- **Where:** `tools/sprite_forge/actors/wade.py`, `_wade_mesh.py`,
  `_wade_boxing.py`, `assets/sprites/actors/Wade.png`.

## 2026-11-13: Wade's kit rebuilt on real boxing
- **Decision:** (requested by a human, with wireframe references for each
  move; the numbers and the points marked *agent's call* are mine.)
  - **Simple Punches (LMB):** the server picks the punch. He throws
    three-punch combinations chosen at random from ten (1-2-3, 1-1-2,
    1-2-3b, 1-2b-3, 1-3-2, 1-4-3, 2-3-2, 2-3-4, 3-2-3, 3b-3-2; b = to the
    body), never the same one twice running. Each of the six punches (jab,
    cross, lead hook, rear hook, cross to the body, lead hook to the body)
    has its own animation. Damage, range and cooldown are unchanged and
    identical for all six; the third punch still grants +1 Haste and
    resets Roll. *Agent's call:* the punches differ only in picture.
  - **Uppercuts (hold RMB):** alternating 5s and 6s, lead hand first, as a
    loop that stays low between punches. He is now planted: move speed is
    0 while the channel runs (was 55%).
  - **Slip (Q):** he slips to the rear side with the rear hand chambered
    and holds it for the 0.5 s window. Struck in the window, he throws a
    haymaker at the attacker. It now takes 210 ms to land (was instant);
    he is locked in place and cannot be hit until it does. Damage, range
    and the cooldown refresh are unchanged.
  - **Roll (Shift):** a boxing roll (slip, duck, under, up the other side)
    instead of a tumble. New, from the kit doc: with Resonance, Roll spends
    it to travel 430 instead of 190.
- **Pillars:**
  - Clarity, the pillar that decided most of this: you can read which punch
    was thrown; the chamber shows the Slip window is open; the haymaker's
    damage lands on the frame the fist does.
  - Crunch: planted Uppercuts and the haymaker's lock are commitments.
  - Creative Builds: combinations vary run to run; Roll gains a Resonance
    hook.
  - Comradery: neutral.
- **Why:**
  - The old kit alternated jab/cross, had no hooks or body shots, a duck
    for Slip and a forward tumble for Roll, and did not match the kit doc
    in three places (not planted, no Resonance roll, counter with no
    visible punch).
  - Combinations are fixed threes because the Haste trigger is every third
    punch: the finisher of each combination is the punch that pays out.
  - Rejected: per-punch damage or reach (head vs body, straight vs hook).
    The kit doc gives them one effect, and six sets of numbers would hide
    a dominant punch the player cannot choose anyway.
  - Rejected: an instant haymaker. A punch with no travel cannot be seen.
- **Known gaps:**
  - Nothing in the game gives Wade Resonance yet (only El Presidente gains
    it, from his own grab), so the long Roll cannot happen in play. It is
    covered by a test only.
  - Bots: over 6 seeds with Wade + Suds + Gravon + Isaac the team got as
    far as before (rounds 2 to 3), but the Wade bot scored lower in most.
    The bots hold Uppercuts while being hit and spam Slip; whether planted
    Uppercuts are too punishing for people needs a playtest.
  - Not seen in the running game: the client could not be built on the
    machine this was made on. Server logic is covered by
    `tests/test_wade_kit.cpp`; the art was checked in forge previews and
    on the baked sheet.
  - The sheet grew to 5984 x 5772 (15 clips), over the 4096 guideline but
    under the forge's 8192 limit and smaller than Gravon's.

  ![Wade's boxing clips](../prototype/wade_boxing_clips.png)
- **Status:** Built, needs review.
- **Where:** `src/CharacterKits.cpp` (WadeKit), `include/ArenaData.h`
  (`Arena::WadeForm`), `src/AbilitySystem.cpp` (cast events carry the
  form), `src/ArenaView.cpp` (`actionClip`), `tools/sprite_forge/actors/wade.py`,
  `tools/sprite_forge/actors/_rig.py`, `tests/test_wade_kit.cpp`

## 2026-11-13: Fog of war with shared crew sight
- **Decision:** The crew shares sight. Every living hero sees 640 world
  pixels around themselves, a downed hero 260 (enough to see the revive),
  the ship 520. Ground is in one of three states: seen now (clear, and
  lifted by a faint cool light), explored (dimmed; enemies, shots and
  their light are hidden) or unexplored (solid fog). Hidden with the fog:
  enemies and their health bars, enemy shots, damage numbers, enemy dots
  on the minimap. Always shown: teammates, the ship, objectives, the zone
  ring, and pickups anywhere the crew has been. An attack or telegraph is
  shown as soon as any part of it reaches into sight. F3 turns the fog off
  for comparison. A new match starts unexplored.
- **Pillars:** Crunch (what you know is now scarce: scouting and where you
  stand are decisions). Comradery (a teammate's sight is yours, so
  spreading out helps everyone: "when I win, we win"). Clarity is traded
  against on purpose, since fog hides state; the cost is kept small by
  never hiding objectives, the ship, teammates or an attack that can reach
  you.
- **Why:** Requested by a human together with the unlocked camera: a free
  camera with no fog would show the whole map for free. Choices made here
  that were **not** specified and need review:
  - Sight is a plain circle. **Walls do not block it yet**; the arena map
    has one collision shape, so it would change little today.
  - The fog is drawn by the client. The server still sends every enemy, so
    this is presentation, not an anti-cheat boundary (fine for co-op PvE).
  - The minimap hides enemies out of sight but does not draw the explored
    area yet.
  - Before this, the minimap showed every enemy in range. It no longer
    does.
  - All radii are first guesses (`include/config/CameraConfig.h`).
- **Status:** Built, needs review (the feature was requested by a human;
  the rules and numbers above were not)
- **Where:** `include/FogOfWar.h`, `src/FogOfWar.cpp`, `src/ArenaView.cpp`,
  `src/ArenaHud.cpp`, `include/config/CameraConfig.h`,
  `docs/prototype/camera_fog_of_war.png`

## 2026-11-13: Locked and unlocked camera
- **Decision:** Y switches the camera between locked (centred on your hero,
  the default) and unlocked (it stays where you leave it; the cursor within
  28px of a screen edge pans it at up to 1600 world pixels a second, and it
  cannot leave the map). While unlocked, holding Space brings the camera
  back to your hero. A white "YOU" arrow at the screen edge points to your
  hero whenever they are off screen, and the HUD says which mode you are
  in. Aiming follows the camera, so the cursor always aims where it points.
- **Pillars:** Crunch (looking away from your hero to scout is a choice
  with a cost). Clarity (the arrow and the mode label mean you never lose
  your hero or wonder why the view stopped following).
- **Why:** Requested by a human, and it brings the game in line with the
  Controls/HUD doc (unlocked, edge-panning camera, Space to re-centre),
  which the 2026-11-11 entry below departed from. One conflict to flag:
  **Space now means two things.** Locked, it is still the basic attack
  (kept for trackpads); unlocked, it re-centres and does not attack, as the
  doc says. A key that changes meaning with a mode costs some Clarity; the
  alternative (a different re-centre key) would go against the doc. The
  mode is not saved between sessions, and there is no minimap click or
  drag panning yet.
- **Status:** Built, needs review (requested by a human; the Space rule and
  the pan numbers were not specified)
- **Where:** `include/Camera.h`, `src/InputSystem.cpp`,
  `src/ClientPrediction.cpp`, `src/ArenaHud.cpp`

## 2026-11-13: The camera is pulled back (world zoom 1.5 to 0.75)
- **Decision:** The world is drawn at 0.75x instead of 1.5x. About 1700x960
  world pixels are in view (four times the area), and a hero is about 8% of
  the screen height instead of 15%, matched to the reference frame a human
  supplied.
- **Pillars:** Crunch and Clarity of the game state (you can read the room,
  see a pack coming and plan a route, so less of a run is spent walking
  blind). It trades against Clarity of the characters: sprites are half the
  size on screen and are scaled down from their authored size, so fine
  detail and small effects read less well. Health bars and text are drawn
  at a fixed size and are unaffected.
- **Why:** Requested by a human with a reference image. The window is a
  fixed 1280x720; if it becomes resizable, zoom should scale with height so
  the field of view stays the same. Art authored for the old zoom
  (silhouettes, effect sizes, the 300px hero light pool, the floor
  vignette) has not been re-tuned for the new one.
- **Status:** Accepted (requested by a human); the exact value needs review
- **Where:** `include/config/ScreenConfig.h`

## 2026-11-12: The look: inked heroes on a dark world
- **Decision:** The in-game look is set as three layers:
  - **World:** dark, cool, low contrast (floor values in the bottom fifth
    of the range); genus colour arrives as light and glow, not bright
    tiles.
  - **Characters:** the brightest, most saturated things on screen, with
    solid ink shadows, a thick contour and a signature rim light.
  - **Effects:** additive light with bloom that lights the ground.
- **Built 2026-11-13 (the world and effects layers, in the engine):**
  - **World values:** the floor shader pushes every tileset dark and cool
    (texel value 0.045 to 0.245, slate tint), with a floor-only vignette.
    80% of the tile's hue is kept, not the mock-up's near-grey, so the
    map's genus regions still read on the floor (see Why).
  - **Grounding:** a soft contact shadow and a sheared silhouette shadow
    under every player and enemy, from the forge's `KEY_LIGHT`. All
    shadows are drawn under all bodies.
  - **Light pools:** a warm pool around each player; a faint genus-colour
    pool under each enemy that turns danger red as it winds up a hit;
    hits, bursts, swings, beams, projectiles, zones, telegraphs and
    infusion pickups light the floor. Pools add up in a quarter-size
    light map and roll off, so a swarm cannot white out the ground.
  - **Bloom and grade:** the world is drawn offscreen; only what is drawn
    as light (effects, telegraphs, auras) blooms, sprites do not. A mild
    grade and a weak whole-scene vignette (0.12) follow. Health bars,
    names, numbers and the HUD are drawn after it, untouched.
  - **F4** switches the look off and on in game, to compare.
  - Every strength is a named constant in `include/config/LookConfig.h`.
- **Pillars:** Clarity decided it: value separates floor, fighters and
  danger at a glance. It trades nothing against the other pillars. One
  risk for Clarity: a dark floor can hide dark enemies, so enemies need
  the same rim and ink treatment as heroes. Choices made while building,
  all decided by Clarity:
  - Floor hue kept at 80% (above), against the art direction's "never
    bright floor tiles" taken to its limit: the regions are gameplay
    information.
  - Bloom is weighted by coverage squared and telegraph pools were
    halved: at first a filled telegraph circle bloomed into an opaque
    blob over the fight. Readability beats spectacle.
  - The strong vignette is on the floor only, so enemies entering from
    the screen edge are not dimmed.
  - Shadows fall up and to the right, as `KEY_LIGHT` says. The mock-up
    flipped them toward the viewer; that would disagree with the light
    baked into the sprites.
- **Why:** The splash art promises comic inking on black with neon
  accents; the game showed smooth clay figures on a flat brown floor. A
  mock-up with the game's own sprites showed the presentation layer
  alone closes much of the gap (`docs/prototype/styleframe_target.png`).
  Real frames, look off and on, beside the mock-up:
  `docs/prototype/look_before_after.png`.
- **Not achieved / open:**
  - No rim light or outline on enemies. In the frames looked at (Toxlith
    and the old recoloured enemy sheets, on all four floor regions) they
    stayed readable thanks to their pools, so it was left out; a dark
    enemy on a dark floor is still the risk to watch. A rim needs a
    sprite shader (ImGui can only multiply a sprite's colour).
  - The boss (a dark purple body) was seen once, at the screen edge, and
    read as a dark mass on the dark floor. Its pool was then raised
    (`BOSS_POOL_STRENGTH`), but the boss was not seen on screen again
    after that: **unverified**, and the first thing to check.
  - The floor has no seams, cracks or leaking genus glow: the tileset is
    flat colour, and that detail is art, not code.
  - Effects are still alpha-blended shapes that bloom, not true additive
    light; hit flashes still tint red, not white.
  - The Blue Zone ring and the white ring under the local player are
    fainter against the graded world than before; not retuned.
  - Effect "verb" moments (detonate, spread) do not cast pools yet.
  - Looked at only in the WASM client (Edge, one map, Wade, about 20
    enemies). Not measured for frame time; not seen on the native client.
- **Status:** Built, needs review.
- **Where:** `docs/design/ART_DIRECTION.md` ("The look"),
  `tools/sprite_forge/styleframe.py`, `include/config/LookConfig.h`,
  `src/PresentationLayer.cpp`, `src/TileRenderer.cpp`,
  `src/ArenaView.cpp`, `src/RenderSystem.cpp`, `src/UISystem.cpp`

## 2026-11-12: Character bodies come from generated models; ink style
- **Decision:**
  - **Bodies:** a character's body is an image-to-3D model made from its
    concept art (`art/models/<Name>.glb`). The forge puts it on the
    existing skeleton, so clips, eight directions and carry sockets are
    unchanged. Code-built parts remain for props, springy cloth and
    simple creatures.
  - **Shading:** a new `ink` style: lit tone, cool shadow tone, solid ink
    in the shadow cores.
  - **This reverses** the earlier ruling against image-to-3D tools
    ("Heroic body kit" entry below). A human reopened and decided it.
- **Pillars:** Clarity: a character that matches its splash art is
  recognised at once, and ink blacks hold shapes together at 100 px.
  Creative Builds is helped indirectly: a 30-character cast is only
  reachable if a body costs a generation and a fit, not days of code
  modelling.
- **Why:** After many passes the code-built bodies still read as clay
  figures; code cannot draw a face or cloth. At game size the AI-drawn
  Wade turnaround already matches the splash art
  (`docs/prototype/styleframe_closeup.png`). Rejected: 2D frames from the
  image pipeline (every move in eight directions must stay consistent,
  which has not been tenable) and more shading on code-built bodies (the
  mock-up shows its ceiling).
- **Not yet shown:** the route is proven end to end only with a stand-in
  model exported from the code-built Wade. **No real generated model has
  been baked**, so the quality claim is untested. No shipped sprite
  changed; the five existing actors are still code-built.
- **Status:** Accepted (route chosen by a human); the pipeline is Built,
  needs review
- **Where:** `tools/sprite_forge/skin.py`, `fit.py`, `selftest_skin.py`,
  `forge.py` (`shade_ink`, albedo pass), `actors/_mesh_template.py`,
  `art/models/README.md`, `.claude/skills/character-art/SKILL.md`

## 2026-11-12: Wade's boxer guard and likeness restored
- **Decision:**
  - **Guard:** a boxer's guard, not Ken's stance. Chin tucked behind high,
    tight fists, elbows in, shoulders hunched, slight lean.
  - **Likeness:** a tight short white crop instead of spiked hair, a full
    dark beard and hard brows, bold tiger stripes on the left arm (on the
    skin, not floating), and forearm hand-wraps, a brown belt and bandaged
    lower legs, all from the splash art.
- **Pillars:** Clarity: a recognisable character at a glance. It is also
  the character's identity.
- **Why:** Human feedback: the stance was too stiff and upright for a
  boxer, and the likeness had drifted.
- **Status:** Accepted (requested by a human); the exact pose values are
  Built, needs review
- **Where:** `tools/sprite_forge/actors/wade.py`

## 2026-11-12: Proportions retuned to 3rd Strike, not DBZ
- **Decision:** Wade is retuned from bodybuilder to martial-artist
  proportions, using a 3rd Strike Ken reference from the human:
  - longer legs and arms,
  - squared capped shoulders (a new `square` clavicle bar in the body
    kit) over a real waist,
  - smaller deltoids and pecs, normal-sized fists,
  - an upright wide-stance guard with the fists in front of the chest.
- **Pillars:** Clarity: a familiar fighting-game silhouette. It is also
  the target look.
- **Why:** The first body-kit pass read as "muscly DBZ".
- **Status:** Accepted (requested by a human); the exact values are
  Built, needs review
- **Where:** `tools/sprite_forge/actors/_body.py`, `tools/sprite_forge/actors/wade.py`

## 2026-11-12: Heroic body kit and fighting-game motion; Wade rebuilt
- **Decision:** Characters build on a shared heroic body kit: a spine
  chain, curved muscle and proportion parameters. The forge gains:
  - overlapping action,
  - spring-driven secondary motion,
  - squash and stretch.

  Wade is rebuilt as the test, in the toon style: a broad, heavy-handed
  veteran with oversized wrapped mitts. His idle is a boxer's bounce. His
  punches have anticipation, a stretched smear, a held impact and a
  recoil. His hair and sash swing on springs.
- **Pillars:** Clarity: strikes read as strikes (the stretch and hold show
  the hit). It also gives the cast its feel.
- **Why:** Human feedback: proportions and rigid movement were what kept
  the characters from feeling like 3rd Strike / MvC2. The human ruled
  out image-to-3D tools.
- **Status:** Accepted (requested by a human); Wade's specific look and
  timing are Built, needs review
- **Where:** `tools/sprite_forge/actors/_body.py`, `tools/sprite_forge/actors/wade.py`,
  `tools/sprite_forge/forge.py` (`clip_frames`)

## 2026-11-12: Art style moves from pixel art to toon-shaded 3D
- **Decision:**
  - **Pixel art is dropped.** The forge gains a toon render: anti-aliased
    cel shading, ink outlines and creases, and a signature rim light.
  - **Organic shapes:** a `tube` primitive gives curved, tapering forms.
  - **Gravon rebuilt as the test case:** heroic proportions, smooth
    muscle, flared torn trousers, chunky hair, and the blade on his
    spine.
  - **Inspirations:** 3rd Strike / MvC2, Hades, Risk of Rain 2,
    Battlerite.
- **Pillars:** Clarity serves: bolder shapes, outlines and rim light. It
  also gives the game a visual identity.
- **Why:** Human direction. Blocky primitives at about 60 px could not
  give 30+ characters distinct, appealing shapes.
- **Open question (needs a human):** pre-rendered sprites or real-time
  3D?
  - At this resolution, Gravon's sheet is about 200 MB of GPU memory,
    with eight directions and 63 frames. That doesn't scale to four heroes
    in a match plus enemies, or to the WASM build.
  - Options:
    (a) keep pre-rendering, but mirror to five directions, trim frames and
    compress textures;
    (b) render these same jointed models in real time in Gambit with a
    toon shader. That is the Battlerite / Risk of Rain 2 approach: tiny
    memory, smooth turning at any angle, and dynamic light. It is engine
    work.
- **Status:** Proposed
- **Where:** `tools/sprite_forge/forge.py`, `tools/sprite_forge/actors/gravon.py`,
  `src/Texture.cpp`, `src/ArenaView.cpp`

## 2026-11-12: Poses carry personality; Gravon's silhouette is a sword
- **Decision:**
  - **Gravon:** the sword runs down his spine at rest (pommel above the
    head, crossguard at the shoulders), so his silhouette reads as a sword.
    The host slumps underneath. He draws the sword only to attack, using
    the new per-joint show/hide in the forge.
  - **Suds:** idles with a toast.
  - **Wade:** a compact, unbothered guard instead of a lunge.
  - **Presidente:** chest out and chin up, on sturdier legs.
- **Pillars:** Clarity: each character is still distinct, and now also
  in character. Presidente and Suds side-on sit exactly at the
  look-alike line (0.72): two heavy bruisers from the side are both thick
  columns. Front, back and three-quarter views are clearly different.
- **Why:** Human feedback: the silhouettes were distinct but did not show
  personality, and the postures looked odd. The sword silhouette was the
  human's idea, and it fits the Soulblade passive in Character Concepts.
- **Status:** Accepted (requested by a human); the specific poses are
  Built, needs review
- **Where:** `tools/sprite_forge/actors/`, `tools/sprite_forge/forge.py`

## 2026-11-12: Suds and Gravon get forge art; Toxlith restyled
- **Decision:**
  - **Suds** is "the barrel and the cooler": a cooler at his hip and a can
    raised high. His abilities play toss, belch, rounds and chug.
  - **Gravon** is "the blade": a sword longer than he is tall, resting
    across his back. His abilities play alternating slashes, the Ravager
    overhead smash, Fury and Rush.
  - **Toxlith** follows the reference image: a warty acid-green toad-lizard
    with a magenta spine crest, venom beads and a hanging tongue.
- **Pillars:** Clarity serves. The silhouette check flags no pair among
  the forge cast; Presidente and Suds side-on are the closest at 0.66.
  Creative Builds: the characters are visually distinct, and each move has
  its own animation.
- **Why:** Requested by a human. Suds is the MVP's lead character, and
  Gravon's blade is the cast's strongest silhouette hook.
- **Status:** Accepted (requested by a human); the specific looks are
  Built, needs review
- **Where:** `tools/sprite_forge/actors/`, `src/ArenaView.cpp` (`actionClip`)

## 2026-11-12: Silhouette-first art direction; Presidente and Wade reshaped
- **Decision:** Every in-game asset must pass the "who's that?" silhouette
  test from the art on-ramp. `tools/sprite_forge/silhouette.py` checks it.
  - Presidente is now an inverted triangle: boulder shoulders, small legs
    planted wide, a tall striped crest, and a fists-on-hips idle.
  - Wade is a lean lunge: a long bladed stance, oversized white wrapped
    mitts by his face, and swept-back hair.
  - Both render with chunkier pixels (`PIXEL_SCALE` 1.35), at the same size
    on screen, putting Wade near the 64 px target in DD-AA-007.
- **Pillars:** Clarity serves: the heroes now read as different shapes.
  It does not fix finding yourself in a swarm, because enemy bodies cover
  any outline; that needs a player rim or outline (not built).
- **Why:** The forge actors had shared one skeleton and posture, and their
  identity lived inside the outline. A cape flare was tried on Presidente
  and removed because it read as debris. The chunkier pixels look almost
  the same as standard pixels at play size, so they were kept for
  consistency with DD-AA-007.
- **Status:** Accepted (requested by a human); the specific shapes are
  Built, needs review
- **Where:** `tools/sprite_forge/actors/`, `tools/sprite_forge/silhouette.py`,
  `docs/design/ART_DIRECTION.md`

## 2026-11-12: Effect visual language and effect verbs
- **Decision:** Every effect gets its own colour, motion and icon. Stacks
  drive the size of its aura, its particles and the body tint. Six verbs
  have their own moments: apply, amplify, spread, detonate, corrupt
  (buff → debuff) and purify (debuff → buff). Ordinary enemies show
  small pips instead of icons; damage-over-time numbers are smaller and
  dimmer. There is an effect lab on F12 for trying it all.
- **Pillars:** Clarity serves: effects were text tags before. Creative
  Builds serves: this is the substrate for spread, detonate and corrupt
  builds. Clarity trades a little at scale: a big poisoned crowd makes a
  large green blob of auras. That reads as "the crowd is poisoned", but
  watch it in playtests.
- **Why:** Requested as a slice to judge before designing build pools. See
  `docs/design/EFFECT_VISUALS.md`.
- **Status:** Accepted (requested by a human); the specific look is Built,
  needs review
- **Where:** `src/EffectVisuals.cpp`, `src/AbilitySystem.cpp`, `src/MatchDirector.cpp`

## 2026-11-12: Poison is its own effect, separate from Wound
- **Decision:** Venoxarid infusions (Toxic Miasma, Toxic Blade, Acidic
  Shell, Speed Siphon, Venom Infusion) now apply Poison instead of Wound.
  Poison does 5 damage per tick per stack like Wound, but caps at 30
  stacks instead of 10. Cauterize consumes both. Mend purifies Poison.
- **Pillars:** Clarity: poison and bleeding look different. Creative
  Builds: a higher cap leaves room for amplify and spread builds.
- **Why:** Weaker per-stack values were tried. At 2 per stack the
  Suds/Gravon/Isaac/Zephyr bots won 1 of 3, and at 4 per stack 4 of 10.
  At 5 per stack they win 7 of 10, whether the cap is 10 or 30, the same
  as before the change. Mina/Volgore/Wade/Isaac still lose all 10, as
  before. Magnarok burn still uses Wound and should be split the same way.
- **Status:** Built, needs review
- **Where:** `include/config/EffectConfig.h`, `src/Infusions.cpp`

## 2026-11-11: El Presidente's leaps go higher and farther; Bomba is a sit-out powerbomb
- **Decision:** Leap height up about 60%. Bomba range is 170 to 420 by
  charge (was 120 to 320), Splash range 460 (was 360). Air time is 620ms
  for Bomba and 600ms for Splash, and he sits for 260ms after Bomba.
- **Pillars:** Clarity (a big, readable wind-up and landing) and Creative
  Builds (a distinct mobility-and-grapple playstyle). Crunch takes a small
  cost: the longer air time and the seated recovery are a deliberate
  commitment window.
- **Why:** Requested by a human. The commitment window keeps the extra
  range from being free.
- **Status:** Accepted (requested by a human)
- **Where:** `src/CharacterKits.cpp`, `tools/sprite_forge/actors/presidente.py`, FWFsoft/gambit#9

## 2026-11-11: Grappled foes are pinned to the carrier's hands
- **Decision:** The sprite tool exports a per-frame hand position, and the
  held foe is drawn there, in front of or behind the carrier.
- **Pillars:** Clarity: you can always see who is held and where they
  will land.
- **Status:** Built, needs review
- **Where:** `tools/sprite_forge/forge.py`, `src/ArenaView.cpp`

## 2026-11-11: El Presidente's kit (grappler Playmaker)
- **Decision:** 330 hp, speed 238. While holding a foe (up to 3.8s) he
  moves 15% slower and every button switches to a finisher. The boss cannot
  be grabbed. Abrazo (bear hug) has its own 7s cooldown.
- **Pillars:** Creative Builds (a hold-state kit unlike any other), Crunch
  (the hold timer forces you to choose a finisher quickly), Comradery
  (a held foe is helpless, which sets up teammates).
- **Why:** Built from his splash art and the Character Concepts doc. All
  numbers are first guesses and have not been through the bot simulator.
- **Status:** Built, needs review
- **Where:** `src/CharacterKits.cpp`, `src/ArenaData.cpp`

## 2026-11-11: Prototype match is co-op win/lose, not last player standing
- **Decision:** The crew wins by killing the boss and loses if everyone is
  down at once or the ship is destroyed. Score is per player, and the top
  scorer each round gets a bonus infusion.
- **Pillars:** Comradery in Competition is only half served. The GDD and
  CORE GAMEPLAY list "one player is left alive" as an end condition; that
  is **not built**, so the competitive tension the pillar asks for is
  weak.
- **Why:** The co-op loop came first so the core systems could be tested.
- **Status:** Built, needs review (open question: add a last-standing
  ending?)
- **Where:** `src/MatchDirector.cpp`

## 2026-11-11: Space is basic attack; the camera is locked to the player
- **Decision:** Space and LMB both basic attack. The camera follows the
  player.
- **Pillars:** Clarity (you never lose your character). Crunch is
  slightly weaker, because you cannot scout with the camera.
- **Why:** Simpler for the first prototype. This **differs from the
  Controls/HUD doc**, which has an unlocked, edge-panning camera and Space
  to re-centre it.
- **Status:** Superseded in part by "Locked and unlocked camera"
  (2026-11-13): the camera can now be unlocked, and Space re-centres it
  while it is.
- **Where:** `src/InputSystem.cpp`, `src/RenderSystem.cpp`

## 2026-11-11: Infusions give genus effects only, with no slot talents
- **Superseded** on 2026-11-17 by "Infusions are items; the suit is a core
  plus four tiered trees".
- **Decision:** Each infusion applies its genus effect in the tree you put
  it in (Offense, Defense, Mobility or Genealogy). More infusions of the
  same genus level it up.
- **Pillars:** Creative Builds is only partly served. The Genera and
  Infusion doc decided on "BOTH" (a slot talent plus the genus effect) and
  per-item stat lines; neither is built.
- **Status:** Built, needs review
- **Where:** `src/Infusions.cpp`

## 2026-11-11: The input latch keeps quick taps
- **Decision:** A press is remembered until the server reads it, even if
  the press and release arrive in the same tick.
- **Pillars:** Clarity: every press you make does something.
- **Status:** Accepted (a bug fix)
- **Where:** `src/AbilitySystem.cpp`
