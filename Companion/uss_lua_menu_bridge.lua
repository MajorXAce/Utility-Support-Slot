-- HD2-Addon: mods/majorxace/uss_menu_bridge
--[[
Utility Support Slot: optional Lua menu companion.

WHAT IT DOES
  When the Lua Mod Options Menu and Mod Bindings Menu are installed, USS (the C mod) cannot talk to them directly. This small addon registers USS's two entries with
  those menus through their ordinary public API, reads what the player applied, and posts a fixed 64-byte record to the ALREADY-LOADED USS DLL through one exported
  function. USS then shows no second tab of its own while this bridge is healthy, so the menus' tab-count assumptions are not disturbed.

WHAT IT DELIBERATELY DOES NOT DO
  * No game memory, no native call, no UI work, no file or save writes of its own: the providers own the saved mode and the saved key mapping.
  * It never loads or injects a DLL. It finds USS by its manifest module name with GetModuleHandleA (the same way USS finds the Megapack), then two exports by name.
  * It never translates a binding to a Windows/XInput code: it forwards only the provider's evaluated "is down" answer.
  * It posts nothing to USS unless USS's own export exists; if USS or the exports are missing it only retries slowly and logs once.

FAILURE MODEL
  Any error is caught (pcall) and counted; after 8 errors in a row the bridge stops posting and USS falls back to its own menu by lease expiry (about 1 second).
  A restart of this addon creates a new instance id, which USS treats as a new provider (release-before-resume).

RECORD (must match src/uss_lua_menu_mailbox.h; checked by Tools/check_uss_lua_companion.py)
  magic 'USMB'  protocol 2  size 64  flags bit0 = identified options provider  instance (u64)  sequence (u64, +1 per post)
  options_registered bindings_registered ready mode(1 default, 2 dedicated) input_valid utility_down action_blocked generation(nonzero)
]]
local ffi = require('ffi')

-- Configuration. MODULE_NAME is USS's DLL file name from its hellforge-mod.json (code.dll); it is a name, never an install path.
local CONFIG = {
    MODULE_NAME = 'utility_support_slot.dll',
    OPTION_ID = 'majorxace.uss.binding_mode',
    BINDING_ID = 'majorxace.uss.utility_weapon',
    MOD_NAME = 'Utility Support Slot',
    MOD_ID = 'majorxace.uss',
    HEARTBEAT_MS = 250,       -- USS's lease is 1000 ms: four heartbeats per lease
    REGISTER_RETRY_MS = 1000, -- provider registration retries, as the legacy Lua USS does
    MAX_REGISTER_ATTEMPTS = 60,
    DISCOVERY_RETRY_MS = 2000,
    MAX_CONSECUTIVE_ERRORS = 8,
}
local MAGIC, PROTOCOL, SIZE = 0x424D5355, 2, 64

-- Private type/function names (ffi.cdef keeps the FIRST prototype declared for a plain name in the whole game; a private name cannot collide with another mod's declaration).
ffi.cdef [[
typedef struct { unsigned int magic, protocol, size, flags; unsigned long long instance, sequence;
                 unsigned int options_registered, bindings_registered, ready, mode, input_valid, utility_down, action_blocked, generation; } USSMB_post;
typedef int (*USSMB_post_fn)(const void* data, unsigned int size);
typedef unsigned int (*USSMB_mode_fn)(void);
void* USSMB_GetModuleHandleA(const char* name) __asm__("GetModuleHandleA");
void* USSMB_GetProcAddress(void* module, const char* name) __asm__("GetProcAddress");
unsigned long long USSMB_GetTickCount64(void) __asm__("GetTickCount64");
]]

local function new_bridge(env)
    local self = {}
    local state = {
        instance = nil, sequence = 0, generation = 1, post_fn = nil, mode_fn = nil, next_discovery = 0, exports_logged = false,
        options_menu = nil, bindings_menu = nil, registered_options = false, registered_bindings = false, subscribed = false,
        attempts_options = 0, attempts_bindings = 0, next_register = 0, choice = nil,
        last_post_ms = -1e9, last_key = nil, errors = 0, stopped = false, notes = {},
    }
    local record = ffi.new('USSMB_post')

    local function note(text)
        -- once per distinct text: bounded, no per-frame logging
        if not state.notes[text] then state.notes[text] = true; if env.log then pcall(env.log, 'USS menu bridge: ' .. text) end end
    end
    -- An instance id that differs per run: tick count mixed with an address-derived value; never zero.
    local function make_instance(now)
        local mix = tonumber(ffi.cast('unsigned long long', ffi.cast('void*', record)) % 4294967296ULL) or 1
        local id = (ffi.cast('unsigned long long', math.floor(now)) * 4294967296ULL) + mix
        if id == 0ULL then id = 1ULL end
        return id
    end

    local function discover(now)
        if state.post_fn then return true end
        if now < state.next_discovery then return false end
        state.next_discovery = now + CONFIG.DISCOVERY_RETRY_MS
        local module = env.get_module(CONFIG.MODULE_NAME)
        if module == nil then note('USS module not loaded yet'); return false end
        local post = env.get_proc(module, 'uss_lua_menu_post_v2')
        local mode = env.get_proc(module, 'uss_lua_menu_default_mode_v1')
        if post == nil or mode == nil then note('USS exports unavailable (older USS without the companion interface)'); state.next_discovery = now + 30000; return false end
        state.post_fn = ffi.cast('USSMB_post_fn', post); state.mode_fn = ffi.cast('USSMB_mode_fn', mode)
        note('USS companion exports found')
        return true
    end

    local function valid(value) return value == 1 or value == 2 end

    -- Missing ready() is supported by older providers; a present method must return true.
    -- Identity is the named global plus the supported menu API shape, never a native tab label.
    local function options_present(menu)
        return type(menu) == 'table' and menu.api == 1 and menu.version == 3
            and type(menu.register_option) == 'function' and type(menu.get) == 'function'
            and type(menu.on_change) == 'function'
    end

    local function provider_ready(menu, bindings)
        if type(menu) ~= 'table' or menu.api ~= 1 then return false end
        if bindings then
            if type(menu.version) ~= 'number' or menu.version < 2 or type(menu.register_binding) ~= 'function' or type(menu.is_down) ~= 'function' then return false end
        elseif type(menu.register_option) ~= 'function' or type(menu.get) ~= 'function' or type(menu.on_change) ~= 'function' then return false end
        if menu.ready == nil then return true end
        if type(menu.ready) ~= 'function' then return false end
        local ok, ready = pcall(menu.ready)
        return ok and ready == true
    end

    local function poll_options(now, menu)
        -- Same provider contract and retry budget as the legacy Lua USS (binding_mode_options.lua): api==1, register once, read the APPLIED value, never set().
        local usable = options_present(menu)
        if not usable or state.attempts_options >= CONFIG.MAX_REGISTER_ATTEMPTS then return end
        if type(menu.ready) == 'function' then local ok, ready = pcall(menu.ready); if not ok or ready ~= true then return end end
        if not state.registered_options then
            local default_mode = tonumber(state.mode_fn())
            if not valid(default_mode) then return end   -- USS has not published its standalone mode yet (retry)
            state.attempts_options = state.attempts_options + 1
            local ok, accepted, reason = pcall(menu.register_option, CONFIG.OPTION_ID, {
                type = 'choice', label = 'Binding Mode', mod = CONFIG.MOD_NAME, mod_id = CONFIG.MOD_ID, choices = { 'Default', 'Dedicated' }, default = default_mode,
                description = 'Default uses the normal support weapon control. Dedicated uses Utility Weapon.' })
            state.registered_options = ok and accepted == true
            if not state.registered_options then note('option registration refused: ' .. tostring(ok and reason or accepted)) end
        end
        if state.registered_options and not state.subscribed then
            local options = menu
            local ok, accepted = pcall(menu.on_change, CONFIG.OPTION_ID, function(value)
                -- Applied edits only. Ignore a callback from a replaced provider.
                if env.get_global('ModOptionsMenu') == options then state.choice = valid(value) and value or nil end
            end)
            state.subscribed = ok and accepted ~= false
        end
        if state.registered_options and state.subscribed and not valid(state.choice) then
            local ok, value = pcall(menu.get, CONFIG.OPTION_ID)
            if ok and valid(value) then state.choice = value end
        end
    end

    local function poll_bindings(now, menu)
        local usable = type(menu) == 'table' and menu.api == 1 and type(menu.version) == 'number' and menu.version >= 2 and type(menu.register_binding) == 'function' and type(menu.is_down) == 'function'
        if not usable or state.registered_bindings or state.attempts_bindings >= CONFIG.MAX_REGISTER_ATTEMPTS then return end
        if type(menu.ready) == 'function' then local ok, ready = pcall(menu.ready); if not ok or ready ~= true then return end end
        state.attempts_bindings = state.attempts_bindings + 1
        local ok, accepted = pcall(menu.register_binding, CONFIG.BINDING_ID, 'Utility Weapon', nil, { category = CONFIG.MOD_NAME })
        state.registered_bindings = ok and accepted == true
        if not state.registered_bindings then note('binding registration refused') end
    end

    -- One step per game update. `now` is milliseconds from the injected clock.
    function self.step(now)
        if state.stopped then return end
        local ok, err = pcall(function()
            if not discover(now) then return end
            if not state.instance then state.instance = make_instance(now) end
            local options, bindings = env.get_global('ModOptionsMenu'), env.get_global('ModBindingsMenu')
            if options ~= state.options_menu or bindings ~= state.bindings_menu then
                if state.generation >= 4294967295 then state.stopped = true; return end
                state.generation = state.generation + 1
            end
            local present = options_present(options)
            if options ~= state.options_menu then state.options_menu = options; state.registered_options = false; state.subscribed = false; state.choice = nil; state.attempts_options = 0 end
            if bindings ~= state.bindings_menu then state.bindings_menu = bindings; state.registered_bindings = false; state.attempts_bindings = 0 end
            if now >= state.next_register then
                state.next_register = now + CONFIG.REGISTER_RETRY_MS
                poll_options(now, options); poll_bindings(now, bindings)
            end
            -- Evaluate the provider's semantic input state. true/false are real answers; nil means "input context unavailable" (USS treats it as blocked, not as released).
            local down, blocked, input_valid = 0, 0, 1
            local options_ready = present and provider_ready(options, false)
            local bindings_ready = provider_ready(bindings, true)
            -- Read the applied value each sample: callbacks are notifications, not a readiness lease.
            if options_ready and state.registered_options then
                local read_ok, value = pcall(options.get, CONFIG.OPTION_ID)
                state.choice = read_ok and valid(value) and value or nil
            else state.choice = nil end
            if state.registered_bindings and bindings_ready then
                local call_ok, value = pcall(state.bindings_menu.is_down, CONFIG.BINDING_ID)
                if not call_ok then input_valid = 0 elseif value == nil then blocked = 1 elseif value == true then down = 1 elseif value ~= false then input_valid = 0 end
            else input_valid = 0 end
            local mode = valid(state.choice) and state.choice or tonumber(state.mode_fn())
            local ready = (options_ready and bindings_ready and state.registered_options and state.subscribed and valid(state.choice) and state.registered_bindings and input_valid == 1) and 1 or 0
            if not valid(mode) then mode = 1; ready = 0 end
            local key = table.concat({ present and 1 or 0, state.generation, state.registered_options and 1 or 0, state.registered_bindings and 1 or 0, ready, mode, input_valid, down, blocked }, ',')
            if key ~= state.last_key or now - state.last_post_ms >= CONFIG.HEARTBEAT_MS then
                state.sequence = state.sequence + 1
                record.magic, record.protocol, record.size, record.flags = MAGIC, PROTOCOL, SIZE, present and 1 or 0
                record.instance, record.sequence = state.instance, state.sequence
                record.options_registered = present and state.registered_options and 1 or 0
                record.bindings_registered = state.registered_bindings and 1 or 0
                record.ready, record.mode, record.input_valid, record.utility_down, record.action_blocked, record.generation = ready, mode, input_valid, down, blocked, state.generation
                local result = state.post_fn(record, SIZE)
                if result ~= 0 then note('post refused by USS, code ' .. tostring(result)) end
                state.last_post_ms, state.last_key = now, key
            end
            state.errors = 0
        end)
        if not ok then
            state.errors = state.errors + 1; note('step error: ' .. tostring(err))
            if state.errors >= CONFIG.MAX_CONSECUTIVE_ERRORS then state.stopped = true; note('stopped after repeated errors; USS falls back to its own menu') end
        end
    end
    function self.state() return state end
    return self
end

-- Production environment: real ffi, real globals, real clock, loader log if present.
local function real_env()
    local kernel32 = ffi.load('kernel32')
    local loader = rawget(_G, 'CowboyBingusModLoader')
    local log_file
    if loader and type(loader.open_log) == 'function' then pcall(function() log_file = loader.open_log('USSMenuBridge.log') end) end
    return {
        get_module = function(name) local m = kernel32.USSMB_GetModuleHandleA(name); if m == nil then return nil end return m end,
        get_proc = function(module, name) local p = kernel32.USSMB_GetProcAddress(module, name); if p == nil then return nil end return p end,
        get_global = function(name) return rawget(_G, name) end,
        log = function(text) if log_file then log_file:write(text .. '\n'); log_file:flush() end end,
        now = function() return tonumber(kernel32.USSMB_GetTickCount64()) end,
    }
end

local hook = rawget(_G, 'USS_MENU_BRIDGE_TEST_HOOK')   -- tests inject a fake environment and drive step() themselves
if hook then return hook(new_bridge, CONFIG) end

local env = real_env()
local bridge = new_bridge(env)
local previous, shutdown = _G.update, _G.shutdown
local function pack_returns(...) return { n = select('#', ...), ... } end
_G.update = function(...)
    local result
    if previous then result = pack_returns(previous(...)) end
    bridge.step(env.now())
    if result then return unpack(result, 1, result.n) end
end
_G.shutdown = function(...)
    -- Withdraw at once rather than waiting for the lease to expire: one not-ready record, then chain.
    pcall(function()
        local state = bridge.state()
        if state.post_fn then
            state.sequence = state.sequence + 1
            local record = ffi.new('USSMB_post'); record.magic, record.protocol, record.size, record.flags = MAGIC, PROTOCOL, SIZE, 0
            record.instance, record.sequence, record.mode, record.generation = state.instance or 1, state.sequence, 1, state.generation
            state.post_fn(record, SIZE)
        end
    end)
    if shutdown then return shutdown(...) end
end
