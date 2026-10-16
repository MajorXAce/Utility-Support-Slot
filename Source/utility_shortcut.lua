-- The applied behavior mode controls shortcut input. The optional unbound
-- menu action registers in either mode; default mode never reads its key. Dedicated mode remains dedicated even when
-- the optional menu is missing: its INI keyboard key is always the fallback.
-- Mod Bindings Menu starts unbound and exposes action state, not mapping count.
-- Like Helmet Headlamp, the first accepted menu edge selects that input source
-- for this session. Removing/unavailable menu temporarily restores the INI key.
local function utility_shortcut(loader,ffi,kernel,emit,enabled,api,game,configuration,registration)
 configuration=configuration or utility_configuration()
 registration=registration or utility_binding_registration(emit)
 -- Cache one dedicated reader for the session, but evaluate the cached mode
 -- on each update. Re-entering dedicated mode must not recreate the INI, FFI
 -- declarations or menu binding. A held key needs a fresh release baseline.
 if type(enabled)=='function'then
  local instance,current
  return {
   poll=function(now,eligible)
    registration.poll(now) -- Discover the row even before Dedicated Key is selected.
    local selected=enabled(now)==true
    local changed=current~=nil and selected~=current
    current=selected
    if changed and instance then instance.reset()end
    if not selected then return false,changed end
    if not instance then instance=utility_shortcut(loader,ffi,kernel,emit,true,api,game,configuration,registration)end
    return instance.poll(now,eligible and not changed),changed
   end,
   configured=function()return current==true end
  }
 end
 if not enabled then return {poll=function(now)registration.poll(now);return false end,configured=function()return false end}end
 ffi.cdef[[short uss_win1_GetAsyncKeyState(int) __asm__("GetAsyncKeyState"); void *uss_win1_GetForegroundWindow(void) __asm__("GetForegroundWindow");
 uint32_t uss_win1_GetWindowThreadProcessId(void *, uint32_t *) __asm__("GetWindowThreadProcessId"); uint32_t uss_win1_GetCurrentProcessId(void) __asm__("GetCurrentProcessId");]]
 local user=ffi.load('user32')
 local pid=ffi.new('uint32_t[1]')
 local id='utility_support_slot.draw_utility'
 local key=configuration.utility_key -- Shared startup config; defaults to keyboard 6.
 local menu_identity
 local menu_last,ini_last=nil,nil
 local taken=false
 local last_ini,last_menu=-math.huge,-math.huge
 local function reset_edges()menu_last=nil;ini_last=nil end
 local function menu(now)
  local current=registration.poll(now)
  if current~=menu_identity then menu_identity=current;menu_last=nil end
  return current
 end
 local function focused()
  local window=user.uss_win1_GetForegroundWindow()
  if window==nil then return false end
  user.uss_win1_GetWindowThreadProcessId(window,pid)
  return tonumber(pid[0])==tonumber(kernel.uss_win1_GetCurrentProcessId())
 end
 local function uint(bytes,offset)
  if not bytes then return end
  local a,b,c,d=bytes:byte(offset+1,offset+4)
  return d and a+b*256+c*65536+d*16777216 or nil
 end
 -- Copied read-only layout evidence from the supplied Headlamp example, with
 -- this core's startup dependency checks. Check only on an input edge.
 -- Refuse shortcuts if UI state cannot be verified: no swaps while rebinding
 -- in the escape menu or typing chat, and no unguarded fallback after a change.
 local function ui_clear()
  local ui=api.pointer(api.read(game+0x347CE28,8))
  if not ui then return false end
  local chat=api.read(ui+0x10,8)
  local stack=api.read(ui+0x429C,24)
  if not chat or #chat~=8 or not stack or #stack~=24 then return false end
  if chat~=string.rep('\0',8)then return false end
  local depth,bottom=uint(stack,20),uint(stack,0)
  return not(depth>=1 and depth<=5 and bottom==1)
 end
 local function poll(now,eligible)
  local m=menu(now)
  if not eligible then reset_edges();return false end
  -- Read native activation state through the dependency's public API; Press,
  -- Hold and controller bindings are evaluated by the game, not reproduced here.
  local live,menu_down=false,nil
  if m then
   local ok,value=pcall(m.is_down,id)
   if ok and type(value)=='boolean'then live=true;menu_down=value end
  end
  local menu_edge=live and menu_down and menu_last==false
  menu_last=live and menu_down or nil
  -- False must remain false, not nil: release is our required edge baseline.
  if live and not menu_down then menu_last=false end
  local ini_edge=false
  if not taken or not live then
   local held=user.uss_win1_GetAsyncKeyState(key)<0
   ini_edge=held and ini_last==false;ini_last=held
  else ini_last=nil end
  if not live then menu_last=nil end
  if not menu_edge and not ini_edge then return false end
  if not focused() or not ui_clear()then reset_edges();return false end
  -- When both inputs name the same physical key, they may report one frame
  -- apart. Merge across sources for 150 ms, while preserving rapid separate
  -- presses from the same source. INI remains baseline-read when unavailable.
  if menu_edge then
   taken=true
   if now-last_ini>150 then last_menu=now;return true end
   return false
  end
  if ini_edge and now-last_menu>150 then last_ini=now;return true end
  return false
 end
 emit('UTILITY_INPUT_MODE=DEDICATED INI_VK='..key..' MENU_OPTIONAL=true')
 return {poll=poll,configured=function()return true end,
  reset=function()reset_edges();last_ini=-math.huge;last_menu=-math.huge end}
end
