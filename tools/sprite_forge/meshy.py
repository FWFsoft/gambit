#!/usr/bin/env python3
"""Make a character model from its concept art with the Meshy API.

Turns a turnaround (or a splash) into `art/models/<Name>.glb`, the file the
sprite forge fits, animates and bakes. Plain Python, no packages needed.

    python tools/sprite_forge/meshy.py balance
    python tools/sprite_forge/meshy.py list
    python tools/sprite_forge/meshy.py generate Wade front.png side.png back.png
    python tools/sprite_forge/meshy.py remesh Wade --task <task id>
    python tools/sprite_forge/meshy.py fetch Wade <task id>

CREDITS ARE MONEY. `generate` and `remesh` spend them, and nothing is
refunded unless the task fails. So both commands only print what they would
cost and stop. To really run one, a human must have agreed to that cost, and
you pass it back:

    python tools/sprite_forge/meshy.py generate Wade front.png --approve-credits 30

The number must equal the printed cost. `balance`, `list` and `fetch` are
free. Rules for agents are in art/models/README.md ("Spending credits").

The key: set MESHY_API_KEY. In a Claude Code cloud session the key is saved
on the environment instead and added to requests for you; leave the variable
unset there.

Output, next to each other in art/models/:
    <Name>.glb           the model
    <Name>.preview.png   Meshy's own render of it: look at it
    <Name>.meshy.json    which task, settings and inputs made it
"""

import argparse
import base64
import json
import mimetypes
import os
import struct
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
MODELS = os.path.join(REPO, "art", "models")
API = "https://api.meshy.ai/openapi/v1"
KINDS = ("image-to-3d", "multi-image-to-3d", "remesh")

# What the forge wants (art/models/README.md).
TRIANGLES = (20_000, 60_000)
MAX_BYTES = 15 * 1024 * 1024
DEFAULT_POLYCOUNT = 40_000

# Credits per request, from https://docs.meshy.ai/api/pricing (2026-11).
# If Meshy changes its prices this table is wrong: the balance printed
# before and after each run is the check.
MESH_CREDITS = {"latest": 20, "meshy-7.1": 20, "meshy-6": 20, "meshy-6-lite": 5}
TEXTURE_CREDITS = {"2k": 10, "4k": 10, "8k": 15}
REMESH_CREDITS = 5

POLL_SECONDS = 5
TIMEOUT_SECONDS = 30 * 60


def die(message):
    sys.exit(f"meshy: {message}")


# ---- HTTP ---------------------------------------------------------------
def call(method, path, body=None):
    """One API request. Returns (parsed JSON, headers)."""
    key = os.environ.get("MESHY_API_KEY", "proxy-injected")
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(API + path, data=data, method=method, headers={
        "Authorization": f"Bearer {key}",
        "Content-Type": "application/json",
        "User-Agent": "sprite-forge",
    })
    try:
        with urllib.request.urlopen(req, timeout=120) as res:
            return json.loads(res.read() or b"null"), res.headers
    except urllib.error.HTTPError as e:
        text = e.read().decode("utf-8", "replace")[:600]
        if e.code == 401:
            die("401 from Meshy: no valid API key. Set MESHY_API_KEY.")
        if e.code == 402:
            die(f"402 from Meshy: not enough credits. {text}")
        raise RuntimeError(f"{method} {path}: HTTP {e.code}: {text}") from e


def download(url, path):
    req = urllib.request.Request(url, headers={"User-Agent": "sprite-forge"})
    with urllib.request.urlopen(req, timeout=600) as res, open(path, "wb") as f:
        while True:
            chunk = res.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)


def balance():
    return int(call("GET", "/balance")[0]["balance"])


def find_task(task_id):
    """A task can live under any of the endpoints; find which."""
    for kind in KINDS:
        try:
            return kind, call("GET", f"/{kind}/{task_id}")[0]
        except RuntimeError as e:
            if "HTTP 404" not in str(e):
                raise
    die(f"no task {task_id} on this account")


def wait(kind, task_id):
    start = time.time()
    last = None
    while True:
        try:
            task, headers = call("GET", f"/{kind}/{task_id}")
        except RuntimeError as e:          # a failed poll is not a failed task
            print(f"  poll failed, trying again: {e}")
            time.sleep(POLL_SECONDS)
            continue
        status = task["status"]
        line = f"  {status} {task.get('progress', 0)}%"
        if line != last:
            print(line, flush=True)
            last = line
        if status == "SUCCEEDED":
            return task
        if status in ("FAILED", "CANCELED"):
            error = (task.get("task_error") or {}).get("message", "")
            die(f"task {task_id} {status}: {error} (Meshy refunds failed tasks)")
        if time.time() - start > TIMEOUT_SECONDS:
            die(f"task {task_id} still {status} after {TIMEOUT_SECONDS // 60} "
                f"minutes. It keeps running; get it later with: fetch <Name> {task_id}")
        time.sleep(int(headers.get("Retry-After") or POLL_SECONDS))


# ---- files --------------------------------------------------------------
def image_input(path):
    """A local image as a data URI; a URL as it is."""
    if path.startswith(("http://", "https://")):
        return path
    assert os.path.isfile(path), f"no such image: {path}"
    mime = mimetypes.guess_type(path)[0]
    assert mime in ("image/png", "image/jpeg", "image/webp"), \
        f"{path}: Meshy takes .png, .jpg and .webp"
    assert os.path.getsize(path) <= 20 * 1024 * 1024, f"{path}: over 20 MB"
    with open(path, "rb") as f:
        return f"data:{mime};base64,{base64.b64encode(f.read()).decode()}"


def glb_triangles(path):
    with open(path, "rb") as f:
        magic, _version, _length = struct.unpack("<4sII", f.read(12))
        assert magic == b"glTF", f"{path} is not a GLB file"
        size, kind = struct.unpack("<I4s", f.read(8))
        assert kind == b"JSON", f"{path}: unexpected GLB layout"
        doc = json.loads(f.read(size))
    total = 0
    for mesh in doc.get("meshes", []):
        for prim in mesh["primitives"]:
            index = prim.get("indices", prim["attributes"]["POSITION"])
            total += doc["accessors"][index]["count"] // 3
    return total


def paths(name):
    assert name.isalnum(), f"name the model as the game does (Wade), not '{name}'"
    base = os.path.join(MODELS, name)
    return base + ".glb", base + ".preview.png", base + ".meshy.json"


def refuse_overwrite(name, force):
    glb = paths(name)[0]
    if os.path.exists(glb) and not force:
        die(f"{os.path.relpath(glb, REPO)} exists. Pass --force to replace it.")


def save(name, kind, task, record):
    """Download a finished task's model into art/models/ and check it."""
    glb, preview, sidecar = paths(name)
    url = (task.get("model_urls") or {}).get("glb")
    assert url, f"task {task['id']} has no GLB to download"
    os.makedirs(MODELS, exist_ok=True)
    download(url, glb)
    if task.get("thumbnail_url"):
        download(task["thumbnail_url"], preview)
    record = dict(record, task_id=task["id"], endpoint=kind,
                  task_name=task.get("name", ""),
                  saved=time.strftime("%Y-%m-%d"))
    with open(sidecar, "w") as f:
        json.dump(record, f, indent=2)
        f.write("\n")

    triangles = glb_triangles(glb)
    size = os.path.getsize(glb)
    print(f"saved {os.path.relpath(glb, REPO)}: {triangles:,} triangles, "
          f"{size / 1e6:.1f} MB")
    problems = []
    if not TRIANGLES[0] <= triangles <= TRIANGLES[1]:
        problems.append(f"{triangles:,} triangles; the forge wants "
                        f"{TRIANGLES[0]:,} to {TRIANGLES[1]:,}")
    if size > MAX_BYTES:
        problems.append(f"{size / 1e6:.1f} MB; keep models under 15 MB")
    for p in problems:
        print(f"  NOT TO SPEC: {p}")
    if problems:
        print(f"  fix with: meshy.py remesh {name} --task {task['id']}"
              f"   ({REMESH_CREDITS} credits)")
    if os.path.exists(preview):
        print(f"look at {os.path.relpath(preview, REPO)} before going on")
    return not problems


# ---- spending -----------------------------------------------------------
def spend(kind, body, cost, approved, what):
    """The only place credits are spent. Prints the cost and stops unless
    `approved` is exactly that cost."""
    have = balance()
    print(f"{what}")
    print(f"  cost: {cost} credits   balance: {have}   after: {have - cost}")
    if cost > have:
        die("not enough credits for this")
    if approved is None:
        print("NOT RUN. Nothing was spent. Tell the user this cost and the "
              "balance, and get a yes.")
        print(f"Then run the same command with: --approve-credits {cost}")
        sys.exit(0)
    if approved != cost:
        die(f"--approve-credits {approved} does not match the cost ({cost}). "
            "Nothing was spent. Ask again with the right number.")
    task_id = call("POST", f"/{kind}", body)[0]["result"]
    print(f"  task {task_id}")
    task = wait(kind, task_id)
    left = balance()
    print(f"  spent {have - left} credits; {left} left")
    if have - left != cost:
        print(f"  WARNING: expected {cost}. Meshy's prices may have changed: "
              "tell the user and fix the table in meshy.py")
    return task, have - left


# ---- commands -----------------------------------------------------------
def cmd_balance(_args):
    print(f"{balance()} credits")


def cmd_list(_args):
    rows = []
    for kind in KINDS:
        for t in call("GET", f"/{kind}?page_size=20")[0]:
            rows.append((t.get("created_at", 0), kind, t))
    for created, kind, t in sorted(rows, key=lambda r: -r[0]):
        day = time.strftime("%Y-%m-%d", time.gmtime(created / 1000))
        textured = "textured" if t.get("texture_urls") else "no texture"
        print(f"{day}  {t['id']}  {kind:17}  {t['status']:11}  "
              f"{textured:10}  {t.get('name', '')}")


def cmd_generate(args):
    refuse_overwrite(args.name, args.force)
    assert bool(args.images) != bool(args.from_task), \
        "give images, or --from-task to reuse a task's images, not both"
    assert len(args.images) <= 4, "Meshy takes at most four views"
    body = {
        "name": args.name,
        "ai_model": args.model,
        "texture_model": args.texture_model,
        "pose_mode": args.pose,
        "should_remesh": True,
        "topology": "triangle",
        "target_polycount": args.polycount,
        "should_texture": True,
        "enable_pbr": False,              # base colour only: the forge lights it
        "texture_resolution": "2k",
        "origin_at": "bottom",
        "target_formats": ["glb"],
    }
    if args.pose == "none":
        del body["pose_mode"]
    settings = dict(body)
    kind = "image-to-3d"
    if args.from_task:
        kind = find_task(args.from_task)[0]
        assert kind != "remesh", "--from-task needs a generation task"
        body["input_task_id"] = args.from_task
    elif len(args.images) == 1:
        body["image_url"] = image_input(args.images[0])
    else:
        kind = "multi-image-to-3d"        # first image is the front view
        body["image_urls"] = [image_input(p) for p in args.images]
    cost = MESH_CREDITS[args.model] + TEXTURE_CREDITS["2k"]
    source = args.from_task or ", ".join(args.images)
    task, spent = spend(
        kind, body, cost, args.approve_credits,
        f"generate {args.name} ({kind}, {args.model}, "
        f"{args.polycount:,} triangles, {args.pose}) from {source}")
    ok = save(args.name, kind, task, {
        "settings": settings, "credits": spent,
        "inputs": [os.path.basename(p) for p in args.images],
        "from_task": args.from_task})
    sys.exit(0 if ok else 2)


def cmd_remesh(args):
    refuse_overwrite(args.name, args.force)
    body = {
        "input_task_id": args.task,
        "target_formats": ["glb"],
        "topology": "triangle",
        "target_polycount": args.polycount,
        "origin_at": "bottom",
    }
    task, spent = spend(
        "remesh", body, REMESH_CREDITS, args.approve_credits,
        f"remesh task {args.task} to {args.polycount:,} triangles as {args.name}")
    ok = save(args.name, "remesh", task, {
        "settings": body, "credits": spent, "from_task": args.task})
    sys.exit(0 if ok else 2)


def cmd_fetch(args):
    refuse_overwrite(args.name, args.force)
    kind, task = find_task(args.task)
    if task["status"] != "SUCCEEDED":
        print(f"task is {task['status']}; waiting")
        task = wait(kind, args.task)
    ok = save(args.name, kind, task, {"credits": 0, "fetched": True})
    sys.exit(0 if ok else 2)


def main():
    parser = argparse.ArgumentParser(
        description="Concept art to art/models/<Name>.glb with Meshy.",
        epilog="generate and remesh spend credits and only run with "
               "--approve-credits <the printed cost>.")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("balance", help="credits left (free)").set_defaults(run=cmd_balance)
    sub.add_parser("list", help="recent tasks on the account (free)").set_defaults(run=cmd_list)

    def spending(p):
        p.add_argument("--polycount", type=int, default=DEFAULT_POLYCOUNT,
                       help=f"target triangles (default {DEFAULT_POLYCOUNT})")
        p.add_argument("--approve-credits", type=int, metavar="N",
                       help="the cost the user agreed to; without it nothing runs")
        p.add_argument("--force", action="store_true",
                       help="replace an existing art/models/<Name>.glb")

    g = sub.add_parser("generate", help="images to a model (spends credits)")
    g.add_argument("name", help="the game's name for the character, e.g. Wade")
    g.add_argument("images", nargs="*",
                   help="one to four views, the FRONT view first; files or URLs")
    g.add_argument("--from-task", metavar="ID",
                   help="reuse the input images of an earlier task")
    g.add_argument("--model", default="latest", choices=sorted(MESH_CREDITS),
                   help="geometry model (default latest; meshy-6-lite is the cheap draft)")
    g.add_argument("--texture-model", default="meshy-7",
                   choices=("meshy-7", "meshy-n1"),
                   help="meshy-7: sharper faces and patterns (default). "
                        "meshy-n1: flatter colour, fewer seams")
    g.add_argument("--pose", default="a-pose", choices=("a-pose", "t-pose", "none"),
                   help="rest pose Meshy forces (default a-pose; 'none' for creatures)")
    spending(g)
    g.set_defaults(run=cmd_generate)

    r = sub.add_parser("remesh", help="cut a task's model down to size (spends credits)")
    r.add_argument("name")
    r.add_argument("--task", required=True, metavar="ID", help="the task to remesh")
    spending(r)
    r.set_defaults(run=cmd_remesh)

    f = sub.add_parser("fetch", help="download a finished task's model (free)")
    f.add_argument("name")
    f.add_argument("task", help="task id, from `list`")
    f.add_argument("--force", action="store_true")
    f.set_defaults(run=cmd_fetch)

    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
