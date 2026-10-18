# Building Utility Support Slot

This repository contains the readable Lua source and the files used to build the
installable mod. Python 3.10 or newer is required; building does not require the
game, HD2Runtime or a memory tool.

```sh
python tools/verify_public.py
python tools/build_release.py
python tools/verify_public.py
python tools/package_public.py --output-dir dist
```

The first check verifies the checked-in snapshot. After source edits, build first,
then verify. Core and the two binding choices stay at the repository root so an
installable ZIP keeps the same Arsenal layout. Only the approved installable
files enter that ZIP; Source/, tools/ and this guide stay in the source repo.
The packager reuses an identical ZIP and refuses to replace a version ZIP with
different content. Use a separate draft directory while developing a release.

Edit named Source/*.lua factories, then build to assemble their embedded copies.
Keep the readable Lua; there is no minification step. Verification checks assembly,
resource identities, preserved inert resources, reviewed source fingerprints and
exact source/patch agreement. It does not execute native gameplay or establish
multiplayer behavior; testing scope and known issues are in README.md.

## Changing player-facing text

Edit PLAYER_FACING_CONFIG.json, then preview the affected files:

```sh
python tools/player_config.py preview
python tools/player_config.py apply
python tools/build_release.py
python tools/verify_public.py
```

The tool coordinates display text, version messages and the reference inventory.
Renaming the Arsenal title updates unchanged category names; renaming an Arsenal
binding choice updates its matching in-game choice.
It preserves saved identifiers, resource paths and behavioral defaults, and stops
if owned text has been edited elsewhere. Review the diff before applying it.
The applied snapshot and reviewed fingerprints are updated only for the declared
text transformation. New gameplay changes need independent review and testing.
README prose is retained, with matching display names updated outside code and
link destinations; review any surrounding sentences after changing terminology.

PUBLIC_EXPORT.json records the development revision and hashes of the approved
exported files. The public repository has independent history; private research,
logs, personal paths and local test tools are not included. Building or packaging
never uploads, commits, tags or publishes a release.

## GitHub releases

On the public repository, open Actions > Prepare player release > Run workflow
on main. Leave Publish immediately off for a draft you can inspect first. The
workflow takes its tag from VERSION and uploads only the player ZIP; it refuses
an existing tag or release. Source pushes alone do not publish releases.
