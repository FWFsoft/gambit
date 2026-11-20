# Music

Which music plays when. The rules are the designer's; the code is
`MusicDirector` (`include/MusicDirector.h`, tested by
`tests/test_music_director.cpp`), played by `MusicSystem`.

## The rules

| When | Plays |
| --- | --- |
| Character select, the lobby, intermission between rounds | the intermission and character select song, looping |
| The crash (the intro, then round 1) | **First Contact**, once, until it ends, wherever the crew goes: it starts with the crash intro and plays on into round 1 unbroken |
| At the crash site (the ash ground around the ship: in at 1,300 px, out past 1,700 px) | the landing zone's songs: one for Confident, one for Weakened and Alone |
| Anywhere else | the biome's songs for the crew's mood |
| The boss fight | the boss music of the biome the ship landed in; any biome's boss music if it has none |
| Victory, defeat | whatever was playing carries on |

**Mood** follows the party, as the designer set it:

| Party | Confident | Weakened | Alone |
| --- | --- | --- | --- |
| 1 | always | | |
| 2 | 2 alive | | 1 |
| 3 | 3 | 2 | 1 |
| 4 | 4 | 3 or 2 | 1 |

In short: Confident while everyone stands (always, solo), Alone for the
last one standing, Weakened between.

Within a playlist the tracks shuffle, never the same one twice in a row.
Between playlists the old track fades out and the new one fades in over
1.5 s.

## Steady, not twitchy

Music must not flip every few seconds (it would if it followed every
border and every revive), so changes wait:

- **A new place** (out of the home ring, into another biome) takes 5 s of
  standing in it. A quick trip home with scrap does not change the music.
- **A worse mood** takes 1.5 s (so a teammate revived at once does not
  change it); **a better one** takes 6 s.
- **A biome with no music yet** (Plains, Arctic, Jungle, Swamp for now)
  keeps whatever was playing.

The numbers are `MusicDirector::ZONE_HOLD_S`, `WORSEN_HOLD_S`,
`IMPROVE_HOLD_S`, `LANDING_RADIUS` and `LANDING_EXIT_RADIUS`.

## The radio

Every place playlist (`<place>.<mood>`, not the lobby, First Contact or
boss music) is a channel that plays on whether anyone is tuned in, on a
clock that starts with the match. Moving between places turns the dial:
a 0.6 s fade out and in, and the new channel picks up where its song
would be by now, not from the start. A channel first heard mid-match
joins a song somewhere in its first two thirds, as if it had been on all
along. A song that ended while you were away has moved on to the
channel's next one. (SDL_mixer plays one song at a time, so the dial is a
short fade out then in, not an overlap.)

## Adding music

1. Put tracks in a folder per place, the category in brackets at the end
   of each name, as the composer delivers them:
   `Desert/High Noon Strut (Confident).mp3`, `(Weakened)`, `(Alone)`,
   `(Boss)`, `(Weakened_Alone)` for both. MP3, M4A or WAV.
2. Run `python3 tools/music/import_music.py <folder>`. It converts to OGG
   at even loudness (-16 LUFS), trims silence, writes
   `assets/music/<place>/` and the playlist file `assets/music/music.txt`,
   and lists any playlist under six minutes.
3. To move a track to another playlist, edit `music.txt` by hand: one
   `<playlist> <file>` per line.

Playlist keys: `lobby`, `first_contact`, `landing.confident`,
`landing.weakened`, `landing.alone`, `<biome>.confident`,
`<biome>.weakened`, `<biome>.alone`, `<biome>.boss`. Biome keys are the
map's (`include/Biome.h`): `plains`, `pnw`, `arctic`, `desert`, `tidal`,
`city`, `jungle`, `swamp`.

## What there is (2026-11-18)

See the importer's report (`tools/music/import_music.py`). Short of the six
minutes a category should hold: Tidal (every category, and no boss track),
PNW Alone, City Boss. First Contact (4.4 min) is longer than round 1
(2.5 min): it fades out as the intermission starts, which the designer is
happy with (2026-11-18), and does not come back in later rounds.
