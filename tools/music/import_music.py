#!/usr/bin/env python3
"""Import the game's music: convert a folder of tracks to assets/music/ and
write the playlist file the game reads (assets/music/music.txt).

The source folder is laid out the way the composer delivers it, one folder
per place, the category in brackets at the end of each file name:

  Pacific Northwest/First Light Through Cedar Bones (Confident).mp3
  Desert/Ghosts in the Dust (Weakened_Alone).mp3     <- in both playlists
  City/The Heat (Boss).mp3
  Landing Zone/First Contact.wav
  Landing Zone/Frontier Pulse (Landing Zone — Confident).wav
  Landing Zone/First Light (Landing Zone — Weakened_After First Contact).wav
  Intermission/Character Select/AA - Intermission_Select.mp3

Folder names map to playlist keys with FOLDERS below (biome keys match the
map's Biome ids, include/Biome.h). Categories: Confident, Weakened, Alone,
Boss; "Weakened_Alone" goes in both. Landing Zone's "First Contact" is its
own playlist. MP3, M4A and WAV all work; everything is converted to OGG
Vorbis at even loudness, so the game never jumps in volume between tracks.

Usage:
  python3 tools/music/import_music.py <source folder> [--quality 3]
Needs ffmpeg. Re-running replaces assets/music/<key>/ folders it writes and
rewrites music.txt from everything it finds; title-screen and other loose
files in assets/music/ are left alone.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "assets" / "music"

# Source folder name (lower case) -> playlist prefix.
FOLDERS = {
    "pacific northwest": "pnw",
    "desert": "desert",
    "tidal zone": "tidal",
    "tidal": "tidal",
    "city": "city",
    "arctic": "arctic",
    "jungle": "jungle",
    "plains": "plains",
    "swamp": "swamp",
    "landing zone": "landing",
    "intermission": "lobby",
    "character select": "lobby",
}
# A folder already named by its key (pnw, landing, lobby...) maps to itself.
FOLDERS.update({k: k for k in set(FOLDERS.values())})
AUDIO = {".mp3", ".wav", ".m4a", ".ogg", ".flac"}
LOUDNESS = -16.0      # LUFS, integrated
TRUE_PEAK = -1.5      # dBTP


def slug(text):
    text = re.sub(r"\(.*?\)", "", text)
    text = re.sub(r"^AA - ", "", text)
    text = re.sub(r"[^a-zA-Z0-9]+", "_", text).strip("_").lower()
    return text or "track"


def categories(prefix, stem):
    """Playlist keys a track belongs to."""
    if prefix == "lobby":
        return ["lobby"]
    tag = re.findall(r"\(([^)]*)\)", stem)
    tag = tag[-1].lower() if tag else ""
    if prefix == "landing":
        if "first contact" in stem.lower() and not tag:
            return ["first_contact"]
        if "weakened" in tag or "alone" in tag:
            return ["landing.weakened", "landing.alone"]
        if "confident" in tag:
            return ["landing.confident"]
    keys = []
    for mood in ("confident", "weakened", "alone", "boss"):
        if mood in tag:
            keys.append(f"{prefix}.{mood}")
    return keys


def folder_prefix(path, root):
    for part in reversed(path.relative_to(root).parts[:-1]):
        key = FOLDERS.get(part.lower())
        if key:
            return key
    return None


def measure(src):
    """First loudnorm pass: the track's measured loudness."""
    out = subprocess.run(
        ["ffmpeg", "-hide_banner", "-nostats", "-i", str(src), "-af",
         f"loudnorm=I={LOUDNESS}:TP={TRUE_PEAK}:LRA=11:print_format=json",
         "-f", "null", "-"], capture_output=True, text=True, check=True).stderr
    return json.loads(out[out.rindex("{"):out.rindex("}") + 1])


def convert(src, dst, quality):
    m = measure(src)
    af = (f"loudnorm=I={LOUDNESS}:TP={TRUE_PEAK}:LRA=11:linear=true:"
          f"measured_I={m['input_i']}:measured_TP={m['input_tp']}:"
          f"measured_LRA={m['input_lra']}:measured_thresh={m['input_thresh']}:"
          f"offset={m['target_offset']}")
    # Trim silence at both ends (the game crossfades between tracks).
    trim = ("silenceremove=start_periods=1:start_threshold=-60dB,areverse,"
            "silenceremove=start_periods=1:start_threshold=-60dB,areverse")
    dst.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(src),
                    "-af", f"{trim},{af}", "-ar", "44100", "-ac", "2",
                    "-c:a", "libvorbis", "-q:a", str(quality), "-map_metadata", "-1",
                    str(dst)], check=True)


def duration(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                          "-of", "csv=p=0", str(path)], capture_output=True, text=True,
                         check=True).stdout
    return float(out.strip())


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("source")
    ap.add_argument("--quality", type=int, default=3, help="Vorbis quality (3 = ~112 kbps)")
    args = ap.parse_args()
    root = Path(args.source)
    tracks = sorted(p for p in root.rglob("*") if p.suffix.lower() in AUDIO)
    if not tracks:
        sys.exit(f"no audio under {root}")

    playlists = {}
    seconds = {}
    unknown = []
    done_dirs = set()
    for src in tracks:
        prefix = folder_prefix(src, root)
        keys = categories(prefix, src.stem) if prefix else []
        if not keys:
            unknown.append(str(src.relative_to(root)))
            continue
        out_dir = OUT / prefix
        if out_dir not in done_dirs:
            for old in out_dir.glob("*.ogg"):
                old.unlink()
            done_dirs.add(out_dir)
        dst = out_dir / (slug(src.stem) + ".ogg")
        print(f"{src.relative_to(root)} -> {dst.relative_to(OUT)}  [{', '.join(keys)}]")
        convert(src, dst, args.quality)
        length = duration(dst)
        for key in keys:
            playlists.setdefault(key, []).append(str(dst.relative_to(OUT)))
            seconds[key] = seconds.get(key, 0.0) + length

    # Keep playlists the importer did not touch this run (other folders).
    manifest = OUT / "music.txt"
    if manifest.exists():
        touched = {k.split(".")[0] for k in playlists}
        for line in manifest.read_text().splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            key, path = line.split(None, 1)
            if key.split(".")[0] not in touched and (OUT / path).exists():
                playlists.setdefault(key, []).append(path)

    lines = ["# Playlists the game plays (MusicSystem / MusicDirector).",
             "# <playlist key> <file under assets/music/>. Written by",
             "# tools/music/import_music.py; edit by hand to move a track.",
             "# Keys: lobby, first_contact, landing.<mood>, <biome>.<mood>,",
             "# <biome>.boss. Moods: confident, weakened, alone.", ""]
    for key in sorted(playlists):
        for path in sorted(set(playlists[key])):
            lines.append(f"{key} {path}")
    manifest.write_text("\n".join(lines) + "\n")
    print(f"\nwrote {manifest.relative_to(REPO)}")
    for key in sorted(seconds):
        flag = "" if seconds[key] >= 360 or key in ("lobby", "first_contact") \
            else "   (under 6 minutes)"
        print(f"  {key:<20} {len(playlists[key]):2d} tracks  {seconds[key] / 60:5.1f} min{flag}")
    if unknown:
        print("\nskipped (no folder or category recognised):")
        for u in unknown:
            print("  " + u)


if __name__ == "__main__":
    main()
