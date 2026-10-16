-- The optional options menu owns the applied mode; Arsenal remains the fallback.
-- Its choice is read after registration and cached through on_change callbacks.
-- Do not use set(): doing so would overwrite the player's saved menu choice.
local function binding_mode_options(emit)
 local id='utility_support_slot.binding_mode'
 local arsenal,identity,registered,subscribed,choice
 local next_check,attempts=0,0
 local last_mode,last_source
 local function valid(value)return value==1 or value==2 end
 local function poll(now)
  -- Wait until the first update so configuration-only addons can load in either
  -- order. The deployed Arsenal selection remains stable during this session.
  if arsenal==nil then arsenal=rawget(_G,'UtilitySupportSlotBindingMode')=='dedicated' end
  local menu=rawget(_G,'ModOptionsMenu')
  -- This identity check is cheap; all registration/get calls are throttled.
  -- Removing/replacing the dependency invalidates callbacks from the old menu.
  if menu~=identity then
   identity=menu;registered=false;subscribed=false;choice=nil
   next_check=0;attempts=0
  end
  if now>=next_check then
   next_check=now+1000
   local usable=type(menu)=='table' and menu.api==1
    and type(menu.register_option)=='function' and type(menu.get)=='function'
    and type(menu.on_change)=='function'
   if usable and attempts<60 then
    if not registered then
     attempts=attempts+1
     local ok,accepted,reason=pcall(menu.register_option,id,{
      type='choice',label='Binding Mode',mod='Utility Support Slot',
      mod_id='utility_support_slot',choices={'Default Bindings','Dedicated Key'},
      default=arsenal and 2 or 1,
      description='Default Bindings swaps weapons using your normal support controls. Dedicated Key draws the utility with 6 by default or your Utility Weapon binding. Press APPLY to change modes.'
     })
     registered=ok and accepted==true
     if not registered then
      emit('BINDING_OPTIONS_REGISTRATION_RETRY REASON='..tostring(ok and reason or accepted))
     end
    end
    if registered and not subscribed then
     local ok,accepted=pcall(menu.on_change,id,function(value)
      -- The menu calls this only for an applied change. Ignore stale callbacks
      -- after dependency replacement, and refuse unexpected choice values.
      if identity==menu and rawget(_G,'ModOptionsMenu')==menu then
       choice=valid(value) and value or nil
      end
     end)
     subscribed=ok and accepted~=false
     if not subscribed then attempts=attempts+1 end
    end
    if registered and subscribed and not valid(choice) then
     local ok,value=pcall(menu.get,id)
     if ok and valid(value) then choice=value
     else attempts=attempts+1 end
    end
   end
  end
  local from_menu=registered and subscribed and valid(choice)
  local dedicated=arsenal
  if from_menu then dedicated=choice==2 end
  local source=from_menu and 'MOD_OPTIONS_MENU' or 'ARSENAL'
  if dedicated~=last_mode or source~=last_source then
   emit('BINDING_MODE='..(dedicated and 'DEDICATED' or 'DEFAULT')..' SOURCE='..source)
   last_mode,last_source=dedicated,source
  end
  return dedicated
 end
 return poll
end
