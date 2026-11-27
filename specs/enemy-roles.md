# STATUS

**PROPOSED** (2026-11-27, design session with a human). The ideas and the
skill identities below are the human's; the rules, numbers and open
questions are a first pass to review. Nothing here is built. Companion to
`specs/infusion-builds.md`.

# Enemy roles: enemies are questions, archetypes are answers

## Why

Testing El Presidente in a browser sandbox showed the enemies, not the
builds, were the main problem:

- **Nothing needed a clutch play.** With El Presidente standing idle, his
  Striker crewmate went down only twice in 90 seconds.
- **Every enemy was interchangeable.** A horde of identical thugs plus one
  elite has no "right" target, so a Playmaker's setup was worth little
  more than grabbing whatever was nearest.
- **The real game has the same gap.** Only Venoxarid thugs (a grunt and an
  elite) spawn until the other genera have models, plus the Matriarch.

The fix: **each enemy asks a question one archetype answers best.** The
others can still answer, more slowly or at a higher cost, so a crew without
that archetype is in trouble but not stuck (Comradery: the crew needs each
other; nobody is useless).

## What each archetype's skill is

| Archetype | The skill the game should reward |
| --- | --- |
| **Playmaker** | **Clutch.** The right control at the right moment turns a fight. |
| **Striker** | **Uptime.** Consistent, sustained damage, never letting up. |
| **Winger** | **Vision and decisions, then defensive clutch.** Watching (and listening to) the crew, picking who to save, and being so mobile they arrive in time. |
| **Defender** | **Knowing your limits.** Standing in what nobody else can survive, right up to the edge, and not past it. |

## The roster

| Enemy | Question it asks | Best answer | Other answers | If nobody answers |
| --- | --- | --- | --- | --- |
| **Diver** | Can you catch it? | Playmaker: any control kills it | none: damage and effects do nothing | It lands on a lone crewmate and takes them down |
| **Channeler** | Can you interrupt it in time? | Playmaker: displacement or control | Strikers' few infusion stuns | Its spell goes off: its pack is healed and enraged, or the ship is hit |
| **Warden** + **prisoners** | Can you free them? | Winger: heal or buff a prisoner | Purify the prisoners' debuffs | The Warden stays nearly unkillable |
| **Hex totem** | Can you clean it up? | Winger: cleanse destroys it; purify turns it into a buff zone | Slow to break by damage | Crewmates in range keep a debuff |
| **Juggernaut** | Can you keep the pressure on? | Striker: sustained hits | Playmakers open stagger windows | It regenerates whatever you did |
| **Behemoth** | Can you hold it back? | Defender: body-block and tank it | Anyone can block it briefly; its hits are huge | It reaches the ship and smashes it |
| **Bruiser** | Can you stand in front of it? | Defender | Nobody else has the tools for its debuffs and hits | Whoever it targets goes down |
| **Swarm** (today's thugs) | none: filler | Area damage, Outbreak | everyone | Pressure, not disaster |

Genus variants come on top (see Magnarok below).

## The enemies

### Diver (Playmaker: clutch)

- Stalks in the fog of war, out of sight. Every so often it picks the
  most isolated or lowest-health crewmate, shows a 1 s red telegraph on
  them, and leaps.
- **Ignores damage and effects**: hits do nothing, debuffs do not stick.
- **Dies to any control**: stun, snare, knock-up, knockback, pull, grab or
  throw kills it on the spot ("CAUGHT").
- Once it lands it shreds its target every second until someone catches
  it.
- **Why it works:** a perfect Playmaker question. Strikers (no crowd
  control) cannot answer it, which makes them need their Playmaker.
- **Open:** does Slow count as control? If yes, every archetype has some
  answer, through infusions. If no, a crew without a Playmaker relies on
  the few hard-control infusions. First guess: Slow does not count.

### Channeler (Playmaker: clutch)

- The interrupt check from high-level dungeon play. Plants itself behind
  its pack and channels for 3 s, with a cast bar over its head.
- Interrupted by any control or displacement, including another enemy
  thrown into it.
- If it finishes, one of (by genus): heals and enrages its pack (Berserk:
  immune to stuns), shields its pack, or fires at the ship.
- Channelers in the same wave stagger their casts, so a crew has to
  rotate interrupts.

### Warden and prisoners (Winger: vision and decisions)

- A Warden carts two to four **withered prisoners** behind it on chains.
- **Every debuff put on the Warden moves onto its prisoners.** While it
  has prisoners it is effectively immune to debuffs, and without debuffs
  it is very tough (heavy damage reduction).
- **Heal or buff a prisoner and it breaks free.** Purifying a prisoner's
  debuffs into buffs frees it too.
- With no prisoners left, debuffs stick to the Warden and it becomes a
  normal, killable target. The Winger opens it, the crew's damage finishes
  it.
- **Open:** what freed prisoners do. Options: flee (reward: the Warden is
  exposed); fight beside the crew briefly; drop scrap or an infusion.

### Hex totem (Winger: vision and decisions)

- A small totem that keeps one debuff (Slow, Weakened, Silenced or Wound)
  on every crewmate in range.
- **Cleanse destroys it. Purify flips it into a buff zone** (Slow becomes
  Haste, Weakened becomes Empowered) for the rest of the fight, so the
  Winger's choice turns a problem into an asset.
- It can also be broken by damage, slowly.
- Placed by other enemies (a "hexer" variant of the elite) so it shows
  up in the middle of fights.

### Juggernaut (Striker: uptime)

- Huge health. **If nobody has hit it for 1 s, it regenerates quickly.**
- Its armour drops while it is staggered: a Playmaker's knock-up or stun
  opens a short window where Strikers deal much more.
- Rewards the Striker who never stops hitting, and gives the Playmaker
  something to set up.

### Behemoth (Defender: knowing your limits)

- A huge, slow unit that lumbers toward the ship and smashes it when it
  arrives.
- **Can be body-blocked**: it stops while something stands in its way, and
  attacks whatever blocks it, hard.
- Anyone can block it for a moment; only a Defender can keep blocking it.
  The question is how long you can hold before you must let go or get
  help.

### Bruiser (Defender: knowing your limits)

- Hits very hard and stacks heavy debuffs on its target (Wound, Weakened,
  Vulnerable).
- Non-Defenders simply lack the tools to survive it. A Defender can, with
  Fortified, Guard, Berserk and their own cleanses, but only if they
  manage cooldowns and know when to step out.
- This is where the Defender builds' low-health payoffs (Last Stand, Cryo
  Sanctuary, Volgore cashing in his Wound as healing) finally get used.

## Genus variants: Magnarok

- **Burning Magnarok enemies** constantly burn everything around them
  (damage over time on any crewmate nearby).
- **Kite or throw one into water** and it is quenched: it stops burning
  and turns **Brittle** (takes much more damage).
- Uses the map (the coast and inland water) and rewards displacement: a
  Playmaker's throw, a Defender's pull, Zephyr's wind.
- Every genus should get one such twist, so each territory asks something
  different. Ideas to develop: Frigidariant enemies that shatter when
  pushed into molten ground; Cerebrian enemies that are only visible to
  the crewmate they are hunting; Venoxarid enemies that burst into
  poison clouds that a Defender must absorb.

## Wingers: defensive clutch

Wingers should be hyper-mobile. Their vision skill is checking on the
crew, or hearing a call for help, and getting there in time. The enemies
above create those moments (a Diver's telegraph on a lone crewmate, a
Bruiser on the wrong target, a pincer of two packs at once). To make that
readable (Clarity):

- A crewmate in trouble shows at the screen edge (direction, how much
  health is left, what is on them).
- A one-key "I need help" ping.
- Winger infusions should reward arriving in time: Arrival already
  does; more of that family.

## What this means for the infusion builds

A build should make its archetype's answer bigger, not give it a second
job:

- **Playmaker:** longer reach, bigger pulls, safer interrupts. The
  cross-map clutch catch is a build goal.
- **Striker:** more value from staying on target (Momentum, Plague Chain).
- **Winger:** more value from the right choice of target, and faster
  arrivals (Battery, Purifier, Courier).
- **Defender:** payoffs for riding the edge (Last Stand, Cryo Sanctuary,
  Pressure Keg, Thornwall).

## First test (Presidente Sandbox)

The Diver and Channeler were added to the browser sandbox (Divers hunt the
Striker crewmate; a Channeler comes with each wave from wave 2). 3 minutes
of autopilot each:

| Run | Mina downs | Kills | Divers caught / landed | Rallies interrupted / completed |
| --- | --- | --- | --- | --- |
| Idle, old enemies | 4 | 264 | – | – |
| Idle, new enemies | 8 | 190 | 0 / 16 | 0 / 35 |
| Bare kit, new enemies | 4 | 364 | 8 / 7 | 31 / 1 |
| Full build, new enemies | 2 | 427 | 9 / 6 | 21 / 0 |

With the new enemies an idle Playmaker costs the crew heavily, and the
full build beats his bare kit on every measure, which it did not against
the uniform horde. Numbers are first guesses: a landed Diver deals 60 per
second; a completed rally heals its pack to full and makes it Berserk,
Empowered ×6 and 25% faster.

## Open questions for a human

1. Does Slow count as control for killing a Diver?
2. What do freed prisoners do?
3. Should these archetype skills be added to `docs/design/ARCHETYPES.md`?
4. Mix per wave: how many "questions" at once before it stops being
   readable? First guess: one or two per wave, on top of the swarm.
