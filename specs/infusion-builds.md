# STATUS

**PROPOSED, revision 2** (2026-11-25, design session with a human). The
four verb cores (Part 0) and leaning into archetype identity were
**approved by the human in the session**; everything else is a proposal.
Nothing here is built. Extends `specs/genera-and-infusion-system.md` and
`specs/infusion-grades.md`. Prior art: the Risk of Rain 2
items-and-builds research shared in the session. Characters come from the
team's Character Concepts doc (Drive); concept-only characters are marked
with \*. The review copy, with comments, is the Claude doc "Infusion
Builds Spec".

# Infusion builds: start from the end state

We design the infusion system backward from 18 end-state builds, each
built on a proven Risk of Rain 2 engine. Every core ability is now its
genus's effect verb, and every archetype gets its own identity in the suit.

## This revision

- **The cores are now verbs.** Venoxarid Outbreak (Contaminate),
  Cerebrian Inversion (Corrupt and Purify), Frigidariant Stasis (Anchor
  and Doom), Magnarok Cauterize (Detonate, kept). Toxic Miasma, Mind Flay
  and Diamond Fortress are retired. **This goes against the Genera and
  Infusion doc**; the human approved it in the session.
- **Every build was rebuilt from its core.** In revision 1 only 7 of the
  17 builds with a core used the core in their engine; now all 17 do.
- **Answers from the human:** lean into archetypes (trait multipliers plus
  16 archetype signature talents); add the Crown part, and the trees were
  too shallow (signature talents add a third option at tier 3); use the
  full concept roster.
- **Recommended defaults** for the two open answers: Frigidariant's buff is
  Fortified, and Raw part effects stack per copy, up to 3 stacks per hit.
- **Roster mismatch:** Nolan is a Playmaker in the Character Concepts doc
  and a Striker in `src/CharacterKits.cpp`. This spec follows the code.

## Diagnosis

The suit works (grades, combining, attunement, Ascension, 48 genus
effects, 28 talents), but builds don't form. From the code:

1. **Items are stat sticks.** A part is +6% damage, +12 health, 6% recharge
   or 4% speed, with no trigger to combine with anything.
2. **Infusions don't speak the kits' language.** Kits use the effects
   system (Guard, Wound, Expose, Silenced, Grappled). Most genus effects
   use their own poison, freeze and molten ground.
3. **Nothing touches RMB charge**, which every RMB has so items could.
4. **Damage items barely work on Playmakers and Wingers**, two of the four
   archetypes.
5. **Nothing reacts to teammates.** Comradery needs that at the centre.
6. **A third of the talents are flat numbers.**
7. **Most cores were a ticket to their genus, not part of a build.** Only
   Cauterize read the game state; the other three each did one fixed
   thing.

## The layers

Every slotted infusion gives four things at once, and your archetype
shapes all of them:

| Layer | What it is | From |
| --- | --- | --- |
| Talent | the node's own effect | the node you slot into |
| Part effect | the item's effect through one kit button | the item's part |
| Part grade | what the part effect becomes at Refined and Exalted | the item's grade |
| Genus effect | the genus × tree effect, by grade | the item's genus and the tree |
| **Archetype** | trait multipliers on all of the above, plus a signature talent at every tier 3 | your character |

The core (E) is the first infusion, and it is now the genus's verb.

Your Exalted item is two capstones in one: an Exalted Magnarok Eye in
Offense tier 4 gives Inferno Heart (genus), a talent, *and* the Exalted
Eye rule (part). Combining keeps the target's part, so an unwanted Talon
can still become the grade of the Eye you want. That is our scrapper and
printer.

## Part 0: the cores are genus verbs

All four share one shape so the Genealogy tree can modify any of them:
**E targets an area (radius 200, range 500, at the cursor) and does the
verb to everything in it.** Three of the four deal no damage of their own,
so every archetype can use every core; each reads what anyone has applied,
so the crew's effects make your core stronger.

| Genus | Core (E) | Verb | What it does | So it works alone | Cooldown (guess) |
| --- | --- | --- | --- | --- | --- |
| Venoxarid | **Outbreak** | Contaminate | The enemy with the most debuff stacks in the area is the source; every other enemy there gets a copy of its full pile (was half stacks; the sandbox showed half was too weak) | enemies in the area gain 2 Poison first | 12 s |
| Cerebrian | **Inversion** | Corrupt, Purify | Enemies' buffs become their opposite debuffs (Berserk → Stunned, Focused → Confused, Fortified → Vulnerable); allies' debuffs become their opposite buffs (Slow → Haste, Weakened → Empowered, Poison → Mend) | enemies in the area gain 2 Expose | 14 s |
| Frigidariant | **Stasis** | Anchor, Doom | For 4 s, effects in the area stop running down: enemies are Doomed (debuffs can't be cleansed) and their debuff timers freeze; allies are Anchored and their buff timers freeze | enemies in the area gain 2 Slow | 14 s |
| Magnarok | **Cauterize** (kept) | Detonate | Every over-time effect in the area resolves at once: enemies' Wound and Poison as a blast scaled by the time left; allies' Mend as an instant heal (new) | none needed: the kits apply Wound and Mend | 12 s |

The cores chain across the crew: Stasis keeps time on the Wound that
Cauterize scales with; Inversion turns enemy buffs into debuffs that
Outbreak then spreads; Outbreak gives Cauterize more targets.

**Retired:** Toxic Miasma (its cloud lives on in the Venoxarid Exalted
Miasma Heart), Mind Flay and Diamond Fortress. All three are built today
(`src/Infusions.cpp`), so this is replacement work.

**Unchanged:** your first infusion still becomes your core (see open
questions).

## Archetype identity

### Trait multipliers

What an infusion produces is scaled by your archetype's maximum for that
trait (`ARCHETYPES.md`). The same item means different things on different
characters, as in RoR2, and the maxima hold ("no Playmaker out-heals a
Winger").

| Archetype | DMG | Healing | CC | Resilience | Mobility |
| --- | --- | --- | --- | --- | --- |
| Playmaker | ×0.25 | ×0.75 | ×1.25 | ×0.75 | ×1.25 |
| Defender | ×0.5 | ×0.75 | ×0.75 | ×1.25 | ×0.25 |
| Striker | ×1.25 | ×0.5 | ×0.25 | ×0.75 | ×0.75 |
| Winger | ×0.25 | ×1.25 | ×0.75 | ×0.75 | ×1.0 |

Multipliers are first guesses (Very High ×1.25, High ×1.0, Mid ×0.75, Low
×0.5, Very Low ×0.25). Each infusion output carries one trait tag:

- **DMG:** damage, Empowered, Sharpened, and Vulnerable or Expose put on
  enemies.
- **Healing:** heals, Mend.
- **CC:** Slow, Snared, Stunned, Confused, Silenced, Grappled and Marked
  put on enemies.
- **Resilience:** shields, Guard, Fortified, damage reduction, and
  Weakened put on enemies.
- **Mobility:** Haste, move speed, SHIFT recharge.

Multipliers scale durations and amounts, never stack counts, so
thresholds like "10 Poison" read the same for everyone. Tooltips always
show your own numbers.

Example: the same Glacier build roots for 1.25 s on Valthor\* (Defender,
CC Mid) and 0.4 s on Mina (Striker, CC Very Low), but Mina's Splinter hits
2.5 times as hard. Same items, two different jobs.

### Signature talents

Every tree's tier 3 gets a third option that only your archetype sees: 16
signature talents. They live at tier 3 because most players should reach
tier 3 somewhere by the boss (the grades spec's target). They also make
the trees wider, answering "the trees are too shallow". The talents are
listed under "Talents" below.

## Playmaker feel: plays, not damage

Decided by the human on 2026-11-26 (option A): a Playmaker's big moment is
a setup the crew cashes in, not their own damage. The reference moments are
a cross-map initiation arrow, a kick that repositions one key enemy into
your team, and an ultimate that pulls a whole team together. Tested in the
Presidente Sandbox (a browser mock of El Presidente in a live fight).

- **Setup credit (scoring rule).** Damage teammates deal to enemies you
  moved or controlled in the last 3 s counts toward your score, as its own
  line on the meter and scoreboard. Without it a Playmaker almost never
  wins the round's bonus infusion for top score.
- **Plays.** When one action catches 3 or more enemies, the game tracks
  what the crew does to them for 3.5 s. A big result gets a callout:
  "PLAY MADE: 6 kills · 1,400 crew damage off your Bomba".
- **Payoff through the crew.** Setup (Playmaker Offense talent) makes
  enemies you grab, throw, slam or knock up Vulnerable ×3 for 3 s.
- **Gather replaces Hot Potato** (Playmaker Mobility talent): an enemy you
  throw, slam, pull or knock up takes the debuffs off every enemy it hits.
  With Outbreak copying the full pile, one thrown elite picked up 64 stacks
  crossing a pack and spread them to 14 enemies. Grab something tanky: a
  thug carrying 50 stacks dies before you can press E. Rejected on the way:
  "your next core centres on the held enemy" (no way to stage it), a
  press-E-to-arm version (built around one character's buttons), Collateral
  (no link to the core) and a charged echo (fights the gather loop).

| El Presidente, 3 min of autopilot | Setup credit | Crew kills | Mina downs |
| --- | --- | --- | --- |
| Base kit, no infusions | 26,700 | 379 | 3 |
| Full build (9 pieces) | 20,700 | 373 | 2 |

**Still open:** the full-pile copy plus Stoke clears a pack before you can
watch it build (fixes to try: Stoke doubles only the source; cap copied
stacks at 30). The full build does not yet beat his bare kit on setup
credit: the autopilot holds packages instead of grabbing nonstop, so this
needs testing with real players.

## The 18 end states at a glance

Each build's **core is part of its engine**. Pieces: **Core**, then **Deep**
(the tree taken to tier 4, 8 Raw, carrying the capstone), then the
talents and parts that matter, then **Shallow** (the rest of a ~15-Raw
match). **Half-built** is the build at about 6 Raw. No end state may be a
trap before it is finished (the Benthic Bloom lesson).

| # | Build | Archetype (best characters) | Core | Capstone (genus · tree) | RoR2 ancestor |
| --- | --- | --- | --- | --- | --- |
| 1 | Permafrost Lockdown | Playmaker (Zephyr, Kade, Mason\*) | Stasis | Frigidariant · Genealogy (Diamond Heart) | Stun Grenade lock + Death Mark |
| 2 | Puppet Master | Playmaker (Nyx, Kade) | Inversion | Cerebrian · Offense (Puppeteer) | Captain's drone army |
| 3 | Gravity Well | Playmaker (Zephyr, Mason\*, Kade) | Outbreak | Venoxarid · Genealogy (Miasma Heart) | Primordial Cube + Royal Capacitor |
| 4 | Hot Potato | Playmaker (El Presidente; Zephyr) | Outbreak | Magnarok · Genealogy (Supernova) | Bungus Engineer (kit-specific value) |
| 5 | Battery | Winger (Miles\*, Isaac, Lilith\*) | Inversion | Cerebrian · Defense (Psychic Link) | N'kuhana's Opinion, routed to an ally |
| 6 | Plague Doctor | Winger (Eliana\*, Stitches\*) | Outbreak | Venoxarid · Defense (Symbiote) | Void corruption, inverted |
| 7 | Courier | Winger or Playmaker (Miles\*, Eliana\*, Isaac, Zephyr) | Stasis | Venoxarid · Mobility (Swarm Rider) | Perma-Spinel Tonic + War Horn |
| 8 | Thornwall | Defender (Volgore, Valthor\*) | Outbreak | Frigidariant · Defense (Cryo Sanctuary) | Razorwire + Hellfire Tincture |
| 9 | Pressure Keg | Defender (Suds, Nemora\*) | Cauterize | Magnarok · Defense (Heat Sink) | Transcendence |
| 10 | Glacier Turret | Defender or Striker (Valthor\*, Mina) | Stasis | Frigidariant · Offense (Glacier) | Bungus Engineer (stand still) |
| 11 | Haymaker | Striker (Wade, Gravon, Mordryn\*) | Cauterize | Magnarok · Offense (Inferno Heart) | Kjaro's + Runald's Bands |
| 12 | Glass Comet | Striker (Gravon, Mordryn\*) | Inversion | Magnarok · Mobility (Firewalker) | Shaped Glass on an i-frame survivor |
| 13 | Momentum | Striker (Wade, Jeff, Mordryn\*) | Stasis | Venoxarid · Mobility (Swarm Rider) | Speed Loader |
| 14 | Plague Chain | Striker (Jeff, Mina) | Outbreak | Venoxarid · Offense (Plague Carrier) | AtG + Ukulele, Voidsent Flame |
| 15 | Cauterizer | Striker (Mina, Jeff) | Cauterize | Magnarok · Genealogy (Supernova) | Poison-executioner Acrid |
| 16 | Overkill | Striker (Gravon, Nolan) | Stasis | Cerebrian · Mobility (Phantom) | Crit Huntress, Railgunner |
| 17 | Core Loop | any (Kade, Nyx, Eliana\*) | any | any · Genealogy (Overmind shown) | Gesture + Fuel Cells |
| 18 | Chimera | any, never Ascends | Inversion | none (wide) | Growth Nectar + Wake of Vultures |

Cores are spread evenly: Stasis 5, Outbreak 5, Inversion 4, Cauterize 3,
plus Core Loop. The 17 Ascended builds use 15 of the 16 capstones; two
pairs share one (Courier and Momentum; Hot Potato and Cauterizer) and
play differently through their talents and parts. Avalanche (Frigidariant
· Mobility) is the only capstone no build wants.

## Playmaker builds

### 1. Permafrost Lockdown: "Nothing moves while I'm here."

- **Ancestor:** stun-locking with Stun Grenade; Death Mark turning
  debuffs into team damage.
- **Engine:** Talons stack Slow, escalating to Snare → **Stasis** freezes
  the timers and Dooms them, so the lock holds and can't be cleansed →
  Diamond Heart makes the core Stun everything it touches → Brittle makes
  everything you control Vulnerable to the whole team.
- **Deep:** Genealogy, Frigidariant: Unbreakable → Brittle → Diamond
  Heart. Talents Bloodline, Purebred, **Lockdown** (signature), Feedback
  Loop.
- **Parts:** Frigidariant Talons; the Exalted is a Crown (Throne radiates
  its Fortified to allies).
- **Shallow:** Mobility 1 to 2.
- **Half-built:** Stasis on Zephyr's vortex: a stunned cluster that stays
  stunned.
- **Team:** Vulnerable on every controlled enemy. Deals almost nothing
  alone, by design.
- **Weak to:** Berserk and Unbounded elites (unless a crewmate's
  Inversion corrupts Berserk into Stunned).

### 2. Puppet Master: "Their army is my army."

- **Ancestor:** Captain's and Engineer's drone armies.
- **Engine:** **Inversion** corrupts Focused (immunity) into Confused and
  Berserk into Stunned, so the toughest enemies become yours first →
  Dominate confuses more → Puppeteer doubles confused enemies' damage to
  their friends and gives you their kills.
- **Deep:** Offense, Cerebrian: Psychic Shockwave → Dominate → Puppeteer.
  Talents Killer Instinct, Relentless, **Setup** (signature), Focus Fire.
- **Parts:** Cerebrian Eyes (charged RMB stacks Expose); the Exalted Eye
  (Heavy Hand) makes one full charge count as three Dominate hits.
- **Half-built:** Inversion + Raw Cerebrian Talons.
- **Team:** confused enemies don't attack the crew; puppet kills give you
  loot, never score taken from a teammate.
- **Weak to:** few enemies (the boss). **Clarity:** which enemies are
  yours needs one look everywhere.

### 3. Gravity Well: "I set the table."

- **Ancestor:** Primordial Cube + Royal Capacitor: gather, then hit all.
- **Engine:** the kit clusters (Zephyr's vortex, Mason's\* walls, Kade's
  mirrors) → you load one enemy with debuffs → **Outbreak** copies them to
  the whole cluster → a teammate's AoE lands on a pre-debuffed pack.
- **Deep:** Genealogy, Venoxarid: Venom Infusion → Carrier → Miasma Heart
  (a cloud follows you; drag the cluster through it). Talents Bloodline,
  Hybrid Vigor, **Crowd Shield** (signature), Feedback Loop.
- **Parts:** Venoxarid Eyes (charged RMB pours Poison by charge).
- **Shallow:** Raw Frigidariant and Magnarok Talons, so the load holds
  Slow and Wound too.
- **Team:** exists to multiply someone else's AoE.
- **Weak to:** single targets.

### 4. Hot Potato: "Catch."

- **Ancestor:** Bungus Engineer: one character makes an item great.
- **Engine:** El Presidente grabs the most-debuffed enemy → **Gather** makes
  it take the debuffs off every enemy it flies through → he presses E where
  it lands, and **Outbreak** copies its whole pile to the pack → every third
  cast is a Supernova.
- **Deep:** Genealogy, Magnarok: Cataclysm → Stoke → Supernova. Talents
  Bloodline, Purebred, Overcharge, Apex Mutation.
- **Shallow:** Mobility 1 to 3 for **Hot Potato** (signature).
- **Parts:** Magnarok Talons (LMB burns the held foe).
- **Team:** Wound across a pack feeds every Cauterize in the crew.
- **Weak to:** nothing to grab (ranged packs, the boss).

## Winger builds

### 5. Battery: "When I heal you, you hit harder."

- **Ancestor:** N'kuhana's Opinion, routed to an ally because a Winger's
  DMG stays Very Low.
- **Engine:** Crowns gather buffs from what anyone does to nearby enemies
  → **Inversion** turns the crew's debuffs into buffs → **Conduit** hands
  every buff you carry to whoever you heal → Psychic Link hands over your
  barrier too. Miles\* already Radiates and Doubles Mend and Haste.
- **Deep:** Defense, Cerebrian: Mind Barrier → Backlash → Psychic Link.
  Talents Plating, Regrowth, Guardian, Share the Load.
- **Shallow:** Offense 1 to 3 for **Conduit** (signature).
- **Parts:** Crowns of several genera (each a different buff); Refined
  Crowns also give the buff back to the ally who applied the status.
- **Team:** the whole build.
- **Weak to:** a split crew. **Clarity:** ally buff icons must say who
  gave them.

### 6. Plague Doctor: "Their sickness is our medicine."

- **Ancestor:** Void items transforming stacks, used here on effects.
- **Engine:** Eliana's\* Hex Weakens enemies, and her passive heals her
  from damage Weakened enemies take, overflowing to allies → **Outbreak**
  copies Weakened to the whole pack → Corrosion keeps enemies near you
  Weakened → Symbiote heals the crew when poisoned enemies die near you.
  Stitches\* plays it with Wound instead.
- **Deep:** Defense, Venoxarid: Acidic Shell → Corrosion → Symbiote.
  Talents Plating, Second Wind, **Purifier** (signature), Last Stand.
- **Parts:** Venoxarid Hides (Refined: Bodyguard, they fire when a nearby
  ally is struck).
- **Team:** a pack that hits the crew softer and heals it as it dies.
- **Weak to:** enemies that cleanse.

### 7. Courier: "Everyone's fast when I'm here."

- **Ancestor:** perma-Spinel Tonic + War Horn: 100% uptime on a timed buff.
- **Engine:** SHIFT and kits hand out Haste (Slipstream, Tailwind,
  Refined Sinews, Eliana's\* totem, Miles's\* Exploit) → **Stasis**
  Anchors the crew and freezes their buff timers, so the Haste stays →
  Haste also makes abilities recharge faster, so team Haste is team
  cooldown → Swarm Rider keeps your SHIFT up.
- **Deep:** Mobility, Venoxarid: Speed Siphon → Toxic Wake → Swarm Rider.
  Talents Light Frame, Slipstream, **Arrival** (signature), Escape Artist.
- **Parts:** Venoxarid Sinews; the Exalted one (Reset) gives back SHIFT
  time per enemy its trail touches.
- **Team:** the crew crosses the map faster: **the build against dead
  time** (Crunch's key insight).
- **Weak to:** low kill power; Haste caps.

## Defender builds

### 8. Thornwall: "Hit me. Please."

- **Ancestor:** Razorwire + Hellfire Tincture: the thorns tank.
- **Engine:** draw hits (**Lightning Rod** taunts; Valthor's\* domain makes
  attackers Vulnerable) → every hit fires every Hide on the attacker
  (Slow, Poison, Wound) → **Outbreak** copies the loaded attacker's pile
  to the whole pack → Cryo Sanctuary and Last Stand keep you up.
- **Deep:** Defense, Frigidariant: Arctic Aura → Permafrost → Cryo
  Sanctuary. Talents Plating, Second Wind, **Lightning Rod** (signature),
  Share the Load.
- **Parts:** Hides of every genus; the Exalted Hide (Backlash Aura) fires
  on every enemy within 150.
- **Team:** aggro off the squishy players; Share the Load's damage fires
  your Hides too.
- **Weak to:** ranged packs, big burst.

### 9. Pressure Keg: "Everyone's armour is my bomb."

- **Ancestor:** Transcendence: health becomes a regenerating pool.
- **Engine:** Suds hands out Guard; Frothy turns lost Guard into Wound on
  enemies; his SHIFT doubles Guard and Wound → **Cauterize** detonates it
  all → Heat Sink turns the damage you defer into a fire nova and
  Fortified → **Bastion** regenerates your Guard.
- **Deep:** Defense, Magnarok: Slow Burn → Cauterizing Wounds → Heat Sink.
  Talents Plating, Regrowth, Guardian, Last Stand.
- **Shallow:** Genealogy 1 to 3 for **Bastion** (signature).
- **Parts:** Cerebrian Hides in shallow slots (Guard when struck).
  Nemora\* plays it with her Guard stances.
- **Team:** less damage for everyone; enemies hitting an ally's Guard feed
  your Cauterize.
- **Weak to:** burst through Guard; enemies that cleanse.

### 10. Glacier Turret: "Hold here."

- **Ancestor:** Bungus Engineer: standing still is the build.
- **Engine:** stand still → Glacier (Fortified; hits freeze at once) →
  Splinter bursts frozen targets → **Stasis** freezes the timers on your
  Fortified and on enemies' Slows and Stuns. Valthor's\* Sovereign Domain
  is a place to stand.
- **Deep:** Offense, Frigidariant: Icy Strikes → Splinter → Glacier.
  Talents Killer Instinct, Opening Strike, Executioner, Focus Fire.
- **Shallow:** Mobility 1 to 3 for **Anchor** (Defender signature: allies
  near you are Fortified while you hold still).
- **Parts:** Frigidariant Eyes.
- **Archetype:** the multiplier example. On Valthor\* it is a CC wall; on
  Mina it is a damage turret with short freezes.
- **Team:** anchors the ship against raids. **Crunch:** the clock makes
  you choose between holding and going out.

## Striker builds

### 11. Haymaker: "One punch, whole pack."

- **Ancestor:** Kjaro's + Runald's Bands: huge hits trigger huge effects.
- **Engine:** the **Exalted Eye (Heavy Hand)**: a full-charge RMB counts as
  three LMB hits → three Magnarok Talon burns in one punch reach
  Flashpoint → **Cauterize** detonates whatever Wound is left.
- **Deep:** Offense, Magnarok: Eruptive Strikes → Flashpoint → Inferno
  Heart. Talents Killer Instinct, Opening Strike, **Overkill** or
  Executioner, Bloodlust.
- **Parts:** Magnarok Talons in tiers 1 and 2; the Exalted is an Eye.
- **Half-built:** Wade's three-punch combination already reaches
  Flashpoint.
- **Weak to:** whiffs. A missed charge is nothing: skill shows.

### 12. Glass Comet: "Don't get hit."

- **Ancestor:** Shaped Glass on characters with i-frames.
- **Engine:** **Overclock**: always Vulnerable, but Empowered caps higher →
  Magnarok Crowns Empower you whenever enemies near you burn (from anyone)
  → Firewalker keeps you in Burning Rush → **Inversion** cast on yourself
  turns your Vulnerable into Fortified for its duration: timing the core
  is how you play around the downside.
- **Deep:** Mobility, Magnarok: Burning Rush → Eruption Dash → Firewalker.
  Talents Light Frame, Quick Recovery, Hit and Run, Blitz.
- **Shallow:** Defense 1 to 3 for **Overclock** (signature).
- **Parts:** Magnarok Crowns and Sinews.
- **Team:** needs a Winger, which is the Comradery hook. Gravon's
  Soulblade and Mordryn's\* Stealth are its i-frames.
- **Weak to:** one-shots, with a visible downside you can play around.

### 13. Momentum: "Speed is damage."

- **Ancestor:** Speed Loader.
- **Engine:** kits Haste you (Wade's third punch, Jeff's treads,
  Mordryn's\* Frenzy) → Speed Siphon adds speed per poisoned enemy →
  **Stasis** on yourself freezes your Haste timers so the stacks build →
  **Momentum** turns speed above base into damage.
- **Deep:** Mobility, Venoxarid (Courier's tree): Speed Siphon → Toxic
  Wake → Swarm Rider. Talents Light Frame, Slipstream, **Momentum**
  (signature), Blitz.
- **Parts:** Venoxarid Talons (Poison feeds Speed Siphon).
- **Pair:** the same capstone as Courier. Courier hands out speed;
  Momentum spends it.

### 14. Plague Chain: "It spreads."

- **Ancestor:** AtG + Ukulele chains; Voidsent Flame.
- **Engine:** the **Exalted Talon (Echo)** splashes each LMB's Poison →
  Rupture pops at 10 stacks onto neighbours → Plague Carrier hands a dead
  enemy's stacks on → **Outbreak** copies the most-loaded enemy's pile to
  the pack in one press. Jeff's beam hits often and his biomass turns the
  crew's kills into fuel.
- **Deep:** Offense, Venoxarid: Toxic Blade → Rupture → Plague Carrier.
  Talents Killer Instinct, Relentless, Affliction, Bloodlust.
- **Parts:** Venoxarid Talons; the Exalted is a Talon.
- **Needs:** the proc rule and the visual budget (Rules).
- **Weak to:** bosses.

### 15. Cauterizer: "Let it all go off at once."

- **Ancestor:** poison-executioner Acrid.
- **Engine:** mixed genera load every over-time effect (Poison, Wound,
  Suds's Frothy) → Stoke doubles them → **Cauterize** explodes all of it,
  scaled by the time left → every third cast is a Supernova →
  **Opportunist** brings the core back when you kill something a crewmate
  debuffed.
- **Deep:** Genealogy, Magnarok: Cataclysm → Stoke → Supernova. Talents
  Bloodline, Hybrid Vigor, **Opportunist** (signature), Feedback Loop.
- **Shallow:** Offense 1 to 3 with Venoxarid (Refined Rupture, Executioner).
- **Parts:** Eyes.
- **Team:** best beside a crewmate's Stasis, which keeps the time on the
  pile.

### 16. Overkill: "Every hit is a crit, then more."

- **Ancestor:** crit Huntress; Railgunner's crit conversion.
- **Engine:** Cerebrian Crowns give Sharpened whenever enemies near you
  are Exposed (Nolan stacks Expose himself); Nyx's Focused gives Sharpened
  every second; Gravon's full charge adds one → **Stasis** on yourself
  freezes Sharpened timers so stacks pile up → at 100% crit, **Overkill**
  turns the rest into crit damage.
- **Deep:** Mobility, Cerebrian: Phase Shift → Ghost Step → Phantom.
  Talents Light Frame, Quick Recovery, Hit and Run, Blitz.
- **Shallow:** Offense 1 to 3 for **Overkill** (signature).
- **Parts:** Cerebrian Crowns; Gravon wants an Exalted Eye.
- **Team:** Nyx and Overkill are a pair: her Focused is his crit chance.

## Any-archetype builds

### 17. Core Loop: "It's always up."

- **Ancestor:** Gesture of the Drowned + Fuel Cells.
- **Engine:** Bloodline, Feedback Loop, Eyes and Haste shorten the core →
  Mind Echo makes every cast happen twice → Overmind links everything the
  core touched. With Outbreak, debuffs spread twice; with Inversion, the
  flip happens twice; with Stasis, the freeze is renewed.
- **Deep:** Genealogy, Cerebrian: Mass Confusion → Mind Echo → Overmind.
  Talents Bloodline, Purebred, Overcharge (or **Lifeblood** on a Winger),
  Feedback Loop.
- **Parts:** Eyes everywhere.
- **Guard rail:** the core's cooldown never goes below 3 s.

### 18. Chimera: "A bit of everything, all of it working."

- **Ancestor:** Growth Nectar + Wake of Vultures: power per distinct buff.
- **Engine:** never Ascend. All four trees to tier 2, all Raw, all four
  genera → Crowns of all four genera gather four different buffs →
  **Inversion** turns the crew's debuffs into more buffs → **Hybrid Vigor
  (reworked)** counts distinct buffs as well as genera.
- **Why it must exist:** if deep always beats wide, Ascension is the one
  dominant build. It also makes a late drop of any genus worth having.
- **Weak to:** no capstone moment. Reliable, with a lower ceiling.

## The builds as a team

The strongest moments should need two players ("when I win, we win"):

| Setup | Payoff | What links them |
| --- | --- | --- |
| Stasis (any Frigidariant core) | Cauterize (any Magnarok core) | the preserved time makes the blast bigger |
| Inversion on enemies | Outbreak | corrupted buffs become debuffs that spread |
| Permafrost Lockdown | Glass Comet, Haymaker | Brittle's Vulnerable; locked targets |
| Gravity Well | Plague Chain, Cauterizer | a clustered, pre-debuffed pack |
| Puppet Master | Thornwall | puppets and the taunt split the horde |
| Battery | Overkill, Haymaker | buffs handed over on every heal |
| Plague Doctor | Glass Comet | a Weakened pack and a healed cannon |
| Courier | everyone | Haste is speed and cooldown |
| Pressure Keg | Cauterizer, Hot Potato | Wound piles that someone detonates |
| Nyx (any build) | Overkill | Focused is crit chance |

## Parts and part grades

### Each part is a kit button

| Part | Hooks into | Raw effect (the genus delivers) | Stat line (kept, small) |
| --- | --- | --- | --- |
| Talon | LMB hits | each hit applies 1 genus status | +damage |
| Eye | RMB charge | release applies the status, stacks = charge level; +10% charge speed | +recharge |
| Sinew | SHIFT | SHIFT applies the status in a burst where you leave | +speed |
| Hide | being struck | the attacker gets the status | +health |
| **Crown** (new) | anyone's effects | when an enemy within 300 gets the status **from anyone**, you gain the genus buff (1 stack, up to 5) | none |

Q stays the kit's own. Defenders' SHIFT is a second unique; Sinew still
fires on it ("SHIFT was used", not "dashed"). Raw part effects stack per
copy, up to 3 stacks per hit (recommended default).

**Genus palette:**

| Genus | Status (Raw parts) | Escalates to (Refined Talon) | Buff (Crown) | Core verb |
| --- | --- | --- | --- | --- |
| Venoxarid | Poison | Weakened | Haste | Contaminate |
| Cerebrian | Expose | Confused (0.75 s) | Sharpened | Corrupt, Purify |
| Frigidariant | Slow | Snared (0.5 s) | Fortified | Anchor, Doom |
| Magnarok | Wound (burn) | Vulnerable | Empowered | Detonate |

All of these already exist in `EffectType` and the effect verbs
(`spreadEffect`, `corruptBuffs`, `purifyDebuffs`, `detonateEffect`,
`amplifyEffect`). Fortified for Frigidariant (recommended): the effects
spec's Iceborn Crown gave Empowered, but one buff per genus lets a Crown's
colour say what it gives.

20 items (4 genera × 5 parts). Genus follows the territory, so you steer it
by where you go (which costs time); the part is the random roll; combining
turns an unwanted part into the grade of a wanted one.

### One rule per part grade, the same for every genus

Refined **reaches** allies or more enemies. Exalted **breaks a rule**.

| Part | Refined | Exalted (rule break) |
| --- | --- | --- |
| Talon | **Escalate**: every 4th hit on the same target applies the escalation status | **Echo**: LMB statuses also land on every enemy within 90 of the target |
| Eye | **Overcharge**: hold up to 1 s past full; an overcharged release applies double | **Heavy Hand**: a full-charge RMB counts as 3 LMB hits for every on-hit effect |
| Sinew | **Wake**: allies you pass during or just after SHIFT gain the genus buff | **Reset**: SHIFT recharges 1 s per enemy its effect touches (up to half its cooldown) |
| Hide | **Bodyguard**: also fires when an ally within 250 is struck | **Backlash Aura**: fires on every enemy within 150, not just the attacker |
| Crown | **Share**: the ally who applied the status also gets the buff | **Throne**: up to 10 stacks, and your buff radiates to allies within 250 every 2 s |

Refined and Exalted part rules don't stack: you have them or you don't.
The Refined rules are where Playmakers and Wingers stop getting dead
items: an Empowered Crown does little for Kade, but a Refined one hands
Empowered to the Striker who applied the Wound.

## Genus effects

- **Keep** the 48 effects, but **re-speak them in the shared vocabulary**:
  a freeze is Stunned (so Berserk counters it and Brittle reads it),
  "poisoned enemies deal less" is Weakened, and so on.
- **Rewrite the Genealogy column.** Genealogy modifies your core, and the
  old effects assumed damaging cores ("core damage poisons"). The new
  effects work on any core, through the shared "act on an area" shape:

| Genus | Raw | Refined | Exalted |
| --- | --- | --- | --- |
| Venoxarid | **Venom Infusion**: your core applies 2 Poison to enemies it affects (+1 per level) | **Carrier**: each enemy your core affects passes one of its debuffs to the nearest enemy outside the area | **Miasma Heart** (kept): a small poison cloud always follows you |
| Cerebrian | **Mass Confusion**: your core Confuses enemies it affects for 0.5 s (+0.25 s per level) | **Mind Echo**: 1.5 s later, your core happens again on the same targets at 40% | **Overmind**: enemies your core affected are linked for 4 s; 30% of the damage and every debuff one gets is shared with the others |
| Frigidariant | **Unbreakable**: your core shields you for 5 per enemy it affects | **Brittle**: enemies your core affects, and any you stun or root, are Vulnerable for 3 s | **Diamond Heart**: your core Stuns everything it affects for 1.2 s; your shields don't expire while a stunned enemy is within 300 |
| Magnarok | **Cataclysm**: your core leaves molten ground (Wound) in its area | **Stoke** (replaces Fuel): before your core acts, it doubles every over-time effect in its area | **Supernova**: every third core cast covers double the area and leaves a molten field |

- **Rework Avalanche** (Frigidariant · Mobility), the one capstone no build
  wants. Candidate: "SHIFT through enemies Snares them; Snared enemies you
  pass are Stunned", a second Frigidariant end state for Zephyr.

## Talents

### General talents

The tier-1 legs (Killer Instinct, Plating, Light Frame, Bloodline) stay
flat: they are the cheap filler. **Hybrid Vigor** (Genealogy · 2) is
reworked to also count each distinct buff on you (up to 6). Contagion
from revision 1 is dropped: Outbreak does it.

### Signature talents (tier 3, one per tree per archetype)

| Archetype | Offense | Defense | Mobility | Genealogy |
| --- | --- | --- | --- | --- |
| Playmaker | **Setup**: enemies you grab, throw, slam or knock up are Vulnerable ×3 for 3 s, so the crew hits them harder | **Crowd Shield**: +1 Fortified for each enemy you have crowd-controlled within 300 (up to 5) | **Gather** (was Hot Potato): an enemy you throw, slam, pull or knock up takes the debuffs off every enemy it hits | **Lockdown**: crowd control on enemies your core affects lasts 50% longer and is Doomed |
| Winger | **Conduit**: your heals also give the ally 1 stack of every buff you carry | **Purifier**: your heals Purify one debuff on the ally | **Arrival**: ending your SHIFT within 150 of an ally Hastes them ×2 and heals 5% of their max health | **Lifeblood**: your core heals each ally it affects for 3% of max health per effect it changes on them |
| Defender | **Retribution**: damage your Guard, shields and Fortified prevent builds up to 60; your next hit deals it | **Lightning Rod**: SHIFT Marks you for 3 s (enemies within 300 attack you); Fortified ×2 while Marked | **Anchor**: after 1 s without moving, allies within 250 are Fortified ×1 and Unbounded | **Bastion**: your Guard regenerates 1 stack every 2 s (up to 3); every 25 max health above base raises your Guard cap by 1 |
| Striker | **Overkill**: crit chance over 100% becomes crit damage; Sharpened stacks past the cap add 10% crit damage each | **Overclock**: always Vulnerable ×3; Empowered caps 3 higher, and you start each fight Empowered ×2 | **Momentum**: +1% damage per 1% move speed above base (up to +40%) | **Opportunist**: killing an enemy carrying a debuff a crewmate applied takes 1 s off your core's cooldown |

Each archetype's four signatures turn its strongest trait into team value
or into its own fantasy. Defenders have no mobility, so their Mobility
signature is about holding ground.

## Rules the builds need

1. **Archetype multipliers** on everything infusions produce (above).
2. **Proc depth is 1, and the tooltip says so.** An effect caused by an
   infusion reaction (Echo splashes, Crown buffs, Carrier and Plague
   Carrier hand-offs, detonations, Mind Echo) is an **echo**: echoes never
   trigger Talons, Hides, Echo or Carrier again. Crowns listen to
   everything but only grant buffs, so they can't loop. The code already
   skips procs and damage over time for on-hit (`AbilitySystem.h`).
3. **Cooldown floors:** the core never below 3 s, SHIFT never below 1 s.
4. **Visual budget:** each player's echoes merge into one visible pulse per
   0.25 s (Clarity; "AtG lags the game hard").
5. **Pacing:** each end state fits about 15 Raw (core 1 + deep tree 8 +
   shallow 6) and is already doing its thing at about 6 Raw. Measure with
   `ArenaSim` once bots can aim for a build.

## Order of work

1. **The four verb cores** and the rewritten Genealogy column (approved;
   replaces three built cores).
2. **Parts as kit buttons** (Raw) for Talon, Eye, Sinew and Hide, with the
   proc-depth rule.
3. **Archetype multipliers**, shown in tooltips.
4. **Crown** and the ten part-grade rules.
5. **The 16 signature talents** and the Hybrid Vigor rework.
6. **Build presets for testing:** a debug key that gives a player one end
   state's exact pieces, so each of the 18 can be played in minutes and
   measured in the sims.

## Open questions

1. **Should your first drop still decide your core?** "Play the hand
   you're dealt" says yes; picking your core from your first two drops
   would let players aim for a build from round 1.
2. **Nolan's archetype:** Playmaker (Character Concepts doc) or Striker
   (the code)?
3. **Multiplier numbers:** are ×1.25 to ×0.25 the right spread, and should
   anything besides durations and amounts scale?
4. **Signature talents at tier 3:** the right depth, or should some sit at
   tier 2 so every player sees theirs?
5. **Recommended defaults to confirm:** Frigidariant's buff is Fortified;
   Raw part effects stack per copy, up to 3 stacks per hit.
