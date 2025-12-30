# Scoring & Economy

[Goals](#goals)

[Definitions](#definitions)

[Map Concept \#1](#wagers-in-detail)

[Open Questions](#open-questions)

## Goals {#goals}

Players should always feel rewarded for playing the game. They should feel further rewarded for playing the game well and they should feel *maximally* rewarded when the whole team plays well together. Even when players end up with suboptimal builds, they should eventually be beaten by hordes of enemies or a big boss, ideally not the storm timer or some random minion.

In between games, players should be able to track meta-progression on their account. This way, they can track personal goals, compare stats with friends, and see how their effort compares to other players.

## Definitions {#definitions}

**As of yet, there is no in-game currency. That is, players acquire items only from drops in the game and there is no determinism outside of event-specific drops.**

**Points** are the metric we use to reflect how well a player performs. Players earn points in three ways:

1. During a round by completing objectives, participating in combat, or solving puzzles.  
2. At the end of a round by fulfilling conditions of a **wager**   
3. At the end of the game by meeting the requirements of a multi-round goal.  
   1. I.e. “Doom Slayer” \- do at least 5000 damage to elite monsters

A **wager** is a contest that can be initiated by one player and accepted by another in between rounds. The wager requires a number of points to bet and the condition to win. If a participant dies and does not finish the round, they are considered the loser. Here are some hypothetical examples:

1. A wagers 100 points that B will defeat fewer enemies than A this round.  
2. A wagers 200 points that B will defeat fewer enemies than A this round with the handicap that B cannot use their charged ability.

**Player stats** should be tracked on their profile and accessible in game. This allows the player to see the summary of their journey with sensible breakdowns (i.e. by character, by map, etc). Some examples:

* Total enemies slain  
* Total healing done  
* Most played character  
* Average survival time

## Wagers in Detail {#wagers-in-detail}

Wagers are probably not trivial to get players to engage with so we want to keep them pretty straightforward. In between rounds, each player can create one wager with one other player. More than one player can offer wagers to the same player and that player can accept as many as they choose. In the interest of keeping the math simple, players cannot propose wagers with more points than they have. If one player is the subject of multiple wagers and would go negative when failing all of them, they instead go to 0 points.

## Open Questions {#open-questions}

1. Should we have the concept of “medals” for end of round scoring? For example:  
   1. “Can’t touch this” (+50pts) \- took 0 damage this round  
   2. “Teacher’s pet” (+100pts) \- completed the most objectives  
2. Should we allow wagers to be a point multiplier rather than just *n* points? So a 2x wager would mean that whoever wins gets double points this round.  
3. Players must choose between preset conditions for a wager. Should we make some of these unlockable over time? Perhaps there’s an internal progression system where earning points or winning wagers allows the player to use new conditions.