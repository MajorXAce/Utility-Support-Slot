# Utility Support Slot

This mod allows you to carry one utility (Solo Silo designator or C4 detonator)
alongside another support weapon. Two utilities or two ordinary support weapons
do not form a pair, so you cannot carry two EATs, for example.

This is the HellForge version. The original Lua version is preserved on the
[legacy branch](https://github.com/MajorXAce/Utility-Support-Slot/tree/legacy)
and in the older releases.

## Installation and Requirements

Requires [HellForge](https://github.com/SkyeShade/HellForge) on game build
**25480438**, with mod SDK **11 or newer**. Import the ZIP through
HellForge's mod manager and enable **Utility Support Slot**. Start or restart the
game through HellForge's launcher.
This mod only works for HellForge, not the current live game.

Remove any older USS test build or Lua version from the same setup before enabling
this version. They should not run together.

The **Mod Options Menu** and **Mod Bindings Menu**, included in CowboyBingus's
HellForge Vanilla Plus Megapack, are optional. If either is unavailable, USS uses
its own menu instead.

## Controls

**Default:** press your support weapon key while holding a support weapon to
swap the pair. On controller, hold the weapon-switch button instead.

**Dedicated:** use **Utility Weapon** to draw the utility directly. Other weapon
controls behave as normal. If you only have a utility, the normal support weapon
control also works.

Without the optional menu mods, open **Options > USS** to choose Binding Mode and
assign a key or controller button to Utility Weapon. The default is **6**.

With the optional menu mods, choose **Binding Mode** under
**Options > MODS > Utility Support Slot** and press **APPLY**. Assign
**Utility Weapon** under **Utility Support Slot** on the keyboard/controller
binding pages. You need to assign a control there before using Dedicated mode.
Release the control after changing modes before using it again.

The menu mods save their settings separately from USS's own menu. If you remove
them, USS goes back to the settings you used in its own menu.

## Pickup and Recovery

Pickups follow the game's **Switch Weapon On Pickup** setting. With **No**, you
keep using your previous weapon. If you replace a utility while holding it, the
new utility stays drawn.

Replacing one weapon keeps the other and drops the old one. Dropping or expending
one weapon makes the remaining weapon available again. Dying drops both weapons
like vanilla.

## Known Limitations and Multiplayer Support

Hosting and joining have been tested with two computers. More players are welcome
to try it and report issues.

- Your primary weapon may briefly appear during weapon changes.
- Players joining mid-mission may not see your stored weapon until you swap weapons.
- Resupplying from another player's supply backpack does not refill the stored weapon.
- Leaving a mission with a pair may lose the stored weapon.
- Mods that change support weapon pickup or inventory slots can conflict with USS.
- USS is currently incompatible with 8 Player Mod, even when playing solo.
- If a Silo pod is destroyed while its remote is stored, a ghost remote can remain
  attached until you die. It is a visual bug and shouldn't affect gameplay.

## Bug reports

If something goes wrong, describe what happened and attach the USS log. Include
your USS and HellForge versions, binding mode (Default or Dedicated), whether you
use a keyboard or controller, the weapons involved, whether you were hosting or
joining, and any other mods you use.

Open your HellForge folder, then **logs > sessions**. Open the folder for the
session where it happened and attach **mod-majorxace.utility-support-slot.log**.
If you are unsure which session to use, choose the newest folder. The log is in
that session folder, not in the mod's folder.

## Building

If you want to build the mod yourself, see [BUILDING.md](BUILDING.md).

## License

The project's original code is available under the [MIT license](LICENSE). This
license does not grant rights to Helldivers 2, its assets, or third-party material.
This is an unofficial project and is not affiliated with or endorsed by Arrowhead
Game Studios or Sony Interactive Entertainment.

## Credits

The gameplay logic is mine, but I had **GPT-6.1 Sol** and **Claude**
help with research, implementation, debugging, and documentation.

Big thanks to **CowboyBingus** for the Bingus Shared Loader, Mod Options Menu and
Mod Bindings Menu. The original Lua version and the native menu work benefited
from these projects. Reused menu code is covered by the included
[Zero-Clause BSD license](THIRD_PARTY_LICENSES.txt).

I'd also like to thank **SkyeShade**, creator of HellForge and HD2Runtime, and
**notzelda**, creator of LinksLinkToMemory, for their development and research
tools. They helped me inspect live game state, understand weapon and multiplayer
behavior, and run experiments throughout development.
