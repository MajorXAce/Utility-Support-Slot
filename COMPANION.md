# Lua Menu Companion

Use this optional ZIP with the Lua Mod Options Menu and Mod Bindings Menu and
Bingus Shared Loader. Install the matching Utility Support Slot version too.
Import both ZIPs with HellForge's mod manager. C menu providers and standalone
USS do not need this companion.

Choose Binding Mode in MODS and assign Utility Weapon in the binding menu.
With Lua menus, 6 works until the first accepted Utility Weapon binding press.
The menu providers save their own settings. Remove older USS companion builds
before enabling this version.

The companion uses LuaJIT ffi and the Bingus loader lifecycle. It calls no
HD2Runtime API; HellForge's launcher may still warn about its Lua dependencies.
