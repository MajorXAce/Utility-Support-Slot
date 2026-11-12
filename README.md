# Utility Support Slot

This mod allows you to carry one utility (Solo Silo designator or C4 detonator)
alongside another support weapon. Two utilities or two ordinary support weapons
do not form a pair, so you cannot carry two EATs, for example.

This is the HellForge version. The original Lua version is preserved on the
[legacy branch](https://github.com/MajorXAce/Utility-Support-Slot/tree/legacy)
and in the older releases.

## Installation and requirements

Requires [HellForge](https://github.com/SkyeShade/HellForge) on game build
**25480438**, with mod SDK **11 or newer**. Import the release ZIP through
HellForge's mod manager and enable **Utility Support Slot**, then restart.
This package is for HellForge, not the current live game.

Remove any older USS test build or Lua copy from the same setup before enabling
this version. They should not run together.

The C **Mod Options Menu** and **Mod Bindings Menu**, included in CowboyBingus's
HellForge Vanilla Plus Megapack, are optional. Integration currently supports
Megapack **0.2.1**. Without both working menus, USS uses its own menu instead.

## Controls

**Default:** press your support binding while your support weapon is already
drawn to swap the pair. On controller, hold the weapon-switch button while
support is drawn.

**Dedicated:** use **Utility Weapon** to draw the utility directly. Pressing it
again leaves the utility drawn, like vanilla. Other weapon controls behave as
normal. A lone utility can also be drawn with the normal support control.

Without the optional menus, open **Options > USS** to choose Binding Mode and
assign Utility Weapon. The standalone default is **6**; the assignment accepts
keyboard or controller input.

With the optional menus, choose **Binding Mode** under
**Options > MODS > Utility Support Slot** and press **APPLY**. Assign
**Utility Weapon** under **Utility Support Slot** on the keyboard/controller
binding pages. That external binding starts unbound. Release the control after
changing modes before using it again.

The external menus save their own choices. Your standalone choices are kept
separately and are used again if the external menus are unavailable.

## Pickup and recovery

Pickups follow the game's **Switch Weapon On Pickup** setting. With **No**, your
previously selected weapon is restored; replacing an already drawn utility keeps
the new utility drawn. Replacing either weapon keeps the other one and drops the
old weapon like vanilla. Dropping or expending one weapon restores the remaining
weapon. Dying drops both weapons like vanilla.

## Known limitations and multiplayer support

The game still has one support slot. The mod tracks the other weapon separately
and keeps it attached to your character.

Hosting and joining have been tested with two computers. More players are welcome
to try it and report issues.

- A brief switch through your primary weapon can be visible during handling.
- Players joining mid-mission may not see your stored weapon until you swap weapons.
- Resupplying from another player's supply backpack does not refill the stored weapon.
- Leaving a mission with a pair may lose the stored weapon.
- Mods that change support weapon pickup or inventory slots can conflict with USS.

## Building

Source and offline checks are included. See [BUILDING.md](BUILDING.md).

## License

The project's original code is available under the [MIT license](LICENSE). This
license does not grant rights to Helldivers 2, its assets, or third-party material.
This is an unofficial project and is not affiliated with or endorsed by Arrowhead
Game Studios or Sony Interactive Entertainment.

## Credits

The gameplay logic is mine, but I had **GPT-6.1 Sol**
help with research, implementation, debugging, and documentation.

Big thanks to **CowboyBingus** for the Bingus Shared Loader, Mod Options Menu and
Mod Bindings Menu. The original Lua version and the native menu work benefited
from these projects. Reused menu code is covered by the included
[Zero-Clause BSD license](THIRD_PARTY_LICENSES.txt).

I'd also like to thank **SkyeShade**, creator of HellForge and HD2Runtime, and
**notzelda**, creator of LinksLinkToMemory, for their development and research
tools. They helped me inspect live game state, understand weapon and multiplayer
behavior, and run experiments throughout development.
