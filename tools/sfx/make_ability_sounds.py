#!/usr/bin/env python3
"""Build the ability sounds in assets/sfx/ability/ from Kenney's CC0 packs.

Sources (all CC0 1.0, each archive's License.txt says so), from kenney.nl:
  sci-fi-sounds, rpg-audio, impact-sounds, digital-audio.
Unzip each into <packs>/<pack-name>/ (the zip's own layout, with Audio/
inside), then:

    python3 tools/sfx/make_ability_sounds.py <packs>

Writes assets/sfx/ability/shape_<shape>_<n>.ogg and tint_<theme>_<n>.ogg:
mono, 44.1 kHz, trimmed (long loops are cut and faded), and levelled to
the same RMS loudness with peaks no higher than -1.5 dBFS, so the code sets
the mix and not the source files. See docs/design/SFX.md.
"""
import pathlib
import re
import subprocess
import sys

RATE = 44100
TARGET_RMS_DB = -20.0
PEAK_CEILING_DB = -1.5
MONO = "aformat=sample_fmts=flt:channel_layouts=mono"

# (pack, file, keep): keep is None for all of it, seconds to keep from the
# start, or (start, end) in seconds of the source.
SHAPES = {
    "slash": [("rpg-audio", "knifeSlice.ogg", 0.45),
              ("rpg-audio", "knifeSlice2.ogg", 0.45),
              ("rpg-audio", "cloth2.ogg", None)],
    "cone": [("sci-fi-sounds", "thrusterFire_000.ogg", 0.4),
             ("sci-fi-sounds", "thrusterFire_001.ogg", 0.4),
             ("sci-fi-sounds", "thrusterFire_002.ogg", 0.4)],
    "burst": [("sci-fi-sounds", "explosionCrunch_000.ogg", 0.6),
              ("sci-fi-sounds", "explosionCrunch_001.ogg", 0.6),
              ("sci-fi-sounds", "explosionCrunch_002.ogg", 0.6)],
    "ring": [("digital-audio", "phaserDown1.ogg", None),
             ("digital-audio", "phaserDown2.ogg", None),
             ("digital-audio", "phaserDown3.ogg", None)],
    "beam": [("sci-fi-sounds", "laserLarge_000.ogg", None),
             ("sci-fi-sounds", "laserLarge_001.ogg", None),
             ("sci-fi-sounds", "laserLarge_002.ogg", None)],
    "projectile": [("sci-fi-sounds", "laserSmall_000.ogg", None),
                   ("sci-fi-sounds", "laserSmall_001.ogg", None),
                   ("sci-fi-sounds", "laserSmall_003.ogg", None)],
    "lob": [("rpg-audio", "cloth1.ogg", 0.45),
            ("rpg-audio", "cloth3.ogg", None),
            ("rpg-audio", "cloth4.ogg", None)],
    "zone": [("sci-fi-sounds", "forceField_000.ogg", 0.5),
             ("sci-fi-sounds", "forceField_001.ogg", 0.5),
             ("sci-fi-sounds", "forceField_002.ogg", 0.5)],
    "dash": [("digital-audio", "phaseJump1.ogg", None),
             ("digital-audio", "phaseJump2.ogg", None),
             ("digital-audio", "phaseJump3.ogg", None)],
    "tether": [("digital-audio", "zap1.ogg", 0.5),
               ("digital-audio", "zap2.ogg", 0.5)],
    "shield": [("digital-audio", "powerUp2.ogg", None),
               ("digital-audio", "powerUp5.ogg", None),
               ("digital-audio", "powerUp6.ogg", None)],
    "generic": [("digital-audio", "phaserUp4.ogg", None),
                ("digital-audio", "phaserUp6.ogg", None),
                ("digital-audio", "phaserUp7.ogg", None)],
}

# Only the themes the kits use (src/CharacterKits.cpp). Wind and Neutral
# have no tint: the shape alone is the sound.
TINTS = {
    "holy": [("impact-sounds", "impactBell_heavy_002.ogg", 0.6),
             ("impact-sounds", "impactBell_heavy_003.ogg", 0.6)],
    "steel": [("impact-sounds", "impactMetal_light_001.ogg", None),
              ("impact-sounds", "impactMetal_light_002.ogg", None)],
    "brew": [("impact-sounds", "impactGlass_medium_000.ogg", 0.35),
             ("impact-sounds", "impactGlass_medium_001.ogg", 0.35)],
    "optic": [("sci-fi-sounds", "computerNoise_000.ogg", 0.3),
              ("sci-fi-sounds", "computerNoise_001.ogg", 0.3)],
    "nanite": [("digital-audio", "spaceTrash1.ogg", 0.35),
               ("digital-audio", "spaceTrash2.ogg", 0.35)],
    "biomass": [("sci-fi-sounds", "slime_001.ogg", (0.0, 0.4)),
                ("sci-fi-sounds", "slime_001.ogg", (1.2, 1.6))],
    "blood": [("impact-sounds", "impactSoft_heavy_002.ogg", None),
              ("impact-sounds", "impactSoft_heavy_003.ogg", None)],
    "shadow": [("digital-audio", "lowDown.ogg", 0.45)],
    "fire": [("sci-fi-sounds", "thrusterFire_003.ogg", 0.35),
             ("sci-fi-sounds", "thrusterFire_004.ogg", 0.35)],
    "heal": [("digital-audio", "highUp.ogg", None)],
}

FADE_S = 0.08  # fade-out on trimmed clips, so a cut loop does not click


def stats(path, chain):
    out = subprocess.run(
        ["ffmpeg", "-hide_banner", "-i", str(path), "-af",
         f"{chain},astats=metadata=0", "-f", "null", "-"],
        capture_output=True, text=True).stderr
    rms = re.findall(r"RMS level dB: (-?[0-9.]+|-inf)", out)
    peak = re.findall(r"Peak level dB: (-?[0-9.]+|-inf)", out)
    if not rms or not peak or "inf" in rms[-1]:
        raise SystemExit(f"no levels for {path}")
    return float(rms[-1]), float(peak[-1])  # the "Overall" lines come last


def build(packs, out, prefix, table):
    for name, clips in table.items():
        for i, (pack, clip, keep) in enumerate(clips, 1):
            source = packs / pack / "Audio" / clip
            assert source.exists(), source
            chain = MONO
            if isinstance(keep, tuple):
                start, end = keep
                chain += (f",atrim={start}:{end},asetpts=PTS-STARTPTS,"
                          f"afade=t=in:d=0.01")
            chain += ",silenceremove=start_periods=1:start_threshold=-35dB"
            if isinstance(keep, (int, float)):
                chain += f",atrim=0:{keep}"
            if keep is not None:
                length = keep if not isinstance(keep, tuple) else end - start
                chain += f",afade=t=out:st={length - FADE_S:.3f}:d={FADE_S}"
            rms, peak = stats(source, chain)
            gain = min(TARGET_RMS_DB - rms, PEAK_CEILING_DB - peak)
            dest = out / f"{prefix}_{name}_{i}.ogg"
            subprocess.run(
                ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                 "-i", str(source), "-af", f"{chain},volume={gain:.2f}dB",
                 "-ar", str(RATE), "-c:a", "libvorbis", "-q:a", "5",
                 str(dest)], check=True)
            print(f"{dest.name}  <- {pack}/{clip}  {gain:+.1f} dB")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    packs = pathlib.Path(sys.argv[1])
    out = pathlib.Path(__file__).resolve().parents[2] / "assets/sfx/ability"
    out.mkdir(parents=True, exist_ok=True)
    build(packs, out, "shape", SHAPES)
    build(packs, out, "tint", TINTS)


if __name__ == "__main__":
    main()
