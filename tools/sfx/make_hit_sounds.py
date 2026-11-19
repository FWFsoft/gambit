#!/usr/bin/env python3
"""Build the hit sounds in assets/sfx/ from CC0 source clips.

Sources: Kenney's "Impact Sounds" pack (CC0 1.0), as redistributed in the
npm package @netforge/asset-library-files 0.2.0 (library/audio/sfx/). Get it
with:

    curl -sSO https://registry.npmjs.org/@netforge/asset-library-files/-/asset-library-files-0.2.0.tgz
    tar xzf asset-library-files-0.2.0.tgz package/library/audio/sfx

then: python3 tools/sfx/make_hit_sounds.py package/library/audio/sfx

Each sound gets three pitch variants (so repeated hits do not sound like a
machine gun), mono, 44.1 kHz, peaks near -1.5 dBFS (resampling and Vorbis overshoot a little). See docs/design/SFX.md.
"""
import pathlib
import re
import subprocess
import sys

# our name -> source clip
SOUNDS = {"hit": "hit.ogg", "crit": "impact.ogg", "hurt": "hurt.ogg"}
PITCHES = [0.94, 1.0, 1.07]  # playback-rate factors, _1 _2 _3
RATE = 44100


MONO = "aformat=sample_fmts=flt:channel_layouts=mono"


def peak_db(path):
    # Measured in float: the decoded sources overshoot full scale, which a
    # 16-bit measurement would hide by clipping.
    out = subprocess.run(
        ["ffmpeg", "-hide_banner", "-i", str(path), "-af",
         f"{MONO},astats=metadata=0", "-f", "null", "-"],
        capture_output=True, text=True).stderr
    peaks = re.findall(r"Peak level dB: (-?[0-9.]+)", out)
    if not peaks:
        raise SystemExit(f"no peak for {path}")
    return float(peaks[-1])  # the "Overall" line comes last


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    src = pathlib.Path(sys.argv[1])
    out = pathlib.Path(__file__).resolve().parents[2] / "assets" / "sfx"
    out.mkdir(parents=True, exist_ok=True)
    for name, clip in SOUNDS.items():
        source = src / clip
        assert source.exists(), source
        gain = -1.5 - peak_db(source)
        for i, pitch in enumerate(PITCHES, 1):
            dest = out / f"{name}_{i}.ogg"
            subprocess.run(
                ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                 "-i", str(source),
                 "-af", f"{MONO},volume={gain:.2f}dB,"
                        f"asetrate={RATE * pitch:.0f},aresample={RATE},"
                        "silenceremove=start_periods=1:start_threshold=-60dB",
                 "-c:a", "libvorbis", "-q:a", "5", str(dest)], check=True)
            print(dest.relative_to(out.parents[1]))


if __name__ == "__main__":
    main()
