-- Read the live preference used by the native pickup consumer. This factory
-- performs no writes and installs no update hook. The caller already checks the
-- required build dependencies; validate these specific instructions once as well.
local function pickup_preferences(api, game)
    local signatures = {
        -- Writer associates the key string with preference-object +F7.
        {0x102FB73, '488d1566f92201440fb683f7000000488bce4c8b'},
        -- Loader uses object +BC, then loads the key into subobject +3B.
        {0x1030260, '48895c2408574883ec20488d99bc0000004c8bd2488bcb'},
        {0x1030631, '488d15a8ee220188433a488b05c65c2f02488b8828010000488b8190000000488bcfffd0440fb6433f488d1597ee220188433b'},
        -- Pickup code reaches the preference pointer through manager +14028.
        {0x97CA43, '488b4580488b802840010080b8f700000000750c4585e47407443b6424507507c644'},
        -- Account-settings load caller uses global owner +AC3DC.
        {0x13779A7, '488b0d92e9fa01458bc14881c1dcc30a00498bd2e8a088cbff4c8b0579'},
    }
 local function unhex(s)
  return (s:gsub('..',function(x)return string.char(tonumber(x,16))end))
 end
 local permitted=true
 for _,signature in ipairs(signatures)do
  local expected=unhex(signature[2])
  if api.read(game+signature[1],#expected)~=expected then permitted=false;break end
 end
 local function ptr(address)return api.pointer(api.read(address,8))end
 return function()
  if not permitted then return nil,'CODE_SIGNATURE_MISMATCH' end
  -- Use the game-selected account, never a guessed Steam save directory.
  -- Both confirmed paths must identify the same preference object.
  local owner=ptr(game+0x3326340)
  local manager=ptr(game+0x3326D68)
  if not owner or not manager then return nil,'NO_SETTINGS_CONTEXT' end
  local active=ptr(manager+0x14028)
  if active~=owner+0xAC3DC then return nil,'PREFERENCE_OBJECT_MISMATCH' end
  local bytes=api.read(active+0xF7,1)
  if not bytes or #bytes~=1 then return nil,'UNREADABLE_PICKUP_SETTING' end
  local value=bytes:byte(1)
  if value~=0 and value~=1 then return nil,'NON_BOOLEAN_PICKUP_SETTING' end
  -- Refuse a value sampled across a settings/account/scene transition. False
  -- is a valid No setting; only nil means that the decision must be deferred.
  if api.read(active+0xF7,1)~=bytes or ptr(game+0x3326340)~=owner
   or ptr(game+0x3326D68)~=manager or ptr(manager+0x14028)~=active then
   return nil,'SETTINGS_CONTEXT_CHANGED'
  end
  return value==1
 end
end
