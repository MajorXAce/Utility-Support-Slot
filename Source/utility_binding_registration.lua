-- Discover/register the optional action in either behavior mode. Registration
-- exposes an unbound menu row; it never polls the key or draws a weapon.
local function utility_binding_registration(emit)
 local id='utility_support_slot.draw_utility'
 -- identity is the optional menu TABLE, not a weapon identity. Replacing it
 -- resets registration so loader/menu reloads can recover. nil in the register
 -- call leaves the menu key unbound; the INI supplies the separate fallback.
 -- Retry at most once per second, up to 60 attempts for each menu instance.
 local identity,registered,next_try,tries=nil,false,0,0
 return {poll=function(now)
  local menu=rawget(_G,'ModBindingsMenu')
  if menu~=identity then identity=menu;registered=false;next_try=0;tries=0 end
  if type(menu)~='table' or menu.api~=1 or type(menu.version)~='number' or menu.version<2
   or type(menu.register_binding)~='function' or type(menu.is_down)~='function'then return end
  if not registered and now>=next_try and tries<60 then
   next_try=now+1000;tries=tries+1
   local ok,value=pcall(menu.register_binding,id,'Utility Weapon',nil,{category='Utility Support Slot'})
   if ok and value==true then registered=true;emit('UTILITY_BINDING_REGISTERED MENU=true')end
  end
  if registered then return menu end
 end}
end
