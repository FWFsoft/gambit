# STATUS

**ACCEPTED** (issue #44; spec approved by a human on 2026-11-23, with the
defaults for the four open questions at the end). **Step 1 built**: grades,
combining, depth requirements, attunement, Ascension and the suit screen.
**Step 2 not built**: the 32 Refined and Exalted effects exist as text
only; the suit screen marks them "not in the game yet". Extends
`specs/genera-and-infusion-system.md`.

# Infusion grades: combine, go deep, become something

## The problem

Playtest, 2026-11-22: upgrading through infusions is too fast. Every
infusion fills exactly one slot, so by round 3 a lucky player has most of
the suit, and every build is open for the whole match. The genus effects
only get bigger with more copies (level 1, 2, 3...), so going deeper never
changes *what* you do.

The aim (from the issue):

- Deeper slots need a higher-grade infusion.
- You make higher grades by combining infusions.
- A higher grade's genus effect is a **different effect**, not a bigger
  number.
- Going down a track **locks out** earlier options, so you become
  something as the match goes on.

**Words.** The issue says "tier" for both the item and the tree row. The
code already uses *tier* for a row of the tree (tiers 1 to 4). To keep
these apart, this spec calls the item's level its **grade**:

| Grade | Name | Pip | Made from |
| --- | --- | --- | --- |
| I | Raw | one dot | a drop |
| II | Refined | two dots | 2 Raw of one genus |
| III | Exalted | three dots | 2 Refined of one genus (4 Raw) |

## 1. Combining

- **Recipe: 2 → 1.** Two infusions of the **same genus and same grade**
  combine into one of the next grade. Exalted is the top.
- **Where:** in the suit screen, at the ship, during the night (the same
  place and time as slotting). Not in the field.
- **Which part survives:** the result keeps the part of the item you
  dragged *onto* (the target); the other is used up. Two Talons make a
  Talon; a Talon dropped on a Hide makes a Hide.
- **Part stat counts what went in.** A part's stat is multiplied by the
  number of Raw infusions inside it: Raw ×1, Refined ×2, Exalted ×4. So
  combining never loses stats overall; it only moves them onto one part
  and into fewer slots.
- **Permanent.** Nothing ever splits back.

Why 2 and not 3: a match hands a player roughly 12 to 18 infusions
(estimate, below). At 3 → 1 an Exalted would cost 9 Raw, more than half a
match, and almost nobody would see a capstone. At 2 → 1 it costs 4.

### The trade-off this creates (Strategic Crunch)

Every pair is a choice between **wide** and **deep**:

- Two Raw spent wide: two slots, two talents, two genus effects, and they
  can go in different trees or genera.
- Two Raw combined: one slot, one talent, but the slot can be in tier 3,
  and it carries a grade II effect.

A player with 9 Raw (a decent two-round haul) can have, for example:

- Wide: core + tiers 1 and 2 of all four trees (8 talents, all Raw);
- Deep: core + Offense 1, 2, 3 + Defense 1, 2 (5 talents, one Refined);
- or something between.

## 2. What each depth needs

| Node | Needs |
| --- | --- |
| Core | any grade (a higher grade gives nothing extra in the core) |
| Tree tier 1 | any grade |
| Tree tier 2 | any grade |
| Tree tier 3 | Refined or better |
| Tree tier 4 | Exalted |

A full tree now costs 1 + 1 + 2 + 4 = **8 Raw** (was 4), and the deep half
of it must be one genus. A higher grade may go in a shallow node: it still
gives its own grade's effects there (you paid for them).

The round depth cap (`SUIT_TIER_CAP`: tiers 2 / 3 / 4 by round) stays for
the first build. With grades it should rarely bind; if the sims show it
never does, remove it (one less rule to explain).

## 3. Genus effects by grade

An infusion slotted in a tree gives its genus's effect for that tree **for
its own grade and every grade below it**: a Refined Venoxarid in Offense
gives Toxic Blade *and* Rupture.

The Raw effect keeps its current level rule: each infusion of that genus in
that tree adds a level, whatever its grade (Apex Mutation still adds one
for the core's genus). The Refined and Exalted effects do not level up;
you have them or you don't.

The pattern for every genus:

- **Raw** is today's effect (from the Genera and Infusion doc). It
  *builds* something: stacks, a status, a shield, a trail.
- **Refined** *pays off* what Raw builds, in a new way: a burst, a
  conversion, a team effect.
- **Exalted** *changes how you play*: a standing state or new behaviour you
  plan the rest of the match around. This is the "you become something".

Numbers are first guesses, like the rest of the infusion numbers.

### Venoxarid (poison)

| Tree | Raw (today) | Refined | Exalted |
| --- | --- | --- | --- |
| Offense | **Toxic Blade**: attacks poison and splash venom. | **Rupture**: hitting an enemy at its poison cap pops all its stacks for 6 damage each, and puts half of them on enemies within 90. | **Plague Carrier**: a poisoned enemy that dies passes its full poison stacks to the 2 nearest enemies within 250. Kills chain through a pack. |
| Defense | **Acidic Shell**: melee attackers are poisoned; poisoned enemies hit you softer. | **Corrosion**: poisoned enemies within 200 of you are Weakened (deal less damage to **everyone**, not just you). | **Symbiote**: a poisoned enemy that dies within 200 of you heals you and allies within 200 for 2% of max health. |
| Mobility | **Speed Siphon**: move faster per poisoned enemy nearby. | **Toxic Wake**: passing within 60 of an enemy poisons it (once per enemy per second). You poison by running past. | **Swarm Rider**: with 5+ poisoned enemies within 300 you are Unbounded (cannot be slowed or rooted) and your SHIFT recharges 50% faster. |
| Genealogy | **Venom Infusion**: core damage poisons; raises the poison cap. | **Toxic Catalyst**: core damage is doubled on enemies with 3+ poison stacks. | **Miasma Heart**: you always exude a small poison cloud (radius 90, 1 stack a second). You are the plague. |

### Cerebrian (psychic, confusion, shields)

| Tree | Raw (today) | Refined | Exalted |
| --- | --- | --- | --- |
| Offense | **Psychic Shockwave**: hits emit a small psychic burst. | **Dominate**: every 6th hit on the same enemy Confuses it for 2 s. Offense now controls. | **Puppeteer**: enemies you Confused deal double damage to other enemies, and their kills count as yours for loot. |
| Defense | **Mind Barrier**: a recharging shield; CC immune while it holds. | **Backlash**: when the barrier breaks, enemies within 150 are Stunned for 1 s. | **Psychic Link**: when your barrier recharges, allies within 250 get a barrier of half its strength. |
| Mobility | **Phase Shift**: dashing damages enemies along the path. | **Ghost Step**: after a dash you are Stealthed for 1.5 s (enemies lose track of you). | **Phantom**: you are always ghosted: you walk through enemies, and touching one deals psychic damage (once per enemy per 0.5 s). This is the doc's original Phase Shift fantasy. |
| Genealogy | **Mass Confusion**: core and psychic damage briefly confuse. | **Mind Echo**: 1.5 s after your core ability damages enemies, a psychic echo hits the same enemies for 40% of it. | **Overmind**: 30% of damage dealt to an enemy you Confused is also dealt to every other enemy you Confused within 400. |

### Frigidariant (ice: slow, root, freeze, shields)

| Tree | Raw (today) | Refined | Exalted |
| --- | --- | --- | --- |
| Offense | **Icy Strikes**: hits escalate slow → root → freeze. | **Splinter**: hitting a frozen enemy ends the freeze and bursts for 12% of its max health (cap 60) to enemies within 80. | **Glacier**: after standing still for 0.5 s you are Fortified (take 40% less) and your hits freeze at once (skip slow and root) until you move. You become a turret. |
| Defense | **Arctic Aura**: melee attackers escalate slow → root → freeze. | **Permafrost**: enemies within 120 of you are always Slowed, so a melee attacker's first hit roots it. | **Cryo Sanctuary**: when you are hit below 50% health, an ice dome (radius 160, 5 s) forms around you: enemies entering are rooted, allies inside take 20% less damage. Once every 25 s. |
| Mobility | **Glacial Path**: leave an ice trail that slows. | **Ice Skate**: on your own ice you are Unbounded and 20% faster. | **Avalanche**: moving for 2 s straight builds momentum; your next hit freezes the target and hits every enemy in a 90° cone in front of you. |
| Genealogy | **Unbreakable**: hard CC grants a shield. | **Brittle**: enemies you stun or root are Vulnerable for the duration (the whole team hits them harder). | **Diamond Heart**: your core ability freezes everything it damages for 1.2 s, and your shields do not expire while a frozen enemy is within 300. |

### Magnarok (fire, burns, molten ground)

| Tree | Raw (today) | Refined | Exalted |
| --- | --- | --- | --- |
| Offense | **Eruptive Strikes**: hits ignite; burns add up. | **Flashpoint**: a burn that reaches 10 s left detonates: the rest of the burn is dealt at once to enemies within 70. | **Inferno Heart**: your hits leave molten ground under the target (one patch per 0.5 s), and you are Empowered while you stand on molten ground. A walking volcano. |
| Defense | **Slow Burn**: part of each hit is dealt to you over time instead. | **Cauterizing Wounds**: killing a burning enemy clears 30% of your deferred damage. | **Heat Sink**: damage you defer builds Heat; at 100 Heat you release a fire nova (40 damage, radius 200) and are Fortified for 3 s. |
| Mobility | **Burning Rush**: every 12 kills, a burst of speed and a molten trail. | **Eruption Dash**: your dash or blink ends in a molten burst (radius 90) where you land. | **Firewalker**: while 3+ burning enemies are within 300, you are always in Burning Rush. |
| Genealogy | **Cataclysm**: core damage leaves molten ground. | **Fuel**: your core ability ignites everything it damages (4 s burn). | **Supernova**: every third core cast has double radius and leaves a molten field (radius 200, 6 s). |

### Checks on the table

- **Different in kind.** No Refined or Exalted effect is a bigger copy of
  the one below it; each one adds a new trigger or a new state.
- **Combines across genera.** Brittle and Corrosion help any teammate's
  build; Fuel feeds a Magnarok core's Cauterize; Toxic Wake feeds Speed
  Siphon and Swarm Rider; Splinter and Diamond Heart want freezes from
  anywhere (a Cerebrian Backlash stun counts).
- **Team effects** (Comradery): Corrosion, Symbiote, Psychic Link, Brittle,
  Cryo Sanctuary. None can hurt or block an ally.
- **Overlaps to watch:** Cryo Sanctuary and the Defense tier-4 talent Last
  Stand both answer low health; they stack, which is fine, but they should
  not fire on the same hit with two big effects (Last Stand first). Plague
  Carrier and Symbiote both key on a poisoned enemy dying; that is a
  deliberate Venoxarid synergy.

## 4. Lock-out rules

Three rules, each tied to a moment the player chooses:

1. **Combining is permanent.** Two flexible items become one committed
   item. The pack gets narrower as you go deep.
2. **Attunement (Refined).** The first Refined-or-better infusion slotted
   into a tree **attunes** that tree to its genus. From then on the tree
   only takes that genus. Infusions of other genera already in it stay and
   keep working (nothing is ever overwritten, as decided 2026-11-17). Since
   tiers 3 and 4 need Refined and Exalted, a tree's deep half is always one
   genus.
3. **Ascension (Exalted).** The first Exalted infusion you slot (anywhere)
   makes you that genus's **Ascendant** ("Venoxarid Ascendant") for the
   rest of the match. After that:
   - Exalted infusions of other genera cannot be slotted;
   - you cannot combine Refined into Exalted of another genus.
   You get **one capstone genus per match**. Your title shows on the
   scoreboard and over your health bar during the night.

What is *not* locked: Raw infusions of any genus can still go in tiers 1
and 2 of an un-attuned tree, so a late lucky drop is never wasted.

Why these and not stricter ones (Creative Builds): locking the whole suit
to one genus at the first Refined would kill the mixed builds the
2026-11-17 decision opened (Hybrid Vigor vs Purebred). Attunement locks a
tree, Ascension locks the capstone; the shallow suit stays a place to mix.

## 5. Pacing estimate

Not measured: the native build needs the vcpkg packages, which were not
available when this was written. From the drop code
(`MatchDirector::dropLoot`, `onObjectiveCompleted`):

- pity guarantees each player one infusion early every round;
- elites always drop one; thug kills drop at 16%;
- objectives pay 1, far ones 2; the round MVP gets one more.

That suggests 4 to 6 per player per round, **12 to 18 per match** (3
rounds before the boss). Today that fills 13 to 17 of the 17 slots. With
grades, 15 Raw buys for example: core + one full tree (8) + tiers 1 and 2
of three more trees (6). Or two trees to tier 3 and the rest wide. One
capstone per match is reachable by a focused player and not by everyone.

**Must measure before tuning:** run `ArenaSim` with bots that combine
(see Build notes) and log, per player per match: infusions collected,
slots filled, deepest tier, grade counts, and whether anyone ascended.
Targets: average slots filled 8 to 11 (was ~13+), most players reach tier
3 somewhere by the boss, about one player in four ascends.

## 6. Suit screen (Clarity)

- **Grade pips** on every item and slot: one, two or three dots under the
  genus icon. The same pips everywhere (pack, tooltip, node, pickup
  banner).
- **Combining:** drag an item onto another of the same genus and grade in
  the pack (or select one and press C). Valid targets pulse while you
  drag. The tooltip shows the result first: "Refined Venoxarid Talon:
  Talon ×2, adds Rupture in Offense...".
- **Node requirements:** an empty tier-3 node shows two hollow pips, tier 4
  three. An item that is too low a grade does not light the node.
- **Attuned trees** get a ring of the genus colour around their tier-3 and
  tier-4 nodes; hovering a blocked node says why ("Offense is attuned to
  Magnarok").
- **Ascension** gets a full-screen moment when it happens (genus colour
  flash, "VENOXARID ASCENDANT"), and a confirm step before the slot
  ("Ascend as Venoxarid? Other genera can never reach Exalted this
  match").
- **Tooltip for a slotted item** lists every effect its grade gives, with
  the Raw level.

## 7. Build notes (for whoever implements this)

- `Player`: inventory grows a grade axis
  (`infusionInventory[genus][part][grade]`); add `nodeGrade[]`,
  `coreGrade`, `ascendedGenus`; derive tree attunement in `recount`.
- `ArenaData`: `SuitNode` gains `minGrade` (or derive it from tier:
  1, 1, 2, 3); `nodeState` takes the item's grade and genus and the
  tree's attunement, with new states for "needs a higher grade" and
  "attuned to another genus" so the HUD can say why.
  `InfusionText` becomes per grade (`infusionText(genus, tree, grade)`).
- `PickupId` stays Raw-only (drops are always Raw).
- Network: `InfusionSlotPacket` gains the grade; new
  `InfusionCombinePacket {genus, grade, targetPart, otherPart}`; the
  state packet's inventory grows to 4 × 4 × 3 bytes, plus `nodeGrade` and
  `ascendedGenus`.
- `Infusions.cpp`: `combineInfusions(player, genus, grade, targetPart,
  otherPart)`; `slotInfusion` checks grade, attunement and ascension;
  the 32 new effects hook into the existing callbacks (`onHitEnemy`,
  `onPlayerDamaged`, `onKill`, `onDash`, `tick`, `castCore`). New state
  goes in `infusionTimer` / `infusionCounter` with a grade axis or new
  per-effect fields.
- Debug: F6 should give one Raw, one Refined and one Exalted of every
  genus.
- `ArenaSim` bots: combine a pair when they hold two of the same genus and
  grade and the deepest open node in a tree they are building needs the
  next grade.
- Tests in `test_infusions`: recipe and part rule, grade gating per tier,
  attunement, ascension blocking slot and combine, stats scale ×1/×2/×4,
  and one test per new effect.
- A big change; it is worth building in two steps: (1) grades, combining,
  gating and the lock-outs with the Refined/Exalted effects stubbed as
  text; (2) the 32 effects, a genus at a time.

## Open questions for a human

Answered on 2026-11-23 by accepting the spec as written: 2 → 1, no core
grade, one capstone genus per match, keep the round cap. Revisit after a
playtest.

1. Is 2 → 1 the right recipe, or should Exalted cost more (e.g. 3
   Refined)? This spec chose 2 → 1 so a capstone is reachable at all.
2. Should the core take a grade (a Refined core strengthening the core
   ability)? This spec says no, to keep the core simple.
3. One capstone genus per match (Ascension): too strict, or not strict
   enough?
4. Keep the round depth cap with grades, or let grades do the pacing?
