# Utility Support Slot 0.8.1

Bug-fix and compatibility patch for the native HellForge version of Utility
Support Slot (game build 25480438).

## Fixed
- Support weapons added by other mods, such as the Compendium EAT-77 and
  Beelzebub's Strat Pack support weapons, are now recognized as ordinary support
  weapons. They pair with a Solo Silo designator or C4 detonator, and two
  ordinary support weapons still do not pair.
- Resupply no longer tries to refill one-shot support weapons added by other
  mods, such as the EAT-77 clones.
- The pickup prompt no longer stays up for a weapon you are carrying in storage.
- Other players' pings can no longer target the weapon you have stored, matching
  the original Lua version.
- If the stored weapon disappears, the pair is cleared cleanly, and the log now
  names the weapons and the reason when a check is declined.
- The supported Mod Options Menu and Mod Bindings Menu are now recognized by
  their API version instead of one exact file, so newer releases of those menu
  mods keep working with this version.

## Known issue
- After a Solo Silo is destroyed, a ghost Silo can remain visible on your
  character until you die.

The original Lua releases remain available on the legacy branch and in the
release history.
