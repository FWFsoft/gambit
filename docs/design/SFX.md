# Sound effects

Placeholder hit feedback, so every hit has a sound (Pillar 4, Clarity:
"every hit needs feedback: animation, flash, number and, eventually,
sound"). The sounds are free CC0 clips from Kenney's Impact Sounds pack
(`assets/sfx/CREDITS.md`) and should be replaced by the game's own sound
design later.

Code: `HitSounds` (`include/HitSounds.h`, the rules, tested by
`tests/test_hit_sounds.cpp`) and `SfxSystem` (`src/SfxSystem.cpp`, plays
them with SDL_mixer beside the music). It listens to the server's `Damage`
game events, the same ones that draw damage numbers and hit sparks.

## The rules

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

## Keeping the mix readable

A four-player fight makes dozens of hits a second, so:

- **Gaps and voices:** the same sound starts at most once every 35 ms
  (`hit`), 60 ms (`crit`) or 250 ms (`hurt`), with at most 4, 2 and 1
  copies sounding at once. Anything over is dropped.
- **Variants:** each sound has three pitch variants (0.94x, 1x, 1.07x),
  chosen at random and never the same twice in a row, so repeated hits do
  not sound like a machine gun.
- Mute (`M` or `F2`) mutes sound effects with the music.

The numbers are in `include/HitSounds.h` (`limitsFor`, `HEARING_RADIUS`,
...) and `SfxSystem::MASTER_VOLUME` (0.7, relative to the music's 0.5).

## Adding or replacing sounds

1. Put the source clip in a folder.
2. Add it to `SOUNDS` in `tools/sfx/make_hit_sounds.py` and run the tool
   on that folder; it writes `assets/sfx/<name>_1..3.ogg`.
3. Credit it in `assets/sfx/CREDITS.md`. Only use CC0 or licences the team
   has checked.

## Not done yet

Kill sounds, ability casts, enemy attack telegraphs (the most important
next one for Clarity), UI, pickups, objectives.
