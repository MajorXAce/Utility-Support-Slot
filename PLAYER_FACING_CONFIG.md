# Player-facing configuration and text inventory

Current v0.7.0 text is recorded verbatim below. Descriptions use the same plain
language as the README; labels, defaults and saved identifiers are preserved.
Edit display text or the version in `PLAYER_FACING_CONFIG.json`, then run:

```sh
python tools/player_config.py preview
python tools/player_config.py apply
python tools/build_release.py
python tools/verify_public.py
```

Preview shows the coordinated diff without writing files. Apply updates the
owning source, assembled copies, manifest, matching document names and this
inventory. Saved identifiers, paths and behavioral defaults are protected.
Renaming the Arsenal mod title also updates unchanged category names; renaming
a binding choice updates its matching menu choice. Review surrounding prose.
Use `python tools/player_config.py check` to detect drift. Packaging is separate;
`python tools/package_public.py --output-dir dist` creates only the installable ZIP.

## Arsenal

Owner: `manifest.json` (under `release/` in the development checkout). Core contains both runtime addons; DefaultBindings
and DedicatedKey contain only a mode-setting resource each. Every option/choice
uses `Image: icon.png`; the top-level preview uses `IconPath: icon.png`.

| Field | Current text |
| --- | --- |
| Mod title | Utility Support Slot |
| Mod description | Carry a Solo Silo designator or C4 detonator alongside another support weapon. Choose normal support controls or a separate Utility Weapon keybind. Pickups follow the game's Switch Weapon On Pickup setting. Requires Bingus Shared Loader (tested with v19). Mod Options Menu and Mod Bindings Menu are optional. |
| Core label | Core |
| Core description | Required for the mod to work. Enable Core, then choose one binding mode below. |
| Bindings label | Bindings |
| Bindings description | Choose how you switch to your utility weapon. |
| Default Bindings label | Default Bindings |
| Default Bindings description | Swap between your utility and support weapon using your normal support binding while support is drawn. On controller, hold the weapon-switch button while support is equipped. |
| Dedicated Key label | Dedicated Key |
| Dedicated Key description | Draw your utility directly with 6 by default. Change the key in utility_support_slot.ini or assign Utility Weapon through Mod Bindings Menu. |

Arsenal chooses which patch is installed. The manifest defines no initial
selection/default field. Core-only falls back to Default Bindings. `Version: 1`
is the manifest format, not the mod release number. The mod release number is
in `VERSION`, release notes and runtime readiness messages.

## In-game Mod Options Menu

Owner: `Source/binding_mode_options.lua` (assembled into the main core).

| Field | Current value |
| --- | --- |
| Mod name | Utility Support Slot |
| Option label | Binding Mode |
| Choices | Default Bindings; Dedicated Key |
| Description | Default Bindings swaps weapons using your normal support controls. Dedicated Key draws the utility with 6 by default or your Utility Weapon binding. Press APPLY to change modes. |
| Default | Installed Arsenal mode: 1 = Default Bindings, 2 = Dedicated Key |
| Saved option ID | `utility_support_slot.binding_mode` |
| Mod ID | `utility_support_slot` |

The saved/applied in-game choice takes precedence over Arsenal while the menu
API is available. APPLY changes the mode live. Without it, Arsenal is the fallback.
There is no mod-defined per-option icon field in this menu integration.

## In-game Mod Bindings Menu

Owner: `Source/utility_binding_registration.lua`; activation logic is in
`Source/utility_shortcut.lua`. Both are assembled into the main core.

| Field | Current value |
| --- | --- |
| Category | Utility Support Slot |
| Action label | Utility Weapon |
| Saved action ID | `utility_support_slot.draw_utility` |
| Default mapping | Unbound (`nil`) |
| Description | No separate description is registered through this API |

The action is visible in either mode, but is read only in Dedicated Key mode.
The INI key is the fallback until the first accepted menu action, which selects
menu input for the session. An unavailable menu restores INI fallback. Merely
registering the row does not prove it is bound; this API exposes action state,
not a mapping-count query. Clearing a binding after menu takeover is therefore
not detected as becoming unbound during the same session.

## INI

Owner: `Source/utility_configuration.lua`. Path:
`%APPDATA%\Arrowhead\Helldivers2\utility_support_slot.ini`.
Initialized in both modes; edits require a game restart. Existing values/comments
are preserved; missing settings are appended. Invalid values use defaults.

| Setting | New-file default | Accepted values | Purpose |
| --- | --- | --- | --- |
| `utility_key` | `6` | a–z, 0–9, f1–f12, numpad0–9, page up/down, home/end, insert/delete | Keyboard fallback for Dedicated Key mode |
| `log_level` | `error` | error, info, debug (case-insensitive) | Select detail in UtilitySupportSlot.log |

Key names are case-insensitive; spaces, underscores and hyphens are ignored.
INI field names are case-sensitive. The first occurrence of a recognized field
wins; unknown fields are ignored. There is no INI binding-mode setting.

New-file template, verbatim:

```ini
; Utility Support Slot
; Restart after editing. log_level: error, info, debug
; utility_key: a-z, 0-9, f1-f12, numpad0-9, page up/down, home/end, insert/delete
utility_key=6
log_level=error
```

`error` is the lowest verbosity and can produce an empty file in a clean session.
`info` includes normal readiness/mode messages. `debug` enables detailed gameplay,
HUD/refill evidence and counters. Severity routing is owned by
`Source/utility_logging.lua`; several detailed success lines remain labeled INFO
but are emitted only with debug enabled. These are logging levels, not gameplay
options. The active log filename is owned by `Source/utility_support_slot.lua`.

There is no mod-defined file-size cap. `Source/utility_log_writer.lua` owns the
writer; a failed write/flush disables that broken handle for the session so the
update loop does not repeatedly hit a filesystem error. Verbosity is unchanged.

## Game-owned setting the mod observes

`Switch Weapon On Pickup` / `switch_weapon_on_pickup` is a game setting, not a
mod option. `Source/pickup_preferences.lua` reads the verified live preference.
Our mod does not define its label, description or default.

## Stable identifiers and packaging

Change display labels/descriptions freely, but changing saved IDs would create
new settings/actions and orphan prior bindings/preferences. `Guid` in the Arsenal
manifest and runtime resource markers are identity, not display text. Keep them.
`icon.png` is used for the mod and Arsenal option previews.

The public README and release notes are updated for 0.7.0. Registered Arsenal,
menu, action and INI text/defaults were checked against the owning source without
changing identifiers or behavior. The configuration workflow checks manifest/menu/action/INI agreement against
the applied snapshot. Manual edits to owned text must be reconciled before apply;
the tool does not overwrite an existing player INI or change gameplay settings.
