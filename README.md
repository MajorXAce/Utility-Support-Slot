# Utility Support Slot v0.7.0

This mod allows you to carry one utility (Solo Silo designator or C4 detonator) alongside another
support weapon. Another utility replaces the utility; another ordinary support
weapon replaces the ordinary one. Two weapons of the same class do not form
a pair.

## Installation and requirements

Requires **Bingus Shared Loader; tested with v19**. Import this ZIP into Arsenal, enable **Core**
and choose exactly one mode under **Bindings**, then deploy and restart the game.
Core alone uses Default Bindings. Do not install both binding choices manually.
The root folders are `Core/`, `DefaultBindings/` and `DedicatedKey/`.

**Mod Options Menu** and **Mod Bindings Menu v2+** are optional (but recommended) and installed
separately.

## Controls

**Default Bindings:** press your support binding while support is already drawn
to swap the pair. On controller, hold the weapon-switch button while support is
equipped.

**Dedicated Key:** press **6** by default, or assign **Utility Weapon** through
Mod Bindings Menu, to draw the utility directly. Pressing it again leaves the
utility drawn, like vanilla. The other weapon keys behave as normal. If you only
have a utility weapon, it stays in the support slot and can be drawn with either
the support weapon binding or the utility weapon binding.

With **Mod Options Menu**, open Options > MODS > Utility Support Slot, choose
Binding Mode and press APPLY. The saved, applied menu choice takes priority over
Arsenal while the menu is available. Without it, Arsenal supplies the mode.
Release the control after changing modes before using it again.

With **Mod Bindings Menu**, open the MODS tab in the keyboard/controller options
and bind **Utility Weapon** under Utility Support Slot. It starts unbound. Once
you assign and use it, that binding replaces the INI shortcut for the session.
If the menu isn't installed or becomes unavailable, the INI shortcut is used.
Clearing the menu binding after takeover requires a restart to return to the INI.
The action is used only in Dedicated Key mode.

## Pickup and recovery

The **Switch Weapon On Pickup** game setting controls whether a pickup is
drawn. With No, the mod restores the previously selected weapon; replacing an
already drawn utility keeps the new utility drawn. Replacing either member keeps
the other member and drops the old weapon like vanilla.
Dropping or expending a member restores the survivor; dropping that final member
then leaves the pair empty. Existing refill and death-recovery handling remains.

## Multiplayer and known limitations

Multiplayer support was tested with the mod user **hosting a two-player game**.
The other player saw both stored weapons and correct draws, including a run with
Utility Support Slot disabled for the other player. Using the mod as a joining
player, larger groups, and completely vanilla observers haven't been tested.

- Your primary weapon may briefly appear during a swap or replacement.
- The mod hides extra pickup messages caused by its weapon handling. An incorrect
  utility pickup message can occasionally appear after replacing an ordinary
  support weapon while primary or secondary remains held with **Switch Weapon On
  Pickup** set to **No**. Weapon handling worked correctly in the tested cases;
  a fix for the message is planned for a future update.
- Replacements are occasional heavier operations; a noticeable hitch remains
  possible. Performance is something that will be an ongoing project.
- If something interrupts a weapon swap or pickup, the mod may stop handling weapons to avoid making things worse. Changes already made will remain. Recovery from these interruptions hasn’t been tested in-game yet, so you may need to start a new mission if the mod stops responding.

## INI and logging

Created on startup in either binding mode:
`%APPDATA%\Arrowhead\Helldivers2\utility_support_slot.ini`

```ini
utility_key=6
log_level=error
```

Edit and restart the game. Existing settings/comments are preserved; missing
settings are appended. Invalid values use the defaults above. The keyboard
fallback accepts a-z, 0-9, f1-f12, numpad0-numpad9, page up/down, home/end and
insert/delete. Saved older keys, including Page Up, are not overwritten.

Logging uses one **UtilitySupportSlot.log**, without a mod-defined size cap.
`error` is the default and may leave a clean session's file empty; `info` includes
readiness/mode messages and `debug` includes detailed gameplay/HUD/refill evidence.
For an issue report, include the version, host/client role, both weapons, pickup
setting, binding mode, what each player saw and the relevant log. Raising logging
requires an INI edit and restart.

## License

The project's original code is available under the [MIT license](LICENSE). This
license does not grant rights to Helldivers 2, its assets, or third-party material.
This is an unofficial project and is not affiliated with or endorsed by Arrowhead
Game Studios or Sony Interactive Entertainment.

## Credits

Created by **MajorXAce**. The gameplay logic is mine, with **GPT-6.1 Sol** helping
with research, implementation, debugging, and documentation.

Thanks to the creators of **Bingus Shared Loader**, **Mod Options Menu**, and
**Mod Bindings Menu**. Bingus Shared Loader provides mod loading and logging,
and this mod wouldn't exist without it. The optional menus make configuration
and keybinding easier.

I'd also like to thank the creators of **HD2Runtime** and **LinksLinkToMemory**
for their development and research tools. They helped me inspect live game state,
understand weapon and multiplayer behavior, and run experiments throughout
development.
