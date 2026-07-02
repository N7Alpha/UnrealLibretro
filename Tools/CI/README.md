# CI Engine Cache

The `UnrealLibretro` workflow builds the plugin on GitHub-hosted Windows runners, in parallel across engine versions, with no self-hosted runner. Hosted runners have neither Unreal Engine nor the disk for a full install (~100+ GB), but a pruned "installed build" containing only what `RunUAT BuildPlugin` needs (sources, headers, import libraries, build tools) fits comfortably.

How it fits together:

1. `Tools/CI/prune_engine.py` runs **once per engine version on a machine that has that engine installed** (e.g. your dev machine with the Epic Games Launcher builds). It stages the minimal subset, packs it into AES-256-encrypted split 7z volumes, and uploads them as assets on a `ue-cache/<major>.<minor>` prerelease of this repository.
2. The `discover-engines` job in `.github/workflows/UnrealLibretro.yml` lists the `ue-cache/*` releases and generates the build matrix from them — adding an engine version to CI is just uploading a new cache.
3. Each matrix job downloads its volumes, verifies them against `manifest.json`, decrypts them with the `ENGINE_CACHE_KEY` repository secret, extracts to `C:\UE`, and runs `setup.cmd` + `package.py` as before.

Release assets are used instead of Actions artifacts because artifacts expire after at most 90 days and can only be uploaded from inside a workflow run — and the machine with the engine on it isn't a runner anymore. Release assets are permanent and capped at 2 GiB per file, which the 7z volume splitting stays under.

## One-time setup

1. Generate a strong key and store it as a repository secret:

   ```sh
   # Any long random string works; this is a symmetric encryption password
   python -c "import secrets; print(secrets.token_urlsafe(48))"
   gh secret set ENGINE_CACHE_KEY --repo N7Alpha/UnrealLibretro
   ```

   Keep a copy somewhere safe — you need the same key locally every time you capture an engine.

2. For each engine version, on the machine that has it installed:

   ```powershell
   $env:ENGINE_CACHE_KEY = "<the same key>"

   # See what would be kept and how big it is
   python Tools/CI/prune_engine.py "C:/Program Files/Epic Games/UE_5.3" --dry-run

   # Recommended for the first capture of a version: stage without archiving and
   # test a real plugin build against the pruned tree before uploading anything
   python Tools/CI/prune_engine.py "C:/Program Files/Epic Games/UE_5.3" --stage-only
   python package.py "ue-cache-5.3/staging"

   # Then archive, encrypt, and upload in one go
   python Tools/CI/prune_engine.py "C:/Program Files/Epic Games/UE_5.3" --upload N7Alpha/UnrealLibretro
   ```

   Staging uses hardlinks, so it needs almost no extra disk space when the output directory is on the same drive as the engine. The archive itself is roughly 10–20 GB per version depending on engine generation.

3. Push to `master` (or run the workflow manually) — the matrix picks up every version you uploaded.

## Refreshing or removing a version

Re-running `prune_engine.py --upload` replaces the assets on the existing `ue-cache/<version>` release (`--clobber`). To drop a version from CI, delete its release:

```sh
gh release delete ue-cache/4.24 --repo N7Alpha/UnrealLibretro --yes
```

## If a build fails with missing engine files

The pruning is deliberately aggressive; the riskiest cut is stripping the editor DLLs out of `Engine/Binaries/Win64` (linking uses the import libraries under `Engine/Intermediate/Build/Win64`, and `UnrealHeaderTool` is kept for UE < 5.3 — but a UBT version might unexpectedly probe a binary). If a version fails on missing files, recapture it with:

```powershell
python Tools/CI/prune_engine.py "C:/.../UE_X.Y" --keep-editor-binaries --upload N7Alpha/UnrealLibretro
```

and if that fixes it, note the offending file so the keep-list in `prune_engine.py` can be tightened instead.

## Known limitations

- **UE4 versions need the VS2019 (v142) toolchain**, which GitHub's `windows-2022`/`windows-2025` images no longer include. The workflow installs VS2019 Build Tools via Chocolatey for `4.x` matrix entries (~10–20 min per job) and passes `--compiler VS2019` to `package.py`. If Epic's UBT in a given 4.x version refuses to discover the Build Tools SKU, that version can't build on hosted runners.
- `package.py` no longer hardcodes `-VS2019`; pass `--compiler VS2019` explicitly when building UE4 locally.
- Hosted Windows runners put the workspace on the small `D:` drive; the engine extracts to `C:\UE`, which has substantially more free space. If extraction ever runs out of disk, the pruning needs to get more aggressive (or the version dropped).
- Fork PRs can't read `ENGINE_CACHE_KEY` (GitHub doesn't expose secrets to fork PR runs); the workflow only triggers on pushes to this repository's branches and manual dispatch, same as before.

## Epic EULA considerations

Unreal Engine may not be redistributed publicly. The caches this system publishes are AES-256 encrypted with `-mhe=on` (file *names* are encrypted too, so nothing about the engine contents is exposed), and the key lives only in this repository's secrets and with the maintainer. Only people you deliberately give the key to — who should themselves have accepted Epic's EULA, which is free — can make any use of the assets. This is the same spirit as sharing engine tools privately between licensees rather than public distribution. It is your responsibility to keep the key private and to satisfy yourself that this arrangement complies with the EULA; when in doubt, host the `ue-cache/*` releases on a private repository instead and point the workflow's `gh release download` at it with a PAT.
