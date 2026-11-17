# Character archetypes (DD-AA-003, accepted)

Every character belongs to one of four archetypes, named after roles on a
sports team. An archetype is a set of **restrictions** on a character's
base kit, not a list of abilities: it says what the character may not
have, and the most it may have of each trait. Check a new or changed kit
against its archetype before building it.

This file keeps the conclusions of the original design doc. It serves
Comradery in Competition (see `PILLARS.md`): characters need each other,
unlike games without archetypes where it is every player for themselves.

## Traits

- **DMG**: damage dealt.
- **Healing**.
- **Mobility**: above all, crossing the map quickly. Short dodges and
  repositioning count too, but map traversal is the point.
- **Resilience**: reducing the damage you take.
- **Crowd Control (CC)**.

Area, burst and over-time effects are ways to shape these traits, not
traits of their own.

## The archetypes

| Archetype | Role | Hard restriction (base kit) | Very High | High | Mid | Low | Very Low |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Playmaker | Peel: crowd-control enemies to set up plays | **No DMG** | CC, Mobility | | Healing, Resilience | | DMG |
| Defender | Soak: protect the ship and allies | **No mobility** | Resilience | | CC, Healing | DMG | Mobility |
| Striker | Kill: capitalise on openings | **No crowd control** | DMG | | Mobility, Resilience | Healing | CC |
| Winger | Heal: help those in need | **No DMG** | Healing | Mobility | CC, Resilience | | DMG |

The maxima are compared across the cast with each character's base kit
plus its synergy with infusions. Example: no Playmaker may out-heal a
Winger.

## Why

- **Wingers and Playmakers deal no damage.** Every character gains damage
  from infusions. A character with heavy AoE crowd control and any base
  damage would steamroll from minute one once an on-hit infusion lands.
  Their healing and crowd control should instead feel very powerful.
- **Strikers wait for an opening.** Like a shooting guard, they rarely make
  their own opening: a Playmaker creates it. Their mid mobility and
  resilience buy time until it comes; they are slower than Playmakers and
  Wingers, who play around them.
- **Wingers and Playmakers are the fast ones.** With little damage, their
  points come mostly from objectives, and speed wins objectives. Healers
  should not be stuck in place. Fast tanks or fast damage dealers would
  break the feeling of being overwhelmed; a fast Winger who arrives to
  heal a Defender at the last moment buys time without removing the
  pressure.
