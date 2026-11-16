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
