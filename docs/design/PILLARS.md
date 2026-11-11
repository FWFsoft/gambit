# Alien Arena design pillars

Source of truth: the "Design Pillars" and "PHILOSOPHY" docs in the team
Google Drive. The pillar statements and example applications below are
copied from there word for word. The "In practice" parts are how agents and
contributors apply them to day-to-day decisions. If the two ever disagree,
the Drive wording wins: update this file to match.

Every gameplay, balance, UI, content or art decision should be made with
these four pillars in mind, including the small ones nobody will review.

---

## 1. Strategic Crunch

> Every decision must carry weight and urgency. Design mechanics that
> encourage players to make swift yet thoughtful choices, maintaining a
> balance between strategy and action.
>
> *Example application:* Implement scenarios where players face multiple
> choices under time constraints, ensuring that each choice has meaningful
> consequences on the game's outcome. Use countdown timers, limited
> resources, and escalating challenges to sustain pressure.

**In practice**

- A choice with an obvious right answer is not a choice. Prefer trade-offs
  (one more fight vs. head back to the ship, carry scrap vs. keep both hands
  free) over flat upgrades.
- Pressure comes from the clock and from scarcity. Before adding something
  that removes pressure (free healing, unlimited resources, longer timers),
  ask what replaces it.
- **Key insight:** time is scarce, so dead time feels very bad. Running
  through explored, empty areas just to get from A to B should never be
  required. Fix it with travel options or by putting something worth
  stopping for along the way.
- Escalation matters: later rounds should demand more than earlier ones.

## 2. Comradery in Competition

> Cultivate a cooperative yet competitive environment where players push
> each other to excel. Encourage teamwork while also providing
> opportunities for players to showcase their individual skills. (Think of
> the dynamic between Gimli and Legolas.)
>
> *Example application:* Avoid mechanics where players can *deny* the fun of
> others, but support metrics and group-oriented rewards based on
> individual performances. "When I win, we win."

**In practice**

- Never add a way for one player to grief or block another (stealing
  kills that end someone's combo, taking loot from someone mid-fight,
  friendly fire that ruins a play). Competition is about *outdoing*
  each other, not *undoing* each other.
- Reward individual excellence in ways that also help the team: the top
  scorer's bonus, buffs that land on allies, revives that score points.
- Make individual contribution visible: score, damage, revives,
  objectives.
- Helping a teammate should always be worth something, even when it costs
  you score.

## 3. Creative Builds & Dynamic Play

> Embrace variability in gameplay through dynamic build systems. Encourage
> players to adapt to and experiment with different builds, rewarding
> creative problem-solving and strategic use of available resources.
>
> *Example application:* Develop a wide array of abilities, items, and
> power-ups that change with each playthrough. Create synergies and combos
> that reward players for experimenting and mastering different
> playstyles. Ensure that no two playthroughs feel the same by randomizing
> elements and providing unique challenges.

**In practice**

- Prefer mechanics that interact (statuses, combos, infusion synergies)
  over isolated numbers. A new ability should combine with at least one
  existing system: effects, infusions, another character's kit.
- Avoid a single dominant build. If one option is clearly best, nerf it,
  give it a cost, or make the alternatives better.
- "Play the hand you are dealt": randomness in drops, spawns and
  objectives is good; too much control over what you get is not (the
  Risk of Rain lesson in the GDD).
- Characters should play differently, not just look different. A kit that
  plays like an existing kit is a red flag.

## 4. Clarity (Feedback and Recognition)

> Maintain visual and mechanical clarity to ensure players can quickly
> understand the game state and make informed decisions. Prioritize
> intuitive design and clear communication of game mechanics.
>
> *Example application:* Use distinct visual cues, colors, and animations
> to differentiate between various game elements and actions. Implement a
> clean UI that highlights critical information without clutter.
> Regularly playtest to identify and resolve any areas of confusion,
> ensuring that the game remains accessible and enjoyable even in the heat
> of battle.

**In practice**

- Every hit needs feedback: animation, flash, number and, eventually, sound.
  If you cannot tell an attack landed, it did not happen.
- Enemy attacks that hurt must be telegraphed (red) and dodgeable; avoid
  "random unavoidable BS".
- Readability beats spectacle: when effects pile up (several bursts on one
  landing) remove some until the important thing reads.
- Critical state is always visible: your health, ship health, the zone,
  round time, where the objectives are.
- Each element has one look: the same colour or shape always means the same
  thing.

---

## When pillars conflict

They will. For example, adding a timer (Crunch) can add clutter (Clarity).

1. Say which pillars are in tension and why, in the decision log.
2. Pick the option that serves the pillar this feature exists for, and
   keep the cost to the others small.
3. If the trade-off is big or hard to undo, ask a human before building it.

## Where the rest of the design lives

- Google Drive: GDD, CORE GAMEPLAY, COMPETITIVE PLAY, Controls/HUD, and the
  Genera and Infusion System.
- `PLAYTEST.md`: what is built, and the questions to ask in a playtest.
- `docs/design/DECISIONS.md`: design decisions made in this repository.
