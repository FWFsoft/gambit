---
name: design-review
description: Check a gameplay change (mechanics, tuning, characters, enemies, UI, art, match flow) against Alien Arena's four design pillars and record it in the design decision log. Use before finishing any work that changes how the game plays, looks or feels, when choosing between design options, or when the user asks whether something fits the game's vision.
---

# Design review against the pillars

Alien Arena's pillars are Strategic Crunch, Comradery in Competition,
Creative Builds & Dynamic Play, and Clarity. Humans may not see every
change, so this review and the decision log are how they stay in control
of the design.

## Instructions

1. Read `docs/design/PILLARS.md` in full; the "In practice" sections are
   the checklist. Skim the latest entries in `docs/design/DECISIONS.md`
   so you don't contradict an accepted decision.

2. Work out what the change does to the player experience: look at the
   diff (`git diff`, or `git diff origin/main...HEAD` for a branch) and
   the request it came from. Ignore pure engine and tooling work.

3. Score the change against each pillar: **serves**, **neutral** or
   **weakens**, with one concrete sentence each. Use these questions:
   - **Crunch:** Does it add a real trade-off or pressure, or remove one?
     Does it create dead time, such as walking through empty space or
     waiting with nothing to decide?
   - **Comradery in Competition:** Can one player now deny another's fun?
     Does individual skill show, and does it help the team?
   - **Creative Builds:** Does it combine with existing systems (effects,
     infusions, other kits)? Does it create a dominant option or make two
     options feel the same?
   - **Clarity:** Can a player tell what happened, who did it, and what is
     about to hurt them, in the middle of a fight? Does it add clutter?

4. If anything **weakens** a pillar:
   - Small and easy to change: fix it now if that fits the request, or
     note it.
   - Big or hard to undo: tell the user before going further and suggest
     an alternative.
   - Against a design doc in the team Drive (GDD, CORE GAMEPLAY, COMPETITIVE
     PLAY, Controls/HUD, Genera and Infusion System): say so explicitly.

5. Check the claims you can check:
   - **Balance:** run `./build/ArenaSim` with the affected characters.
   - **Clarity:** run `/visual-test` or record frames and look at them.
   Mention the results.

6. Add or update the entry in `docs/design/DECISIONS.md`, newest first, in
   the format at the top of that file. Use status **Built, needs review**
   unless a human explicitly asked for the change, in which case use
   **Accepted (requested by a human)**.

7. In your summary to the user, give one line per pillar affected and
   point out anything that weakens a pillar.

## Example verdict

```
Pillar check: Bomba sit-out powerbomb
- Crunch: serves. The 260ms seated recovery makes it a commitment.
- Comradery: neutral.
- Creative Builds: serves. A grapple finisher no other kit has.
- Clarity: weakens slightly. Three bursts overlap on landing; one was removed.
Logged in docs/design/DECISIONS.md (Built, needs review).
```
