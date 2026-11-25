# STATUS

**PROPOSED** (2026-11-25, design session; nothing here is built). Works
backward from 18 end-state builds to what each layer of the infusion
system has to provide. Extends `specs/genera-and-infusion-system.md` and
`specs/infusion-grades.md`. Prior art: the Risk of Rain 2 items-and-builds
research (34 community builds, design takeaways), shared in the session.
Open questions for a human are at the end.

# Infusion builds: start from the end state

## Why this doc

The suit works mechanically (grades, combining, attunement, Ascension, 48
genus effects, 28 talents), but the builds have not taken off. The aim
is **Risk of Rain 2's explosive roguelike builds in a game that plays like
a MOBA**: four players, four archetypes, a fixed five-button kit.

RoR2's builds that people still talk about are almost never one item. They
are 2 to 4 pieces that multiply each other ("Gesture + Fuel Cell",
"Transcendence + Shaped Glass", "Bungus Engineer"). So this doc starts from
the **end states** (what a player should be able to *become* by the boss)
and works back to what each layer has to supply.

## Diagnosis: why the current suit doesn't make builds

From the code (`src/ArenaData.cpp` PARTS and SUIT_NODES,
`src/Infusions.cpp`):

1. **Items are stat sticks.** A part is +6% damage, +12 health, 6% recharge
   or 4% speed. No item has a trigger, so items never combine with
   anything. In RoR2 these would all be white items: the stat backbone,
   never what a build is named after.
2. **Infusions don't talk to the kits.** Kits speak the effects system
   (Suds: Guard and Wound; Nolan: Expose; Nyx: Silenced and Focused;
   Presidente: Grappled; Volgore: Snared and Doomed). The genus effects
   mostly speak their own language (poison stacks, freeze chains, molten
   ground). Only Affliction and a few Refined effects (Brittle, Corrosion)
   read the shared vocabulary.
3. **Nothing touches the kit buttons.** Every RMB is charged *so that items
   can interact with charge time and charge amount*, and nothing does.
   Relentless touches LMB; a few Mobility talents touch SHIFT; nothing
   touches RMB or Q.
4. **Half the cast gets half the items.** Playmakers and Wingers have no
   base damage, so Talons, Opening Strike, Executioner, Overcharge and
   most Offense effects barely work for them. RoR2 has the same problem
   ("Fungus is ONLY good on Engineer"), but there it is the exception. Here
   it affects two of the four archetypes.
5. **Nothing listens to your teammates.** No item fires when an *ally's*
   effect lands. RoR2's co-op synergy sits at the edges of the game;
   Comradery needs it at the centre.
6. **A third of the talents are flat numbers** (as the 2026-11-17 entry
   already says). RoR2's lesson: keystones rewrite a rule; they don't add
   a number.

## The four layers (names used below)

Every slotted infusion gives all four at once:

| Layer | What it is | Today | Where it comes from |
| --- | --- | --- | --- |
| **Talent** | the node's own effect | 28 talents, 1 or 2 per tier | the node you slot into |
| **Part effect** | the item's own effect, wherever slotted | flat stat | the item's part (Talon, Eye...) |
| **Part grade** | what the part effect becomes at Refined / Exalted | nothing | the grade of that item |
| **Genus effect** | the genus × tree effect, by grade | 48 effects | the item's genus + the tree |

The key consequence: **your Exalted item is two capstones in one**. An
Exalted Magnarok Eye in Offense tier 4 gives Inferno Heart (genus), Focus
Fire or Bloodlust (talent) *and* the Exalted Eye rule (part). Which part
you combine onto is a build decision. Combining keeps the target's part, so
a Talon you don't want can still go into the grade of the Eye you do want:
that is our version of a RoR2 scrapper and printer.

---

# Part 1: the 18 end states

Chosen to cover every archetype, every genus, every tree, every kit button,
the wide (non-Ascended) path, and at least one build per archetype that
*converts* the archetype's strong trait into team value. Each one names its
RoR2 ancestor, so the engine is a proven one.

How to read the pieces: **Core** is the first infusion. **Deep** is the
tree taken to tier 4 (8 Raw: 1 + 1 + 2 + 4), which carries the capstone.
**Talents** are the node picks that matter. **Parts** says which part the
key items should be. **Shallow** is the rest of a ~15-Raw match.
**Half-built** is the build at about 6 Raw (round 1 to 2). No end state
may be a trap before it is finished: the Benthic Bloom lesson.

New pieces this doc proposes are marked **(new)**; the part rules are in
Part 2.

## Overview

| # | Build | Archetype (best characters) | Capstone (genus · tree) | RoR2 ancestor |
| --- | --- | --- | --- | --- |
| 1 | Permafrost Lockdown | Playmaker (Zephyr, Presidente, Kade) | Frigidariant · Genealogy (Diamond Heart) | Stun Grenade lock + Death Mark |
| 2 | Puppet Master | Playmaker (Nyx, Kade) | Cerebrian · Offense (Puppeteer) | Captain's drone army |
| 3 | Gravity Well | Playmaker (Zephyr, Kade) | Venoxarid · Genealogy (Miasma Heart) | Primordial Cube + Royal Capacitor |
| 4 | Hot Potato | Playmaker (El Presidente; Nolan) | Magnarok · Genealogy (Supernova) | Bungus Engineer (kit-specific value) |
| 5 | Battery | Winger (Isaac) | Cerebrian · Defense (Psychic Link) | N'kuhana's Opinion, routed to an ally |
| 6 | Purifier | Winger (Isaac; Suds) | Venoxarid · Defense (Symbiote) | Void corruption, inverted |
| 7 | Courier | Winger / Playmaker (Isaac, Zephyr) | Venoxarid · Mobility (Swarm Rider) | Perma-Tonic + War Horn |
| 8 | Thornwall | Defender (Volgore, Suds) | Frigidariant · Defense (Cryo Sanctuary) | Razorwire + Hellfire Tincture |
| 9 | Pressure Keg | Defender (Suds) | Magnarok · Defense (Heat Sink) | Transcendence (health becomes a regenerating pool) |
| 10 | Glacier Turret | Striker / Defender (Mina; Suds) | Frigidariant · Offense (Glacier) | Bungus Engineer (stand still) |
| 11 | Haymaker | Striker (Wade, Gravon) | Magnarok · Offense (Inferno Heart) | Kjaro's + Runald's Bands |
| 12 | Glass Comet | Striker (Nolan, Gravon) | Magnarok · Mobility (Firewalker) | Shaped Glass on an i-frame survivor |
| 13 | Momentum | Striker (Wade, Nolan) | Venoxarid · Mobility (Swarm Rider) | Speed Loader |
| 14 | Plague Chain | Striker (Jeff, Mina) | Venoxarid · Offense (Plague Carrier) | AtG + Ukulele, Voidsent Flame |
| 15 | Cauterizer | Striker (Mina, Jeff) | Magnarok · Genealogy (Supernova) | Poison executioner Acrid |
| 16 | Overkill | Striker (Gravon, Nolan) | Cerebrian · Mobility (Phantom) | Crit Flurry Huntress, Railgunner |
| 17 | Core Loop | any (Kade, Nyx, Mina) | any · Genealogy (Overmind shown) | Gesture + Fuel Cells |
| 18 | Chimera | any, never Ascends | none (wide) | Growth Nectar + Wake of Vultures |

The 17 Ascended builds use 15 of the 16 capstones, and two pairs share
one: Courier and Momentum (Swarm Rider), Hot Potato and Cauterizer
(Supernova). **That is the point**: the same capstone with different
talents and parts should play differently. Courier (Tailwind, Sinews) is
selfless; Momentum (Hit and Run, Talons, the Momentum talent) is selfish.
The one capstone no end state wants is **Avalanche** (Frigidariant ·
Mobility), so it is the first candidate to rework (Part 2, C).

---

## Playmakers: crowd control that turns into team damage

### 1. Permafrost Lockdown: "Nothing moves while I'm here."

- **Ancestor:** stun-locking Golems with Stun Grenade + attack speed;
  Death Mark turning debuffs into team damage.
- **Engine:** Slow → Snare → freeze (Icy Strikes), on every LMB. Brittle
  makes everything you stun or root Vulnerable *for the whole team*.
  Diamond Heart freezes everything the core hits. Feedback Loop brings
  the core back sooner.
- **Core:** Frigidariant (Diamond Fortress).
- **Deep:** Genealogy, Frigidariant: Unbreakable → Brittle → Diamond Heart.
  Talents Bloodline, Purebred, Overcharge, **Feedback Loop**.
- **Parts:** Frigidariant **Talons** (Raw: LMB Slows; Refined: Escalate to
  Snare). The Exalted is a **Crown** (Throne: the Fortified it gathers
  radiates to allies).
- **Shallow:** Mobility 1 to 2 (Light Frame, Slipstream).
- **Half-built:** core + 2 Frigidariant Talons + Glacial Path: packs crawl.
- **Team:** Vulnerable on every controlled enemy; frozen targets for a
  Splinter Striker.
- **Weak to:** Berserk and Unbounded elites, the Matriarch. Deals almost
  no damage alone, which is intended: it needs a Striker.

### 2. Puppet Master: "Their army is my army."

- **Ancestor:** Captain and Engineer drone armies; Empathy Cores scaling
  with allies.
- **Engine:** Confuse (Mind Flay, Mass Confusion, Dominate) → confused
  enemies hit their friends, and Puppeteer doubles that and gives you
  their kills.
- **Core:** Cerebrian (Mind Flay).
- **Deep:** Offense, Cerebrian: Psychic Shockwave → Dominate → Puppeteer.
  Talents Killer Instinct (filler), Relentless, **Affliction**, **Focus
  Fire** (your hits make enemies Vulnerable to the team).
- **Parts:** Cerebrian **Eyes**: charged RMB stacks Expose; Exalted Eye
  (Heavy Hand) makes one full charge count as three Dominate hits.
- **Shallow:** Genealogy 1 to 2 with Raw Cerebrian (Mass Confusion).
- **Half-built:** Mind Flay + Mass Confusion: packs brawl each other.
- **Team:** confused enemies don't attack the crew. Kills by puppets give
  you loot only, never score taken from a teammate.
- **Weak to:** few enemies (the boss), Focused enemies. **Clarity risk:**
  which enemies are yours needs one look everywhere.

### 3. Gravity Well: "I set the table."

- **Ancestor:** MUL-T's Primordial Cube + Royal Capacitor: gather the
  pack, then hit all of it.
- **Engine:** the kit clusters (Zephyr's vortex, Kade's mirrors). You load
  one enemy with debuffs; **Contagion** copies them to the whole cluster;
  a teammate's AoE lands on a pre-debuffed pack (Affliction, Rupture,
  Flashpoint all light up).
- **Core:** Venoxarid (Toxic Miasma, a cloud on the cluster).
- **Deep:** Genealogy, Venoxarid: Venom Infusion → Toxic Catalyst →
  Miasma Heart (you are the cloud: drag the cluster through yourself).
  Talents Bloodline, Hybrid Vigor, **Contagion (new)**, Feedback Loop.
- **Parts:** Venoxarid **Eyes** (charged RMB pours poison by charge).
- **Shallow:** Raw Frigidariant and Magnarok Talons, so the load you
  spread holds Slow and Wound as well as Poison.
- **Half-built:** Toxic Miasma on a vortex.
- **Team:** this build exists to multiply someone else's AoE.
- **Weak to:** single targets; spread fractions need tuning.

### 4. Hot Potato: "Catch."

- **Ancestor:** Bungus Engineer: one character makes an item great.
- **Engine:** whoever you Grapple, Snare or knock back carries your
  statuses; when it lands or hits another enemy it **detonates** its
  Wound and Poison on the pack. El Presidente carries, slams and throws,
  so this is his build; Zephyr's shield and Nolan's kicks get a weaker
  version.
- **Core:** Magnarok (Cauterize).
- **Deep:** Genealogy, Magnarok: Cataclysm → Fuel → Supernova. Talents
  Bloodline, Purebred, **Hot Potato (new)**, Apex Mutation.
- **Parts:** Magnarok **Talons** (LMB burns the held foe; Refined
  Escalates to Vulnerable).
- **Half-built:** Cauterize on a slammed pack.
- **Team:** burst into packs; Wound feeds Suds (build 9) and Cauterizer
  (15).
- **Weak to:** nothing to grab (ranged packs, the boss).

## Wingers: healing that becomes everyone else's power

### 5. Battery: "When I heal you, you hit harder."

- **Ancestor:** N'kuhana's Opinion (healing becomes damage), routed to an
  ally, because a Winger's DMG must stay Very Low.
- **Engine:** Crowns gather buffs from what *anyone* does to nearby enemies
  (Slow → Fortified, Wound → Empowered, Expose → Sharpened). **Conduit**
  hands every buff you carry to whoever you heal. Psychic Link hands them
  your barrier too.
- **Core:** Cerebrian.
- **Deep:** Defense, Cerebrian: Mind Barrier → Backlash → Psychic Link.
  Talents Plating, Regrowth, **Conduit (new)**, Share the Load.
- **Parts:** **Crowns** of several genera (each a different buff). Refined
  Crowns also give the buff back to the ally who applied the status.
- **Half-built:** two Raw Crowns + Mind Barrier.
- **Team:** the whole build. "When I win, we win", literally.
- **Weak to:** a split crew. **Clarity risk:** buff icons on allies must
  say who gave them.

### 6. Purifier: "Your curse is my gift."

- **Ancestor:** Void items transforming stacks, used here on effects:
  turn debuffs into their opposites.
- **Engine:** heals **Purify** an ally's debuff (Slow → Haste, Weakened →
  Empowered, Wound → Mend). Corrosion keeps nearby enemies Weakened.
  Symbiote heals the crew when poisoned enemies die near you.
- **Core:** Venoxarid.
- **Deep:** Defense, Venoxarid: Acidic Shell → Corrosion → Symbiote.
  Talents Plating, Second Wind, Guardian, **Purifier (new)**.
- **Parts:** Venoxarid **Hides** (Refined: Bodyguard, they fire when a
  nearby ally is struck).
- **Team:** elites and raids that debuff the crew now feed it.
- **Weak to:** calm rounds. **Depends on enemy content:** enemies must apply
  debuffs for Purify to matter (most do not yet).

### 7. Courier: "Everyone's fast when I'm here."

- **Ancestor:** perma-Spinel Tonic + War Horn: 100% uptime on a timed buff.
- **Engine:** SHIFT Hastes you and nearby allies (Slipstream, Tailwind,
  Refined Sinews). Haste also makes abilities recharge faster, so team
  Haste is team cooldown. Swarm Rider makes you Unbounded and SHIFT
  recharge 50% faster while 5+ poisoned enemies are near.
- **Core:** Venoxarid.
- **Deep:** Mobility, Venoxarid: Speed Siphon → Toxic Wake → Swarm Rider.
  Talents Light Frame, Slipstream, **Tailwind**, Escape Artist.
- **Parts:** Venoxarid **Sinews**; the Exalted one (Reset) gives back SHIFT
  time per enemy its trail touches.
- **Team:** the crew crosses the map faster. **This is the build against
  dead time** (Crunch's key insight), and it helps whoever carries scrap.
- **Weak to:** low kill power; Haste caps.

## Defenders: soak that becomes team safety and team damage

### 8. Thornwall: "Hit me. Please."

- **Ancestor:** Razorwire + Hellfire Tincture + Harvester's Scythe: the
  thorns tank.
- **Engine:** draw hits (**Lightning Rod** taunts; Share the Load takes a
  quarter of allies' damage) → every hit fires every Hide (one Slows, one
  Poisons, one Burns the attacker) and Thorns → Cryo Sanctuary and Last
  Stand keep you up.
- **Core:** Frigidariant (Diamond Fortress).
- **Deep:** Defense, Frigidariant: Arctic Aura → Permafrost → Cryo
  Sanctuary. Talents Plating, Second Wind, **Lightning Rod (new)** or
  Thorns, **Share the Load**.
- **Parts:** **Hides** of every genus. Exalted Hide (Backlash Aura): Hide
  effects hit every enemy within 150, not only the attacker.
- **Team:** aggro off the squishy players; Share the Load's damage also
  fires your Hides.
- **Weak to:** ranged packs (fewer melee attackers), big burst.

### 9. Pressure Keg: "Everyone's armour is my bomb."

- **Ancestor:** Transcendence: health becomes a regenerating pool, and
  the penalty moves somewhere you can manage.
- **Engine:** Suds hands out Guard; Frothy turns lost Guard into Wound
  on enemies; Slow Burn spreads your own damage over time; Heat Sink turns
  the damage you defer into a fire nova and Fortified. **Bastion** makes
  your Guard regenerate and lets extra health raise your Guard cap.
- **Core:** Magnarok (Cauterize explodes all that Wound).
- **Deep:** Defense, Magnarok: Slow Burn → Cauterizing Wounds → Heat Sink.
  Talents Plating, Regrowth, Guardian, **Bastion (new)**.
- **Parts:** Cerebrian **Hides** in the shallow slots (Guard when struck).
- **Team:** less damage taken by everyone; enemies hitting an ally's Guard
  feed your Cauterize.
- **Weak to:** burst through Guard; enemies that cleanse.

### 10. Glacier Turret: "Hold here."

- **Ancestor:** Bungus Engineer: standing still is the build.
- **Engine:** stand still for half a second: Glacier (Fortified, hits
  freeze at once) → Splinter bursts frozen targets → Permafrost slows
  anything close.
- **Core:** Frigidariant.
- **Deep:** Offense, Frigidariant: Icy Strikes → Splinter → Glacier.
  Talents Killer Instinct, Opening Strike, Executioner, Focus Fire.
- **Parts:** Frigidariant **Eyes**: Mina charges while planted anyway.
- **Team:** anchors the ship against raids, next to the roof turrets.
- **Crunch:** the clock makes you move; "hold the ship or go out" is the
  trade-off, by design.
- **Archetype conflict:** on a Striker, freezing is crowd control, and
  Strikers' CC must stay Very Low. See "Archetype maxima" in Part 2.

## Strikers: damage that comes online off a teammate's setup

### 11. Haymaker: "One punch, whole pack."

- **Ancestor:** Kjaro's + Runald's Bands: hits over 400% trigger huge
  effects, so slow, heavy survivors (Loader) live on them.
- **Engine:** **Exalted Eye (Heavy Hand)**: a full-charge RMB counts as
  three LMB hits for every on-hit effect. Three Magnarok Talon burns in one
  punch reach Flashpoint at once; the pack detonates.
- **Core:** Magnarok.
- **Deep:** Offense, Magnarok: Eruptive Strikes → Flashpoint → Inferno
  Heart. Talents Killer Instinct, **Opening Strike**, Executioner,
  Bloodlust.
- **Parts:** Magnarok **Talons** in tiers 1 to 2; the **Exalted is an
  Eye**.
- **Half-built:** Talons + Eruptive Strikes: Wade's three-punch combination
  already reaches Flashpoint.
- **Weak to:** whiffs. A missed charge is nothing: skill shows.

### 12. Glass Comet: "Don't get hit."

- **Ancestor:** Shaped Glass: best on characters with dodges and
  i-frames ("it doesn't matter if your hp is 1 if you don't get hit").
- **Engine:** **Overclock**: always Vulnerable, but Empowered caps higher
  and you start each fight Empowered. Magnarok Crowns Empower you whenever
  enemies near you burn (from anyone). Firewalker keeps you in Burning Rush
  with 3+ burning enemies around.
- **Core:** Magnarok.
- **Deep:** Mobility, Magnarok: Burning Rush → Eruption Dash → Firewalker.
  Talents Light Frame, Quick Recovery, Hit and Run, Blitz.
- **Shallow:** Offense 1 to 2 with **Overclock (new, tier 2)**: a risk you
  can take early, like a Lunar.
- **Parts:** Magnarok **Crowns** and **Sinews**.
- **Team:** depends on a Winger (Battery or Purifier), which is the
  Comradery hook: the glass cannon needs a friend.
- **Weak to:** one-shots. The downside is visible and you can play around
  it, which is what made Shaped Glass loved and Brittle Crown hated.

### 13. Momentum: "Speed is damage."

- **Ancestor:** Speed Loader: movement speed becomes punch damage.
- **Engine:** Wade's third punch Hastes him; Speed Siphon adds speed per
  poisoned enemy; **Momentum** turns speed above base into damage.
- **Core:** Venoxarid.
- **Deep:** Mobility, Venoxarid (Courier's tree): Speed Siphon → Toxic
  Wake → Swarm Rider. Talents Light Frame, Slipstream, Hit and Run,
  **Momentum (new, tier 4 option)**.
- **Parts:** Venoxarid **Talons** (Poison on LMB feeds Speed Siphon).
- **Pair:** the same capstone as Courier. Courier hands out speed;
  Momentum spends it.

### 14. Plague Chain: "It spreads."

- **Ancestor:** AtG + Ukulele proc chains; Voidsent Flame chains through
  full-health packs.
- **Engine:** **Exalted Talon (Echo)** splashes every LMB's Poison to
  enemies around the target → Rupture pops at 10 stacks onto neighbours →
  Plague Carrier hands a dead enemy's stacks to the two nearest. Jeff's
  beam hits often; his biomass vacuum turns the crew's kills into fuel.
- **Core:** Venoxarid.
- **Deep:** Offense, Venoxarid: Toxic Blade → Rupture → Plague Carrier.
  Talents Killer Instinct, Relentless, **Affliction**, Bloodlust.
- **Parts:** Venoxarid **Talons**; the Exalted is a Talon.
- **Weak to:** bosses (no pack to chain through).
- **Needs:** the proc rule (Part 2). This is the build that lags RoR2 and
  needs the visual budget.

### 15. Cauterizer: "Let it all go off at once."

- **Ancestor:** the poison-executioner Acrid: damage over time takes
  elites under the execute line, and on-kill effects chain.
- **Engine:** mixed genera load every kind of damage over time (Poison
  from Venoxarid, Wound from Magnarok, Suds's Frothy) → the Magnarok core,
  Cauterize, explodes all of it, scaled by the time left → Executioner
  finishes. Every third cast is a Supernova; Feedback Loop brings the core
  back.
- **Core:** Magnarok.
- **Deep:** Genealogy, Magnarok: Cataclysm → Fuel → Supernova. Talents
  Bloodline, **Hybrid Vigor**, Overcharge, Feedback Loop.
- **Shallow:** Offense 1 to 3 with Venoxarid (Refined Rupture in tier 3,
  Executioner).
- **Parts:** **Eyes** (the part stat recharges abilities).
- **Proves:** a mixed build can carry a capstone; the purebred route is not
  the only one.

### 16. Overkill: "Every hit is a crit, then more."

- **Ancestor:** Crit Huntress (9 Lens-Maker's + Scythe) and Railgunner
  (crit chance converted to crit damage).
- **Engine:** Cerebrian Crowns give you Sharpened whenever enemies near you
  are Exposed (Nolan stacks Expose himself). Nyx's Focused gives an ally
  Sharpened every second. At 100% crit, **Overkill** turns the extra into
  crit damage, so hitting the cap is a fork, not a wall. Phantom deals
  contact damage while you weave through the pack.
- **Core:** Cerebrian.
- **Deep:** Mobility, Cerebrian: Phase Shift → Ghost Step → Phantom.
  Talents Light Frame, Quick Recovery, Hit and Run, Blitz.
- **Shallow:** Offense 1 to 2 with Killer Instinct and **Overkill (new,
  tier 2 option)**.
- **Parts:** Cerebrian **Crowns**; Gravon's charged RMB already crits, so
  an Exalted Eye suits him.
- **Team:** Nyx and Overkill are a pair: her Focused is his crit chance.

## Any archetype

### 17. Core Loop: "It's always up."

- **Ancestor:** Gesture of the Drowned + Fuel Cells: equipment fires
  itself on a short cooldown. Our equipment is the core ability (E).
- **Engine:** Bloodline, Feedback Loop, Eye parts (recharge) and Haste
  shorten the core's cooldown; an Exalted Genealogy effect makes every cast
  huge (Overmind, Diamond Heart, Supernova, Miasma Heart).
- **Core / Deep:** whatever genus you're dealt, Genealogy. Shown with
  Cerebrian: Mass Confusion → Mind Echo → Overmind.
- **Parts:** **Eyes** everywhere.
- **Guard rail:** the core's cooldown has a floor (first guess 3 s), so
  kills → cooldown → kills can't run away (the Soulbound + Forgive Me
  loop).

### 18. Chimera: "A bit of everything, all of it working."

- **Ancestor:** Growth Nectar + Wake of Vultures (power per distinct buff),
  and the general RoR2 truth that a wide pile of whites wins runs.
- **Engine:** never Ascend. All four trees to tier 2, all Raw, all four
  genera. **Hybrid Vigor (reworked)** counts distinct buffs on you as well as
  distinct genera. Crowns of all four genera gather four different buffs
  from whatever the crew is doing.
- **Why it must exist:** if deep always beats wide, Ascension is the one
  dominant build (Creative Builds). It is also the build that makes a late
  lucky drop of any genus worth picking up.
- **Weak to:** no capstone moment. Reliable, with a lower ceiling.

## The builds as a team

Most of these are halves. The pillar is "when I win, we win", so the
strongest moments should need two players:

| Setup | Payoff | What links them |
| --- | --- | --- |
| Permafrost Lockdown | Glass Comet, Haymaker | Brittle's Vulnerable; frozen targets |
| Gravity Well | Plague Chain, Cauterizer | a clustered, pre-debuffed pack |
| Puppet Master | Thornwall | puppets and the taunt split the horde |
| Battery | Overkill, Haymaker | buffs handed over on each heal |
| Purifier | Glass Comet | a Vulnerable cannon kept alive |
| Courier | everyone | Haste is speed and cooldown |
| Pressure Keg | Cauterizer, Hot Potato | Wound piles that someone detonates |
| Nyx (any build) | Overkill | Focused is crit chance |

---

# Part 2: working back, what each layer must supply

## A. Parts: each part is a kit button

The builds need items that hook into the five-button kit. Proposal: each
part **is** a button, and its effect is the genus delivered through it.

| Part | Hooks into | Raw effect (genus delivers) | Stat line (kept, small) |
| --- | --- | --- | --- |
| **Talon** | LMB hits | each hit applies 1 genus status | +damage |
| **Eye** | RMB charge | release applies the status, stacks = charge level; +10% charge speed | +recharge |
| **Sinew** | SHIFT | SHIFT applies the status in a burst where you leave | +speed |
| **Hide** | being struck | the attacker gets the status | +health |
| **Crown (new part)** | anyone's effects | when an enemy within 300 gets the status **from anyone**, you gain the genus buff (1 stack, max 5) | none |

Q stays the kit's own: it is "iconic, wide variations", and the talents and
genus effects already reach it through the statuses it applies. Defenders'
SHIFT is a second unique, and Sinew still fires on it: the hook is "SHIFT
was used", not "dashed".

**The genus palette** (what each genus delivers):

| Genus | Status (Raw parts) | Escalates to (Refined Talon) | Buff (Crown) | Meta verb |
| --- | --- | --- | --- | --- |
| Venoxarid | Poison | Weakened | Haste | Contaminate (copy debuffs) |
| Cerebrian | Expose | Confused (0.75 s) | Sharpened | Radiate (copy buffs), Corrupt / Purify |
| Frigidariant | Slow | Snared (0.5 s) | Fortified | Doomed / Anchored (locks effects in) |
| Magnarok | Wound (burn) | Vulnerable | Empowered | Double, Detonate |

All of these are already in `EffectType` and the effect verbs
(`amplifyEffect`, `spreadEffect`, `detonateEffect`, `corruptBuffs`,
`purifyDebuffs`). Magnarok burns are already Wound, which is why Suds's
Frothy and Magnarok already combine.

**Differs from the effects spec example:** its "Iceborn Crown" gives
Empowered when enemies are Slowed. Here the Frigidariant buff is Fortified
and Empowered is Magnarok's, so each genus owns one buff and a Crown's
colour tells you what it gives (Clarity).

20 items (4 genera × 5 parts) instead of 16. The genus follows the
territory, so the player steers genus by where they go (that costs time:
Crunch). The part is the random roll (1 in 5), and combining turns an
unwanted part into the grade of a wanted one. Steering with real
randomness: the Artifact of Command lesson.

## B. Part grades: one rule per part, the same for every genus

To keep this learnable, each part has **one** Refined and **one** Exalted
rule, and the genus only changes the status. Refined **reaches**: allies,
or more enemies. Exalted **breaks a rule**.

| Part | Refined | Exalted (rule break) |
| --- | --- | --- |
| Talon | **Escalate**: every 4th hit on the same target applies the genus's escalation status | **Echo**: LMB status applications also land on every enemy within 90 of the target (Ukulele) |
| Eye | **Overcharge**: hold up to 1 s past full charge; an overcharged release applies double | **Heavy Hand**: a full-charge RMB counts as 3 LMB hits for every on-hit effect (the Bands) |
| Sinew | **Wake**: allies you pass during or just after SHIFT gain the genus buff | **Reset**: SHIFT recharges 1 s per enemy its effect touches (up to half its cooldown) |
| Hide | **Bodyguard**: also fires when an ally within 250 is struck | **Backlash Aura**: fires on every enemy within 150, not just the attacker |
| Crown | **Share**: the ally who applied the status also gets the buff | **Throne**: cap 10, and your buff radiates to allies within 250 every 2 s |

Raw part effects stack per copy, like today's stat lines. Refined and
Exalted part rules don't stack; you have them or you don't, like the genus
grades. The Refined rules are where Playmakers and Wingers stop
getting dead items: an Empowered Crown is useless on Kade, but a Refined
one hands Empowered to the Striker who applied the Wound.

## C. Genus effects: keep, re-speak, rework one

- **Keep** the 48 effects. Every genus × tree except Frigidariant ·
  Mobility anchors an end state.
- **Re-speak them in the shared vocabulary** wherever there's a choice:
  a "freeze" should be Stunned (so Berserk counters it and Brittle reads
  it), "poisoned enemies deal less" should be Weakened, and so on. Then
  kits, talents and other players' items can read them.
- **Rework Avalanche** (Frigidariant · Mobility), which no end state wants:
  a moving freeze is a Playmaker tool with nobody to deliver it.
  Candidate: "SHIFT through enemies Snares them; Snared enemies you pass
  are Stunned". That gives Zephyr a second Frigidariant end state.

## D. Talents: converters and keystones, not numbers

The tier-1 legs (Killer Instinct, Plating, Light Frame, Bloodline) can stay
flat: they are the white items, cheap filler. Every other tier should hold
rule-changers. New talents the end states need (numbers are first
guesses; "option" means a new choice at that tier, which the layout
already supports):

| Talent | Tree · tier | Effect | Builds |
| --- | --- | --- | --- |
| **Overclock** | Offense · 2 option | You are always Vulnerable ×3. Empowered caps 3 higher, and you start each fight Empowered ×2 | 12 |
| **Overkill** | Offense · 2 option | Crit chance over 100% becomes crit damage (1:1); each Sharpened stack over the cap adds 10% crit damage | 16 |
| **Contagion** | Genealogy · 3 option | When an enemy you debuffed dies, its debuffs copy to enemies within 150 (half stacks) | 3 |
| **Hot Potato** | Genealogy · 3 option | An enemy you Grapple, Snare or knock back detonates its Wound and Poison when it lands or hits another enemy | 4 |
| **Conduit** | Defense · 3 option | Your heals also give the healed ally 1 stack of every buff you carry | 5 |
| **Lightning Rod** | Defense · 3 option | SHIFT Marks you for 3 s: enemies within 300 attack you; Fortified ×2 while Marked | 8 |
| **Purifier** | Defense · 4 option | Your heals Purify one debuff on the ally (into its opposite buff) | 6 |
| **Bastion** | Defense · 4 option | Your Guard regenerates (1 stack per 2 s, up to 3); every 25 max health above base raises your Guard cap by 1 | 9 |
| **Momentum** | Mobility · 4 option | +1% damage per 1% move speed above base (up to +40%) | 13 |
| **Hybrid Vigor** (rework) | Genealogy · 2 | Also counts each distinct buff on you (up to 6) | 18 |

Five of the nine are team converters (Contagion, Conduit, Lightning Rod,
Purifier, Bastion). Two are deliberate risk items (Overclock, Lightning
Rod), whose downside you can see and play around.

## E. Rules the builds need

1. **Archetype maxima apply to infusions, too.** `ARCHETYPES.md` compares
   each trait across "base kit plus synergy with infusions". The builds
   above break that in places: Glacier freezes on a Striker, Momentum or
   Talons give a Playmaker damage, Symbiote heals on a Defender. The
   effects spec already says intensity "varies per character". Proposal:
   **scale what an infusion produces by the archetype's maximum for that
   trait**:

   | Archetype max | Very High | High | Mid | Low | Very Low |
   | --- | --- | --- | --- | --- | --- |
   | Multiplier (guess) | ×1.25 | ×1.0 | ×0.75 | ×0.5 | ×0.25 |

   It applies per trait: damage, healing, CC duration, damage reduction,
   mobility. A Frigidariant Talon then roots for 0.6 s on Zephyr and
   0.12 s on Wade. This does automatically what RoR2 does by hand ("the
   same item means different things on different survivors"), and it
   keeps the "no Playmaker out-heals a Winger" promise. The tooltip
   always shows *your* number. **A big trade-off: see question 1.**
2. **Proc depth is 1, and the tooltip says so.** An effect caused by an
   infusion reaction (Echo splashes, Crown buffs, Contagion copies, Plague
   Carrier hand-offs, detonations) is an **echo**: echoes never trigger
   Talons, Hides, Echo or Contagion again. Crowns listen to everything but
   only grant buffs, so they cannot loop. RoR2 was still patching an
   infinite Runic Lens + AtG chain in 2026; the code already skips procs
   and damage over time for on-hit (`AbilitySystem.h`), so this extends a
   rule that exists.
3. **Cooldown floors.** The core and SHIFT never go below a floor (first
   guess: core 3 s, SHIFT 1 s), whatever Feedback Loop, Reset, Haste and
   Eyes add up to.
4. **A visual budget per player.** Each player's echoes merge into one
   visible pulse per 0.25 s (Clarity; "AtG lags the game hard").
5. **Pacing.** Each end state fits ~15 Raw (core 1 + deep tree 8 +
   shallow 6) and is already doing its thing at ~6 Raw. Measure with
   `ArenaSim` once the bots can be given a build to aim for.

## F. Suggested order of work

1. **Parts become buttons (Raw only)** for Talon, Eye, Sinew and Hide, plus
   the proc-depth rule. This makes every item do something with every kit.
   It is the biggest change to how infusions feel and needs no new UI.
2. **Archetype multipliers** (if approved), shown in the tooltips.
3. **Crown** and the ten part-grade rules (Refined Share is the Comradery
   payoff).
4. **The nine talents**, converters first (Conduit, Contagion, Purifier).
5. **Build presets for testing**: a debug key that gives a player one end
   state's exact pieces, so each of the 18 can be played in minutes and
   the sims can measure them. That is how we find the dominant one before
   players do.

## Open questions for a human

1. **Archetype multipliers on infusion output** (E.1): yes or no? It
   changes every infusion number per character, and it is the main answer
   to "damage items are dead on Playmakers and Wingers". Without it,
   several end states break the archetype maxima.
2. **A fifth part, Crown** (20 items instead of 16): worth the extra pool
   dilution for the "listens to your teammates" layer?
3. Is **this list of 18** right? Which would you cut or add? Specifically:
   all three Winger builds ride on Isaac, the only Winger with a kit.
4. **Frigidariant's buff:** Fortified (this doc) or Empowered (the effects
   spec's Iceborn Crown)?
5. Should **Raw part effects stack** per copy (three Frigidariant Talons =
   3 Slow per hit), or should only the highest copy count?
