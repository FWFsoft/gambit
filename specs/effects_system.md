# Effects System

# Glossary

**Effect** — An effect (short for *status effect*) is something applied to a character in the game that applies a positive or negative impact on their gameplay.

**Buff** — An effect that is positive.

**Debuff** — An effect that is negative.

**Cleanse** — Remove a debuff

**Purge** — Remove a buff

**Corrupt** — Change a buff to its opposite (buff \-\> debuff)

**Purify** — Change a debuff to its opposite (debuff \-\> buff)

**Double** — doubles the stacks of a stackable buff/debuff

**Contaminate** — copies debuffs from one target to another

**Radiate** — copies buffs from one target to another

# Background

[DD-AA-008: Let’s Talk About Frothy](https://docs.google.com/document/d/1-nBGk6HEfjqIhhU5zhgD_SoFbmfv2Bzf8v0Yu7LTS0M/edit) discussed what we should do about a cool effect that wasn’t really playing into our design pillar of *Creative Builds and Dynamic Play.* It was not clear how we could make the effect interact with infusions without significantly complicating the core design of the infusion system. The outcome of that DD was that we should create an Effects System.

The Effects System gives us a way to design infusions that interact with generic effects that characters may/may not apply to enemies and allies. 

# Attributes of an Effect

Every effect should have the following defined:

**Duration** — Defines how long the character will be affected by the effect

**Intensity** — e.g. Does a slow reduce the characters speed by 10% or 20%?

**Stacks vs. Overrides vs Extends** — What happens when an effect is reapplied when the target is already affected? Does it add an additional stack? Does it overwrite the previous application? Is it applied after the current application of the effect finishes?

**Opposite** — Every buff/debuff should have an opposite, e.g. Slow Down vs Speed Up

# Effects

Key: Green cell \= Buff, Red cell \= Debuff, Yellow \= Neutral effect (has no opposite)

| Name | Description | Opposite | Stacks, Overrides, or Extends | Intensity | Duration |
| :---- | :---- | ----- | ----- | :---- | :---- |
| Slow | **Decreased** run speed Abilities recharge **slower** | Haste | *Stacks* | Affected by Infusions Varies per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied Not affected by Infusions Fixed duration per character Refreshed on new application Can be affected by global objective bonuses |
| Haste | **Increased** run speed Abilities recharge **faster** | Slow | *Stacks* | Affected by Infusions Varies per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied **NOT** affected by Infusions **FIXED** duration per character Refreshed on new application Can be affected by global objective bonuses |
| Weakened | Deal **less** damage | Empowered | *Stacks* | **NOT** affected by infusions **DOESN’T** vary per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied |
| Empowered | Deal **more** damage | Weakend | *Stacks* | **NOT** affected by infusions **DOESN’T** vary per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied |
| Vulnerable | Take **more** damage from all sources | Fortified | *Stacks* | **NOT** affected by infusions **DOESN’T** vary per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied |
| Fortified | Take **less** damage from all sources | Vulnerable | *Stacks* | **NOT** affected by infusions **DOESN’T** vary per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied |
| Wound | Take **damage** over time | Mend | *Stacks* | Affected by infusions Varies per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied  **NOT** affected by infusions **DOESN’T** vary per character Can be affected by global objective bonuses—makes the same amount of damage apply over longer or shorter amounts time |
| Mend | Receive **healing** over time | Wound | *Stacks* | Affected by infusions Varies per character Stacks with additional applications | Duration for all applications are refreshed when new application are applied  **NOT** affected by infusions **DOESN’T** vary per character Can be affected by global objective bonuses—makes the same amount of healing apply over longer or shorter amounts time |
| Dulled | **Reduce** critical strike **chance Reduce** critical strike **damage** |  |  |  |  |
| Sharpened | **Increase** critical strike **chance Increase** critical strike **damage** |  |  |  |  |
| Cursed | Cannot **gain buffs** | Blessed | *Extends* | N/A | Duration of Cursed is **extended by** additional applications of Cursed **NOT** affected by infusions **DOESN’T** vary per character **NOT** affected by global objective bonuses. |
| Blessed | Cannot **gain debuffs** | Cursed | *Extends* | N/A | Duration of Blessed is **extended by** additional applications of Blessed **NOT** affected by infusions **DOESN’T** vary per character **NOT** affected by global objective bonuses. |
| Doomed | **Debuffs** cannot be cleansed or purified | Anchored | *Overrides* | N/A | Duration of Doomed is **reset by** additional applications of Doomed **NOT** affected by infusions **DOESN’T** vary per character **NOT** affected by global objective bonuses. |
| Anchored | **Buffs** cannot be purged or corrupted | Doomed | *Overrides* | N/A | Duration of Anchored is **reset by** additional applications of Anchored **NOT** affected by infusions **DOESN’T** vary per character **NOT** affected by global objective bonuses. |
| Marked | Nearby enemies will attack this target | Stealth | *Extends* | N/A | Duration of Marked is **extended by** additional applications of Marked Affected by infusions Varies per character **NOT** affected by global objective bonuses |
| Stealth | **Cannot be seen** by enemies | Marked | *Extends* | N/A | Duration of Stealth is **extended by** additional applications of Stealth Affected by infusions Varies per character **NOT** affected by global objective bonuses |
| Expose | **Increase** the damage taken by the next source of damage (that isn’t Wound) | Guard | *Stacks* | Affected by infusions Varies per character Stacks with additional applications | Lasts until consumed Duration **NOT** affected by anything else |
| Guard | **Decrease** the damage taken by the next source of damage (that isn’t Wound) | Expose | *Stacks* | Affected by infusions Varies per character Stacks with additional applications | Lasts until consumed Duration **NOT** affected by anything else |
| Stunned | Cannot act or move Gain \+1 **Weakened** whenever you are Stunned | Berserk | *Overrides* | Affected by infusions Varies per character | Duration of Stunned is **reset by** additional applications of Stun Affected by infusions Varies per character Affected by global objective bonuses |
| Beserk | **Immune** to **Stun** Gain \+1 **Empowered** whenever you gain Berserk | Stuned | *Overrides* | Affected by infusions Varies per character | Duration of Berserk is **reset by** additional applications of Berserk Affected by infusions Varies per character Affected by global objective bonuses |
| Snared | Cannot move Gain \+1 **Slow** whenever you are Snared | Unbounded | *Extends* | Affected by infusions Varies per character | Duration of Snared is **extended by** additional applications of Snare Affected by infusions Varies per character Affected by global objective bonuses |
| Unbounded | **Immune** to the movement-impairing effects of **Slow** and **Snare** Gain \+1 **Haste** whenever you are affected by Unbound | Snared | *Extends* | Affected by infusions Varies per character | Duration of Unbounded is **extended by** additional applications of Unbound Affected by infusions Varies per character Affected by global objective bonuses |
| Confused | **Lose control** and begin attacking allies Gain \+1 **Dulled** whenever you are affected by Confusion | Focused | *Extends* | Affected by infusions Varies per character | Duration of Confused is **extended by** additional applications of Confusion Affected by infusions Varies per character Affected by global objective bonuses |
| Focused | **Immune** to Confusion Gain \+1 **Sharpened** whenever Focus is applied to you | Confused | *Extends* | Affected by infusions Varies per character | Duration of Focused is **extended by** additional applications of Focus Affected by infusions Varies per character Affected by global objective bonuses |
| Silenced | Cannot use abilities other than basic attack (LMB) Gain \+1 **Dulled** and \+1 **Weakened** whenever you are Silenced | Inspire | *Extends* | Affected by infusions Varies per character | Duration of Silenced is **extended by** additional applications of Silenced Affected by infusions Varies per character Affected by global objective bonuses |
| Inspired | **Immune** to Silence  Gain \+1 **Sharpened** and \+1 **Empowered** whenever you are Inspired  | Silence | *Extends* | Affected by infusions Varies per character | Duration of Inspired is **extended by** additional applications of Inspire Affected by infusions Varies per character Affected by global objective bonuses |
| Grappled | Movement is linked to another character Cannot move otherwise | Unshackled | *Extends* | Affected by infusions Varies per character | Duration of Grappled is **extended by** additional applications of Grapple Affected by infusions Varies per character Affected by global objective bonuses |
| Freed | **Immune** to the movement-impairing effects of **Slow** and **Grapple** Gain \+1 **Empowered** whenever you are affected by Free  | Grappled | *Extends* | Affected by infusions Varies per character | Duration of Freed is **extended by** additional applications of Free Affected by infusions Varies per character Affected by global objective bonuses |
| Resonance | Not every character can gain Resonance What Resonance does is entirely defined by the character who has the effect applied to them. | N/A | *Stacks* | **NOT** affected by infusions Varies per character Stacks with additional applications | Lasts until consumed Duration **NOT** affected by anything else |

# Example Character Application

Given this was spawned by realizing that *Frothy* might not interact well with infusions, we could try to reimagine Suds design using this new effect system.

**Passive: *Frothy***

When nearby creatures lose **Guard** they gain **Wound** for an amount of damage equal to the amount prevented by **Guard**.

**LMB (Basic Attack): Brew Toss** 

Suds flings his brew in a small area, dowsing creatures with frothy suds. All targets hit by Brew Toss gain \+3 **Guard**

**RMB (Hold): Frothy Cleanse**    
Suds unleashes a mighty belch in a cone in front of him, *cleansing* **Wound** from all creatures. 

Allies who are affected by **Frothy Cleanse** gain **Berserk**

Enemies who are affected by **Frothy Cleanse** are **Stunned**

**Tab: Round’s on me\!**   
Suds rapidly tosses six brews in quick succession, splashing creatures in a larger area. All targets hit gain \+3 **Guard**

**D: What's in This Stuff?** 

Suds taps into the secret ingredient of his brew, *doubling* the stacks of **Guard** and **Wound** on nearby creatures. 

Allies affected by **What’s in This Stuff** gain **Fortified.**

Enemies affected by **What’s in This Stuff** gain **Weakened.**

# Example Infusion Application

| Frigidariant Hide |
| :---- |
| \++ Health Whenever you take damage, apply \+1 **Slow** to the creature who damaged you. |
| **Infusion Type: Frigidariant   Choose One:  Offense** \- ***Icy Strikes*****:** Attacks slow targets hit, attacks on slowed targets briefly root them, and attacks on rooted targets briefly freeze them. **Defense** \- ***Arctic Aura*****:** Melee attackers are slowed on contact, slowed targets are rooted, and rooted targets are frozen. **Mobility** \- ***Glacial Path***: Leave behind a trail of ice that slows enemies. **Genealogy** \- ***Unbreakable***: Applying hard CC to enemies grants a temporary shield. |

| Iceborn Crown |
| :---- |
| \++ Damage Whenever nearby enemies gain **Slow**, gain \+1 **Empowered** |
| **Infusion Type: Frigidariant   Choose One:  Offense** \- ***Icy Strikes*****:** Attacks slow targets hit, attacks on slowed targets briefly root them, and attacks on rooted targets briefly freeze them. **Defense** \- ***Arctic Aura*****:** Melee attackers are slowed on contact, slowed targets are rooted, and rooted targets are frozen. **Mobility** \- ***Glacial Path***: Leave behind a trail of ice that slows enemies. **Genealogy** \- ***Unbreakable***: Applying hard CC to enemies grants a temporary shield. |

