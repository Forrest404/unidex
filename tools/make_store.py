#!/usr/bin/env python3
"""Builds the website's app store catalogue (site/store/) from app folders.

  python3 tools/make_store.py                     the apps in ../unidex-apps/apps (the store's repo, cloned beside this one)
  python3 tools/make_store.py path/to/apps ...    every app folder inside these folders
  ... --source https://github.com/Forrest404/unidex-apps/tree/main/apps   where each app's code can be read

Each app is checked with the same rules the device uses (src/apps/scripts/apps_logic.h, docs/APPS.md): an app
that fails is left out and the build fails, so a broken app is never offered. Writes site/store/index.json and
copies the files to site/store/apps/<id>/<version>/, with the SHA-256 of each, which the store page checks before
sending a file to a device.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

API_LEVEL = 5  # the newest the firmware offers; an app may ask for any level up to it
MAX_FILE, MAX_APP = 64 * 1024, 512 * 1024
PERMISSIONS = {"wifi", "mic", "meet"}
EXTENDABLE = {"badge"}  # the built-in apps plugins can add to (apps_logic.h, extendable())
CATEGORIES = ("games", "tools", "time", "badges", "learning", "fun", "other")  # for the Apps page's filters
ID = re.compile(r"^[a-z0-9][a-z0-9-]{0,23}$")
NAME = re.compile(r"^[a-z0-9_-][a-z0-9._-]{0,31}$")
VERSION = re.compile(r"^\d{1,4}\.\d{1,4}\.\d{1,4}$")
EXTS = (".lua", ".json", ".bmp", ".txt")


def path_ok(p):
    parts = p.split("/")
    if len(parts) > 3 or parts[0] == "data" and len(parts) > 1:
        return False
    return all(NAME.match(s) and ".." not in s for s in parts) and p.endswith(EXTS)


def check(folder):
    """(entry, problems) for one app folder."""
    problems = []
    app_id = os.path.basename(os.path.normpath(folder))
    try:
        m = json.load(open(os.path.join(folder, "app.json")))
    except (OSError, ValueError) as e:
        return None, [f"app.json: {e}"]
    def text(key, limit, need=False):
        v = m.get(key, "")
        if not isinstance(v, str) or len(v) > limit or (need and not v):
            problems.append(f"{key}: {'needed, ' if need else ''}text up to {limit} characters")
        return v if isinstance(v, str) else ""
    if not ID.match(str(m.get("id", ""))):
        problems.append("id: a-z, 0-9 and -, up to 24 characters")
    elif m["id"] != app_id:
        problems.append(f"id '{m['id']}' isn't its folder's name '{app_id}'")
    name = text("name", 20, True)
    author = text("author", 40)
    desc = text("description", 140)
    if not VERSION.match(str(m.get("version", ""))):
        problems.append("version: major.minor.patch")
    if not isinstance(m.get("api"), int) or not 1 <= m["api"] <= API_LEVEL:
        problems.append(f"api: 1 to {API_LEVEL}")
    kind, extends = m.get("type", "app"), m.get("extends", "")
    if kind == "plugin" and extends not in EXTENDABLE:
        problems.append(f"extends: a plugin extends one of {sorted(EXTENDABLE)}")
    elif kind in ("app", "game") and extends:
        problems.append("extends: only plugins extend an app")
    elif kind not in ("app", "plugin", "game"):
        problems.append("type: \"app\", \"plugin\" or \"game\"")
    if kind == "game":  # run by the Games app (apps_logic.h, check())
        if m.get("api", 0) < 4:
            problems.append("api: a game needs 4")
        if m.get("category") != "games":
            problems.append("category: a game's is games")
        frame_ms = m.get("frame_ms", 100)
        if not isinstance(frame_ms, int) or not 48 <= frame_ms <= 1000:
            problems.append("frame_ms: 48 to 1000")
    perms = m.get("permissions", [])
    if not isinstance(perms, list) or any(p not in PERMISSIONS for p in perms):
        problems.append(f"permissions: only {sorted(PERMISSIONS)}")
    category = m.get("category", "other")
    if category not in CATEGORIES:
        problems.append(f"category: one of {', '.join(CATEGORIES)}")
    icon = m.get("icon")
    if icon is not None and (not isinstance(icon, list) or len(icon) != 40 or
                             any(not isinstance(r, str) or len(r) != 40 for r in icon)):
        problems.append("icon: 40 rows of 40 characters")
    files, total = [], 0
    for root, dirs, names in os.walk(folder):
        dirs[:] = sorted(d for d in dirs if not d.startswith("."))
        for n in sorted(names):
            if n.startswith("."):
                continue
            full = os.path.join(root, n)
            rel = os.path.relpath(full, folder).replace(os.sep, "/")
            if rel in ("README.md", "LICENSE"):
                continue  # for people reading the registry, not for the device
            data = open(full, "rb").read()
            if not path_ok(rel):
                problems.append(f"{rel}: not a file an app may hold (a-z names, .lua .json .bmp .txt, two folders deep)")
            if len(data) > MAX_FILE:
                problems.append(f"{rel}: over 64 KB")
            total += len(data)
            files.append({"path": rel, "size": len(data), "sha256": hashlib.sha256(data).hexdigest(), "full": full})
    if not any(f["path"] == "main.lua" for f in files):
        problems.append("no main.lua")
    if total > MAX_APP:
        problems.append("over 512 KB in all")
    files.sort(key=lambda f: f["path"] == "app.json")  # sent last, as the device expects
    entry = {"id": m.get("id"), "name": name, "version": m.get("version"), "author": author, "description": desc,
             "type": kind, "extends": extends if kind == "plugin" else "", "category": category,
             "permissions": perms, "license": m.get("license", ""), "icon": icon, "bytes": total, "files": files,
             "api": m.get("api")}  # the firmware's API level it needs (unidex.api): newer firmware may be needed
    return entry, problems


def updated(folder):
    """The date of the last change to an app folder ("2026-10-14"), from git if it's in a repository with history."""
    try:
        out = subprocess.run(["git", "-C", folder, "log", "-1", "--format=%cs", "--", "."], capture_output=True,
                             text=True, timeout=20)
        return out.stdout.strip() if out.returncode == 0 else ""
    except (OSError, subprocess.SubprocessError):
        return ""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(here, ".."))
    args = sys.argv[1:]
    source = ""
    if "--source" in args:
        i = args.index("--source")
        source = args[i + 1].rstrip("/")
        del args[i:i + 2]
    sources = args or [os.path.join(root, "..", "unidex-apps", "apps")]
    for src in sources:
        if not os.path.isdir(src):
            sys.exit(f"{src} isn't there: clone https://github.com/Forrest404/unidex-apps beside this repo, or name a folder")
    out = os.path.join(root, "site", "store")
    apps, failed = [], False
    for src in sources:
        for d in sorted(os.listdir(src)):
            folder = os.path.join(src, d)
            if not os.path.isdir(folder) or d.startswith("."):
                continue
            entry, problems = check(folder)
            if problems:
                failed = True
                print(f"FAIL  {d}: " + "; ".join(problems))
                continue
            if any(a["id"] == entry["id"] for a in apps):
                failed = True
                print(f"FAIL  {d}: the id is used twice")
                continue
            entry["updated"] = updated(folder)
            entry["source"] = f"{source}/{entry['id']}" if source else ""
            apps.append(entry)
            print(f"ok    {entry['id']} {entry['version']} ({entry['bytes']} bytes)")
    if failed:
        sys.exit("some apps failed their checks: nothing written")
    shutil.rmtree(out, ignore_errors=True)
    for a in apps:
        for f in a["files"]:
            dest = os.path.join(out, "apps", a["id"], a["version"], f["path"])
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copyfile(f.pop("full"), dest)
    apps.sort(key=lambda a: a["name"].lower())
    json.dump({"api": API_LEVEL, "apps": apps}, open(os.path.join(out, "index.json"), "w"), indent=1)
    print(f"{len(apps)} apps in site/store/")


if __name__ == "__main__":
    main()
