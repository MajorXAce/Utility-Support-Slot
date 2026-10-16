-- One shared INI read at startup, regardless of the selected input mode.
-- Preserve existing keys/comments; append only settings which are missing.
local function utility_configuration()
 -- 0x36 is the Windows virtual-key code for keyboard 6, not game action 6.
 -- This INI fallback is separate from the optional native binding menu action.
 -- Invalid/missing values retain these defaults and produce startup warnings.
 local result={utility_key=0x36,log_level='error',warnings={}}
 local levels={error=true,info=true,debug=true}
 local function warning(s)result.warnings[#result.warnings+1]=s end
 local function parse_key(value)
  value=value:lower():gsub('%s',''):gsub('_',''):gsub('-','')
  if #value==1 and value:match('[a-z0-9]')then return value:upper():byte()end
  local number=value:match('^f(%d+)$')
  if number and tonumber(number)>=1 and tonumber(number)<=12 then return 111+tonumber(number)end
  number=value:match('^numpad(%d)$')
  if number then return 96+tonumber(number)end
  return ({pageup=0x21,pagedown=0x22,home=0x24,['end']=0x23,insert=0x2D,delete=0x2E})[value]
 end
 local directory=os.getenv('APPDATA')
 if not directory then warning('INI_UNAVAILABLE APPDATA_MISSING');return result end
 result.path=directory..'/Arrowhead/Helldivers2/utility_support_slot.ini'
 local ok,why=pcall(function()
  local file=io.open(result.path,'rb')
  local data
  if file then data=file:read(65537)or'';file:close()end
  if data and #data>65536 then error('INI_TOO_LARGE')end
  local seen={}
  for line in (data or''):gmatch('[^\r\n]+')do
   local name,value=line:match('^%s*([%w_]+)%s*=%s*([^;#]*)')
   if name and (name=='utility_key' or name=='log_level') and not seen[name]then
    seen[name]=true
    value=value:match('^%s*(.-)%s*$')
    if name=='utility_key' then
     local key=parse_key(value)
     if key then result.utility_key=key else warning('INI_INVALID_KEY USING=6')end
    elseif levels[value:lower()]then result.log_level=value:lower()
    else warning('INI_INVALID_LOG_LEVEL USING=error')end
   end
  end
  local missing={}
  if not seen.utility_key then missing[#missing+1]='utility_key=6' end
  if not seen.log_level then missing[#missing+1]='log_level=error' end
  if #missing>0 then
   local out=assert(io.open(result.path,data and'ab' or'wb'))
   local prefix=data and (#data>0 and '\n' or '') or
    '; Utility Support Slot\n; Restart after editing. log_level: error, info, debug\n; utility_key: a-z, 0-9, f1-f12, numpad0-9, page up/down, home/end, insert/delete\n'
   local written=out:write(prefix..table.concat(missing,'\n')..'\n');out:close()
   assert(written,'INI_WRITE_FAILED')
  end
 end)
 if not ok then warning('INI_UNAVAILABLE REASON='..tostring(why))end
 return result
end
