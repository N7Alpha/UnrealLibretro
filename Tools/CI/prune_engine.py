#!/usr/bin/env python
"""Capture a minimal, encrypted Unreal Engine build for CI plugin builds.

Run this on a machine that has an Epic Games Launcher (installed/"Rocket") build
of Unreal Engine. It stages the subset of the engine that `RunUAT BuildPlugin`
needs to compile and link a code plugin for Win64, packs it into AES-256
encrypted split 7z volumes (file names encrypted too), and optionally uploads
the volumes as assets on a `ue-cache/<major>.<minor>` prerelease of a GitHub
repository. CI runners then download, decrypt, and build against it -- see
Tools/CI/README.md for the full workflow and the Epic EULA considerations.

Typical usage:

    python Tools/CI/prune_engine.py "C:/Program Files/Epic Games/UE_5.3" ^
        --key-env ENGINE_CACHE_KEY --upload N7Alpha/UnrealLibretro

The staging step uses hardlinks when possible so it needs almost no extra disk
space on the same volume as the engine. Pass --dry-run first to see what would
be kept and how big it is.
"""

import argparse
import fnmatch
import hashlib
import json
import os
import shutil
import subprocess
import sys

# Directory subtrees that are never needed to compile/link a plugin.
# Paths are relative to the engine root, matched case-insensitively.
EXCLUDE_SUBTREES = [
    "Engine/DerivedDataCache",
    "Engine/Content",           # Editor assets; BuildPlugin compiles code only
    "Engine/Documentation",
    "Engine/Extras",
    "Engine/Saved",
    "Engine/Platforms",         # Optional platform extensions (UE5)
]

# Non-Windows platform directories pruned wherever they appear under these roots
PLATFORM_PRUNE_ROOTS = [
    "Engine/Binaries",
    "Engine/Intermediate",
    "Engine/Source/ThirdParty",
    "Engine/Plugins",
]
PLATFORM_DIR_NAMES = {
    "android", "arm64", "armv7", "html5", "hololens", "ios", "tvos", "visionos",
    "linux", "linux32", "linux64", "linuxarm64", "linuxaarch64", "unix",
    "mac", "mac64", "osx", "osx32", "osx64", "ps4", "ps5", "switch", "xboxone",
}

# Content folders inside plugins aren't needed to build against those plugins
PLUGIN_CONTENT_ROOT = "Engine/Plugins"

# File patterns dropped everywhere
EXCLUDE_FILE_PATTERNS = ["*.pdb"]

# Aggressive strip of Engine/Binaries/Win64: linking uses the import libraries
# under Engine/Intermediate/Build/Win64, not the editor DLLs, so the multi-GB
# UnrealEditor-*.dll / UE4Editor-*.dll set can go. UnrealHeaderTool must stay for
# UE < 5.3 (later versions run UHT inside UnrealBuildTool). Keep the small
# receipt/manifest files UBT reads, and ThirdParty runtime DLLs UHT may load.
BINARIES_WIN64 = "Engine/Binaries/Win64"
BINARIES_WIN64_KEEP_PATTERNS = [
    "unrealheadertool*",  # exe + its modular DLLs if the build has them
    "tbb*.dll",
    "*.target",
    "*.modules",
    "*.version",
    "*.json",
]


def norm(relpath):
    return relpath.replace(os.sep, "/").lower()


def is_excluded(relpath, keep_editor_binaries):
    """relpath is relative to the engine root, using the OS separator."""
    p = norm(relpath)
    parts = p.split("/")

    for subtree in EXCLUDE_SUBTREES:
        s = subtree.lower()
        if p == s or p.startswith(s + "/"):
            return True

    for root in PLATFORM_PRUNE_ROOTS:
        r = root.lower()
        if p.startswith(r + "/"):
            tail_parts = parts[len(r.split("/")):]
            if any(part in PLATFORM_DIR_NAMES for part in tail_parts[:-1]):
                return True

    # Engine/Plugins/**/Content/**
    pc = PLUGIN_CONTENT_ROOT.lower()
    if p.startswith(pc + "/") and "content" in parts[len(pc.split("/")):-1]:
        return True

    basename = parts[-1]
    for pattern in EXCLUDE_FILE_PATTERNS:
        if fnmatch.fnmatch(basename, pattern):
            return True

    if not keep_editor_binaries:
        b = BINARIES_WIN64.lower()
        if p.startswith(b + "/"):
            if not any(fnmatch.fnmatch(basename, keep) for keep in BINARIES_WIN64_KEEP_PATTERNS):
                if basename.endswith((".dll", ".exe")):
                    return True

    return False


def long_path(path):
    """Allow paths beyond MAX_PATH on Windows."""
    if os.name == "nt":
        path = os.path.abspath(path)
        if not path.startswith("\\\\?\\"):
            path = "\\\\?\\" + path
    return path


def read_engine_version(engine_root):
    version_path = os.path.join(engine_root, "Engine", "Build", "Build.version")
    with open(version_path, "r") as f:
        data = json.load(f)
    return data["MajorVersion"], data["MinorVersion"]


def collect_files(engine_root, keep_editor_binaries):
    kept, kept_bytes, dropped_bytes = [], 0, 0
    engine_dir = os.path.join(engine_root, "Engine")
    for dirpath, dirnames, filenames in os.walk(engine_dir):
        rel_dir = os.path.relpath(dirpath, engine_root)
        # Prune whole directories early so the walk stays fast
        dirnames[:] = [
            d for d in dirnames
            if not is_excluded(os.path.join(rel_dir, d), keep_editor_binaries)
        ]
        for name in filenames:
            rel = os.path.join(rel_dir, name)
            try:
                size = os.path.getsize(long_path(os.path.join(engine_root, rel)))
            except OSError:
                continue  # broken symlink etc.
            if is_excluded(rel, keep_editor_binaries):
                dropped_bytes += size
            else:
                kept.append(rel)
                kept_bytes += size
    return kept, kept_bytes, dropped_bytes


def gib(n):
    return f"{n / (1 << 30):.2f} GiB"


def report(kept, kept_bytes, dropped_bytes):
    by_top = {}
    for rel in kept:
        parts = norm(rel).split("/")
        top = "/".join(parts[:3]) if len(parts) > 3 else "/".join(parts[:-1])
        by_top[top] = by_top.get(top, 0) + 1
    print(f"Keeping {len(kept)} files, {gib(kept_bytes)} (pruned {gib(dropped_bytes)})")
    for top, count in sorted(by_top.items(), key=lambda kv: -kv[1])[:20]:
        print(f"  {count:>8} files  {top}")


def stage(engine_root, kept, staging_root):
    linked = copied = 0
    for i, rel in enumerate(kept):
        src = long_path(os.path.join(engine_root, rel))
        dst = long_path(os.path.join(staging_root, rel))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        try:
            os.link(src, dst)
            linked += 1
        except OSError:
            shutil.copy2(src, dst)
            copied += 1
        if (i + 1) % 20000 == 0:
            print(f"  staged {i + 1}/{len(kept)}...")
    print(f"Staged {linked} hardlinks, {copied} copies -> {staging_root}")


def archive(staging_root, output_dir, archive_name, key, volume_size):
    seven_zip = shutil.which("7z") or shutil.which("7za")
    if not seven_zip:
        sys.exit("7z not found on PATH. Install 7-Zip (https://www.7-zip.org/)")
    archive_path = os.path.join(output_dir, archive_name)
    for stale in os.listdir(output_dir) if os.path.isdir(output_dir) else []:
        if stale.startswith(archive_name):
            os.remove(os.path.join(output_dir, stale))
    os.makedirs(output_dir, exist_ok=True)
    subprocess.check_call(
        [
            seven_zip, "a", "-t7z", "-mx=5",
            "-mhe=on",              # encrypt file names, not just contents
            f"-v{volume_size}",
            f"-p{key}",
            archive_path,
            "Engine",
        ],
        cwd=staging_root,
    )
    volumes = sorted(
        os.path.join(output_dir, f)
        for f in os.listdir(output_dir)
        if f.startswith(archive_name)
    )
    return volumes


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_manifest(output_dir, major, minor, kept, kept_bytes, volumes):
    manifest = {
        "engine_version": f"{major}.{minor}",
        "file_count": len(kept),
        "uncompressed_bytes": kept_bytes,
        "volumes": [
            {"name": os.path.basename(v), "bytes": os.path.getsize(v), "sha256": sha256(v)}
            for v in volumes
        ],
    }
    path = os.path.join(output_dir, "manifest.json")
    with open(path, "w") as f:
        json.dump(manifest, f, indent=2)
    return path


def upload(repo, tag, files):
    view = subprocess.run(["gh", "release", "view", tag, "--repo", repo],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if view.returncode != 0:
        subprocess.check_call([
            "gh", "release", "create", tag, "--repo", repo, "--prerelease",
            "--title", f"Engine cache {tag.split('/', 1)[-1]}",
            "--notes", "AES-256 encrypted minimal Unreal Engine build for CI plugin builds. "
                       "Usable only by holders of the repository's ENGINE_CACHE_KEY secret, "
                       "who must have agreed to Epic's EULA. See Tools/CI/README.md.",
        ])
    subprocess.check_call(["gh", "release", "upload", tag, "--repo", repo, "--clobber"] + files)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("engine_root", help="Engine install root, e.g. C:/Program Files/Epic Games/UE_5.3")
    parser.add_argument("--output", default=None, help="Directory for staging + archives (default: ./ue-cache-<version>)")
    parser.add_argument("--key", default=None, help="Encryption password (prefer --key-env)")
    parser.add_argument("--key-env", default="ENGINE_CACHE_KEY", help="Environment variable holding the encryption password")
    parser.add_argument("--volume-size", default="1900m", help="7z volume size, must stay under GitHub's 2GiB release-asset limit")
    parser.add_argument("--upload", metavar="OWNER/REPO", default=None, help="Upload volumes to a ue-cache/<version> prerelease with gh")
    parser.add_argument("--keep-editor-binaries", action="store_true",
                        help="Don't strip Engine/Binaries/Win64 DLLs/EXEs (bigger but safer if a build fails without them)")
    parser.add_argument("--dry-run", action="store_true", help="Only report what would be kept")
    parser.add_argument("--stage-only", action="store_true", help="Stage the pruned tree but skip archiving/upload (for local verification)")
    args = parser.parse_args()

    engine_root = os.path.abspath(args.engine_root)
    major, minor = read_engine_version(engine_root)
    print(f"Engine version: {major}.{minor}")

    kept, kept_bytes, dropped_bytes = collect_files(engine_root, args.keep_editor_binaries)
    report(kept, kept_bytes, dropped_bytes)
    if args.dry_run:
        return

    output_dir = os.path.abspath(args.output or f"ue-cache-{major}.{minor}")
    staging_root = os.path.join(output_dir, "staging")
    if os.path.exists(staging_root):
        sys.exit(f"Staging directory already exists, remove it first: {staging_root}")
    stage(engine_root, kept, staging_root)

    if args.stage_only:
        print(f"\nStaged engine at: {staging_root}")
        print("Verify it works before uploading, e.g.:")
        print(f'  python package.py "{staging_root}"')
        return

    key = args.key or os.environ.get(args.key_env)
    if not key:
        sys.exit(f"No encryption key: pass --key or set {args.key_env}")

    archive_name = f"UE-{major}.{minor}-min.7z"
    volumes = archive(staging_root, output_dir, archive_name, key, args.volume_size)
    manifest_path = write_manifest(output_dir, major, minor, kept, kept_bytes, volumes)
    total = sum(os.path.getsize(v) for v in volumes)
    print(f"Archived {len(volumes)} volume(s), {gib(total)} compressed")

    shutil.rmtree(long_path(staging_root))

    if args.upload:
        tag = f"ue-cache/{major}.{minor}"
        upload(args.upload, tag, volumes + [manifest_path])
        print(f"Uploaded to https://github.com/{args.upload}/releases/tag/{tag.replace('/', '%2F')}")


if __name__ == "__main__":
    main()
