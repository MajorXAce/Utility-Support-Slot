# Utility Support Slot v0.7.0

Carry one utility (Solo Silo designator or C4 detonator) alongside another support
weapon, and switch between them when you need to.

## What the mod does

- Carry one utility and one ordinary support weapon together. Picking up another
  utility replaces the utility; another ordinary support weapon replaces the
  ordinary one.
- Choose **Default Bindings** to swap using your normal support controls, or
  **Dedicated Key** to draw the utility directly. The dedicated shortcut is **6**
  by default, or you can assign **Utility Weapon** through Mod Bindings Menu.
- Follow the game's **Switch Weapon On Pickup** setting. With **No**, the mod
  restores your previously selected weapon. Replacing an already drawn utility
  keeps the new utility drawn.
- Keep the remaining weapon when the other is dropped or expended. The mod also
  includes refill and death-recovery handling for the carried pair.

## Requirements

Requires **Bingus Shared Loader; tested with v19**. **Mod Options Menu** and
**Mod Bindings Menu v2+** are optional but recommended for changing modes and
setting your utility keybind.

See the [README](README.md) for installation, controls and configuration.

## Multiplayer and known issues

Multiplayer was tested while **hosting a two-player game**. The other player saw
both stored weapons and correct draws, including a test with Utility Support Slot
disabled for that player. Using the mod as a joining player, larger groups, and
completely vanilla observers haven't been tested.

- Your primary weapon may briefly appear during a swap or replacement.
- An incorrect utility pickup message can occasionally appear after replacing an
  ordinary support weapon while primary or secondary remains held with **Switch
  Weapon On Pickup** set to **No**. Weapon handling worked correctly in the tested
  cases; a fix for the message is planned for a future update.
- Replacing a weapon can cause a noticeable hitch. Performance will be an ongoing
  project.
- If something interrupts a swap or pickup, the mod may stop handling weapons.
  Changes already made will remain. Recovery from these interruptions hasn't
  been tested in-game yet, so you may need to start a new mission if the mod stops
  responding.
