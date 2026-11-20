# Building Utility Support Slot

Players only need the release ZIP. These instructions are for building from source
on Windows x64. Builds do not install the mod or start the game.

Install Python3.10+ and Visual Studio C++ Build Tools, including MSVC and a Windows
SDK. The build tool locates its x64 developer environment automatically.

Fetch the five public SDK headers at the reviewed HellForge commit:

```powershell
python Tools/sdk_inputs.py --fetch
python Tools/build.py --compiler msvc --test --package
```

Downloads are hash-checked against Tools/sdk_baseline.json and stored in ignored
.sdk/. Alternatively set USS_SDK to your own directory containing those exact
headers. No game binaries, SDK binaries or private development evidence are needed.

The DLL and receipt are in build/. The player ZIP is in dist/. Existing ZIPs are
never replaced with different bytes under the same version; use a new candidate
identifier while iterating. No credentials, settings or source are included in the
player ZIP. Source remains available here on GitHub.

Zig is optional: set USS_ZIG to the reviewed0.17.0 executable and pass
--compiler zig. The Windows MSVC build does not require Zig.

The manual GitHub release workflow runs the offline checks, builds and packages
from main, and uploads a review artifact by default. Release creation requires an
explicit publish input after review. Existing releases and tags are preserved.
The separate replace_mistaken_v083_tag input permits only the approved v0.8.3
replacement, and only while its old commit still matches the recorded value.
A concurrent tag change or an authentication/network failure stops publication.
Lua history and its old workflow are preserved on the legacy branch.

Offline checks validate authored fixtures and refusal paths; passing them is not
in-game or multiplayer proof. Future game builds need a separate compatibility review.

The player ZIP contains the native DLL, manifest, icon, README and licences.
Menu support is built into the DLL; no separate menu add-on or Lua data pack is
needed. Use --candidate followed by a label to give local test packages distinct
filenames without changing the version. Release publication is a separate step.
