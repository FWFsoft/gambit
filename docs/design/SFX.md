# Sound effects

Placeholder sound for hits and for every ability, so every hit and every
cast is heard (Pillar 4, Clarity: "every hit needs feedback: animation,
flash, number and, eventually, sound"). The sounds are free CC0 clips from
Kenney's packs (`assets/sfx/CREDITS.md`) and should be replaced by the
game's own sound design later.

Code: `SfxSystem` (`src/SfxSystem.cpp`) plays with SDL_mixer beside the
music. What plays is decided by two pure, tested rule sets:
`HitSounds` (`include/HitSounds.h`, `tests/test_hit_sounds.cpp`) and
`AbilitySounds` (`include/AbilitySounds.h`, `tests/test_ability_sounds.cpp`).
Both read the server's game events, the same ones that draw the effects,
damage numbers and hit sparks.

## Hits

| Hit | Sound | Loudness |
| --- | --- | --- |
| An enemy is hit | `hit`: a short punch | 60 % to 100 % with damage (full at 40) |
| An enemy takes a crit | `crit`: a heavier impact | full |
| You are hit | `hurt`: a grunt | full, centred |
| A teammate is hit | none | |
| Damage over time (poison, burns) | none | |

Enemy hits fade with distance from your hero (full within 250 px, silent
beyond 1,400 px) and pan left or right with where they land (fully at
900 px to the side).

## Abilities

No ability has a hand-picked sound. An ability sounds like **what it does
on screen**, so every kit, and every new kit, is covered:

- **The shape** it draws picks the main sound: slash (knife slice), cone
  (rushing fire), burst (explosion crunch), ring (phaser sweep), beam (big
  laser), projectile (small laser), lob (cloth swish), ground zone (force
  field), dash (phase jump), tether (zap), shield (power-up). A cast that
  draws nothing (a buff) gets a short generic blip. When a cast draws
  several shapes, the most telling one wins: beam, projectile, slash, cone,
  lob, burst, zone, tether, dash, shield, ring.
- **The theme** adds a tint on top, at 55 % volume: Holy a bell, Steel
  metal, Brew glass, Optic computer noise, Nanite skittering, Biomass slime,
  Blood a wet thud, Shadow a low sweep, Fire a roar, Heal a rising tone.
  Wind and Neutral have none. A theme always sounds the same, just as it
  always has the same colour (`EFFECT_VISUALS.md`).
- **Whose cast:** yours play up front (basic attack 60 %, the rest 90 %);
  teammates' at 40 % of that, fading with distance and panned, silent past
  1,400 px.
- **When:** the server sends a cast's shapes and then the cast in the same
  tick, and the client groups them. A cast with nothing drawn yet waits up
  to 450 ms for its first shape and sounds when that appears, in step with
  wind-ups (Gravon's swing lands 370 ms after the press). A cast that turns
  out to draw nothing plays the blip, and that player's slot is remembered,
  so next time it plays at once.
- **Silent:** shapes no cast claims, which are enemy attacks, ship and
  turret effects, and delayed blasts. Their hits make the sound instead.

What each ability sounds like today, found by running every kit on the
real combat code (`AbilitySystem`) through the grouper:

| Character | Key | Ability | Sound |
| --- | --- | --- | --- |
| Gravon | LMB | Titan's Slash | slash + Shadow |
| Gravon | RMB | Ravager's Edge | burst + Shadow |
| Gravon | Q | Fury of the Blade | ring + Shadow |
| Gravon | Shift | Blade Rush | dash + Shadow |
| Isaac | LMB | Ray of Benevolence | cone + Holy |
| Isaac | RMB | Lifeline | generic |
| Isaac | Q | The Blessing of Isaac | tether + Holy |
| Isaac | Shift | Flash of Sanctity | burst + Holy |
| Jeff | LMB | Thermal Disassembly | beam + Fire |
| Jeff | RMB | Acquire Biomass | ring + Biomass |
| Jeff | Q | Reverse of Installation | beam + Biomass |
| Jeff | Shift | They See Me Rollin' | burst + Steel |
| Kade | LMB | Refraction Wall | dash + Optic |
| Kade | RMB | Focused Blast | beam + Optic |
| Kade | Q | Full Release | ring + Optic |
| Mina | LMB | Pass the Ball | projectile + Fire |
| Mina | RMB | The Long Shot | lob + Fire |
| Mina | Q | GOAL!!!! | burst + Fire |
| Mina | Shift | Handball! | ring + Fire |
| Nolan | LMB | Look up! Secret Technique: | slash |
| Nolan | RMB | [TWIN DRAGON MODE ACTIVATE | ring |
| Nyx | LMB | Nanite Swarm | generic |
| Nyx | RMB | Plague Bringer | burst + Nanite |
| Nyx | Shift | Become the Swarm | tether + Nanite |
| El Presidente | LMB | Lucha Libre | slash + Holy |
| El Presidente | RMB | Pesado | dash + Blood |
| El Presidente | Q | Grande Cajones | cone + Holy |
| El Presidente | Shift | El Splash de Rana | generic |
| Suds | LMB | Brew Toss | lob + Brew |
| Suds | RMB | Frothy Cleanse | cone + Brew |
| Suds | Q | Round's On Me! | lob + Brew |
| Suds | Shift | What's In This Stuff? | burst + Brew |
| Volgore | LMB | Hamstring | slash + Blood |
| Volgore | RMB | Hooked | cone + Blood |
| Volgore | Q | Ferocious Bellow | burst + Blood |
| Volgore | Shift | Rage Unending | ring + Blood |
| Wade | LMB | Simple Punches | slash + Steel |
| Wade | RMB | Uppercuts | slash + Steel |
| Wade | Q | Slip | ring + Steel |
| Wade | Shift | Roll | dash + Steel |
| Zephyr | LMB | Wind Slash | projectile |
| Zephyr | RMB | Vortex | zone |
| Zephyr | Q | Gale's Embrace | shield |
| Zephyr | Shift | Tempest Dash | dash |

Kade's Mirror Step, Nolan's Q and Shift, and Nyx's Nanite Cloud sent no
cast event in that test (they need their setup first: mirrors, a combo,
the swarm out). They are silent until they send one.

## Keeping the mix readable

A four-player fight makes dozens of hits a second, so:

- **Gaps and voices:** the same sound starts at most once every 35 ms
  (`hit`), 60 ms (`crit`) or 250 ms (`hurt`), with at most 4, 2 and 1
  copies sounding at once. Anything over is dropped.
- **Variants:** each sound has three pitch variants (0.94x, 1x, 1.07x),
  chosen at random and never the same twice in a row, so repeated hits do
  not sound like a machine gun.
- Mute (`M` or `F2`) mutes sound effects with the music.

- Ability shapes and tints have gaps and voice caps too (40 to 90 ms, 2
  or 3 voices; `AbilitySounds::limitsFor`).

The numbers are in `include/HitSounds.h`, `include/AbilitySounds.h` and
`SfxSystem::MASTER_VOLUME` (0.7, relative to the music's 0.5).

## Adding or replacing sounds

- **Hits:** add the clip to `SOUNDS` in `tools/sfx/make_hit_sounds.py`
  and run it; it writes `assets/sfx/<name>_1..3.ogg`.
- **Abilities:** edit `SHAPES` or `TINTS` in
  `tools/sfx/make_ability_sounds.py` and run it on the unzipped Kenney
  packs; it writes `assets/sfx/ability/shape_*_<n>.ogg` and
  `tint_*_<n>.ogg`, trimmed and levelled. The game loads however many
  numbered variants there are (up to 8).
- A theme a new kit uses needs a tint added to both the tool and
  `AbilitySounds::tintName`.
- Credit every source in `assets/sfx/CREDITS.md`. Only use CC0 or licences
  the team has checked.

## Not done yet

- Enemy attack telegraphs: the most important next sound for Clarity.
- Signature sounds per character (a manifest keyed by character and slot
  that replaces shape and tint), especially for the abilities that only
  draw a ring or nothing.
- Your own casts sound when the server confirms them, so online they lag
  by the round trip. Playing them on the button press would need client
  prediction of casts.
- Kills, UI, pickups, objectives, footsteps.
