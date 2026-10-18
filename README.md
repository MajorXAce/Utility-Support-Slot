# Utility Support Slot v0.7.1

This mod allows you to carry one utility (Solo Silo designator or C4 detonator)
alongside another support weapon. Two utilities or two ordinary support weapons
do not form a pair, so you cannot carry two EATs, for example.

This mod is intended for single-player and private lobbies. Please respect other
players and check with your group before using it.

## Installation and requirements

Requires **[Bingus Shared Loader](https://github.com/CowboyBingus/BingusSharedLoader/releases)**;
tested with **v19**. Import the mod ZIP into your mod manager (for example,
Arsenal), enable **Core** and choose exactly one mode under **Bindings**, then
deploy and restart the game. Core alone uses Default Bindings. Do not install
both binding choices manually.

**Mod Options Menu** and **Mod Bindings Menu v2+** are optional (but recommended)
and installed separately.

## Controls

**Default Bindings:** press your support binding while your support weapon is
already drawn to swap the pair. On controller, hold the weapon-switch button
while support is drawn.

**Dedicated Key:** press **6** by default, or assign **Utility Weapon** through
Mod Bindings Menu, to draw the utility directly. Pressing it again leaves the
utility drawn, like vanilla. The other weapon keys behave as normal. If you only
have a utility weapon, it stays in the support slot and can be drawn with either
the support weapon binding or the utility weapon binding.

With **Mod Options Menu**, open **Options > MODS > Utility Support Slot**, choose
**Binding Mode** and press **APPLY**. The saved, applied menu choice takes priority
over your mod manager while the menu is available. Without it, the mod uses the
mode selected in your mod manager. The menu lets you change modes in-game;
release the control after changing modes before using it again.

With **Mod Bindings Menu**, open the **MODS** tab in the keyboard/controller
options and bind **Utility Weapon** under **Utility Support Slot**. It starts
unbound. Once you assign and use it, that binding replaces the INI shortcut for
the session. If the menu isn't installed or becomes unavailable, the INI shortcut
is used. If you clear a menu binding after using it, restart the game to use the
INI shortcut again. The binding is used only in Dedicated Key mode.

## Pickup and recovery

Pickups follow the game's **Switch Weapon On Pickup** setting. With **No**, your
previously selected weapon is restored; replacing an already drawn utility keeps
the new utility drawn. Replacing either weapon keeps the other one and drops the
old weapon like vanilla. Dropping or expending one weapon restores the remaining
weapon. Dying drops both weapons like vanilla.

## Known limitations and multiplayer support

The game still has one support slot. The mod tracks the other weapon separately
and keeps it attached to your character. I tried my best to make it feel like a
vanilla feature, but there will be cases where the gaps show.

Multiplayer was tested while hosting, joining, with both modded and vanilla combinations. Larger groups and extended multiplayer sessions haven't been tested.

- Resupplying from another player's supply backpack will not refill the weapon
  being stored by the mod.
- Players joining mid-mission may not see your stored weapon until you swap weapons.
- Leaving a mission while carrying a pair may cause the stored weapon to be
  lost. Keep this in mind before leaving; this behavior hasn't been fully
  tested.
- Your primary weapon may briefly appear during a swap or replacement especially in default mode.
- The mod hides extra pickup messages caused by its weapon handling. An incorrect
  utility pickup message can occasionally appear after replacing an ordinary
  support weapon while primary or secondary remains held with **Switch Weapon On
  Pickup** set to **No**. Weapon handling worked correctly in the tested cases;
  a fix for missed messages is planned for a future update.
- Replacing support weapons or utilities can cause a noticeable hitch. Normal
  swaps were stable in my testing; performance work will continue.
- If something interrupts a weapon swap or pickup, the mod may stop handling
  weapons to avoid making things worse. Changes already made will remain.
  Recovery from these interruptions hasn't been tested in-game yet, so you may
  need to start a new mission if the mod stops responding.

## Compatibility

Mods that change support weapon pickups or let you carry support weapons in other
inventory slots may conflict with this mod. This mod only pairs
weapons picked up through the normal support slot; weapons in primary or secondary
aren't included in its pair. Other mods may redirect pickups away from that slot
or interfere with how carried weapons are handled.

**Second Amendment to Liberty is currently incompatible with this mod's paired
weapon handling.** Testing both mods together caused pickup prompts to appear on
carried weapons managed by this mod. Use the mods separately for now.

Other mods that change support weapon pickups or slots haven't been fully tested.
If you encounter similar issues, try disabling those mods.

## Future plans

I'd like to improve a few things in future updates:

- Smoother swaps without briefly switching to your primary weapon.
- Support for melee support weapons, with an option to treat them as a utility
  or an ordinary support weapon. This would let you pair one with a gun or with
  Solo Silo/C4, depending on your preference. The setting would be available
  through Mod Options Menu, with a mod manager option as the fallback.
- Support for melee secondaries as utilities. They would always replace your
  current utility and keep your ordinary support weapon. This depends on finding
  a reliable way to drop, pick up and recover them.
- Further performance improvements and fixes for the remaining pickup messages.

Melee support still needs testing before I can confirm which weapons will work.

## INI and logging

An INI file is created on startup where you can change the shortcut key and
logging level:

`%APPDATA%\Arrowhead\Helldivers2\utility_support_slot.ini`

```ini
utility_key=6
log_level=error
```

Edit the settings and restart the game. You can change `utility_key` for
Dedicated Key mode if you aren't using Mod Bindings Menu. It accepts a-z, 0-9,
f1-f12, numpad0-numpad9, page up/down, home/end and insert/delete.

Find **UtilitySupportSlot.log** in Bingus Shared Loader's log folder:

`%LOCALAPPDATA%\CowboyBingus\Helldivers2\Logs\UtilitySupportSlot.log`

`error` is the default and reports failures, so a clean session may leave an
empty file. `info` includes readiness/mode messages, and `debug` includes detailed
weapon, HUD and resupply events. For an issue report, include the mod version,
whether you were hosting or joining, both weapons, the **Switch Weapon On Pickup**
setting, binding mode, what each player saw and the relevant log. Raising logging
requires an INI edit and restart.

## License

The project's original code is available under the [MIT license](LICENSE). This
license does not grant rights to Helldivers 2, its assets, or third-party material.
This is an unofficial project and is not affiliated with or endorsed by Arrowhead
Game Studios or Sony Interactive Entertainment.

## Credits

The gameplay logic is mine, but I had **GPT-6.1 Sol**
help with research, implementation, debugging, and documentation.

Big thanks to **CowboyBingus**, the creator of **Bingus Shared Loader**,
**Mod Options Menu**, and **Mod Bindings Menu**. Bingus Shared Loader provides
mod loading and logging, and this mod wouldn't exist without it. The optional
menus make configuration and keybinding easier.

I'd also like to thank **SkyeShade**, creator of **HD2Runtime**, and **notzelda**,
creator of **LinksLinkToMemory**, for their development and research tools. They
helped me inspect live game state, understand weapon and multiplayer behavior,
and run experiments throughout development.
