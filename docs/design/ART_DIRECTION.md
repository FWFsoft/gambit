# Art direction: silhouettes first

From the team's "On-Ramp: Art" doc: **clarity over all things.** Every new
character, enemy, particle effect or obstacle must pass the "who's that?"
test: black it out and you still know instantly what it is. Alien Arena
asks for split-second decisions, and it is extremely frustrating to make
the wrong one because you didn't know what you were looking at.

![Silhouettes before and after the shape pass](../prototype/silhouettes_before_after.png)

## Rules for every in-game asset

1. **Shape before detail.** What identifies a character must be in the
   outline, not inside it. A mask, a belt or a tattoo vanishes in
   silhouette.
2. **One dominant shape and one signature piece** that breaks the outline
   from every direction. The camera is high, so things above the head and
   things sticking out sideways read best; anything in front of the torso
   disappears.
3. **No shared skeletons by default.** Proportions are identity. Start from
   the character's own shape, not from another actor scaled up.
4. **Pose with attitude.** Idle and run poses must be asymmetric and
   characterful. Arms hanging at the sides is the most generic outline
   there is.
5. **Size is a cue, not the only cue.** Two characters with the same shape
   at different sizes still read alike in a fight.
6. **Detail level:** about 64 art pixels for an average character
   (DD-AA-007). Forge actors use `PIXEL_SCALE = 1.35`: chunkier pixels,
   same size on screen.
7. **Check it.** Run `python3 tools/sprite_forge/silhouette.py` after every
   bake and look at the lineup. The `/design-review` skill asks for it on
   art changes.

## Shape briefs

Write one before modelling a character. Keep it to a few lines.

**El Presidente: the inverted triangle.**
Boulder shoulders and a trapezius that buries his neck. Small legs planted
wide. A tall red-and-white crest on the mask is the top of his outline from
every side. His idle is a power pose (fists on hips, elbows flared), so
there are gaps between arms and body. From behind, the flag cape is a
solid banner.

**Wade: the lunge.**
Lean, narrow-waisted, always leaning in off a long bladed stance. Two
oversized white wrapped mitts held up beside his face: bright blobs that
read at any size. Swept-back white hair streams behind his head in side
views.

**Suds: the barrel and the cooler.**
A wide barrel of a torso in a blue Hawaiian shirt, on red armoured legs. A
big white cooler juts out at his left hip, and a brew can is raised high in
his right hand, so his outline is lopsided from every side. His moves
(toss, belch, a volley of brews, chug-and-flex) all come from the can arm.

**Gravon: the blade.**
A slim, shirtless body under a slab of a sword longer than he is tall. At
rest it lies back over his right shoulder, angled across his back, so it is
the top of his outline from every side, behind him included. In a fight it
sweeps level, rises straight up, or drives into the ground.

**Toxlith: the warty toad.**
Squat and wide: lumpy acid-green skin, a crest of magenta spines with
glowing venom beads on the tips, red eyes and a long pink tongue. Low and
wide, the opposite of every hero; the spiky crest is its top edge from any
side.

![The forge cast](../prototype/silhouettes_cast.png)

**For the rest of the cast**, start from their concept art; most of it
already has strong outline hooks:
- Zephyr: a ragged, flowing cloak.

## Enemies

Each genus needs its own body plan, and small, medium and large must
differ in shape, not just scale. A starting point:

| Genus | Body plan |
| --- | --- |
| Venoxarid | low and skittering, tails |
| Cerebrian | floating, swollen brain bulbs |
| Frigidariant | angular, crystalline, blocky |
| Magnarok | hunched, rocky, heavy |

The eleven old enemy sheets are one alien recoloured and rescaled. They
fail the test, as the on-ramp doc already says.

## Beyond silhouettes: finding yourself in a crowd

![The crowd test](../prototype/crowd_before_after.png)

When a swarm piles onto a player, enemy bodies cover their outline whatever
its shape. The crest keeps Presidente findable; Wade, half his height,
gets buried. Silhouettes tell you *which character*, but two more things
are needed:

- **A per-player colour rim or outline** that draws over enemies.
- **Distinct ring shapes for effects** (spiky poison, a turning crown for
  Empowered), not only colours.

Neither is built yet.
