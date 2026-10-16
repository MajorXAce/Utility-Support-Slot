-- All components share one bounded writer. Detailed samplers must also obey
-- configuration.log_level, so quiet logging avoids the work, not just the text.
local function utility_logging(write,level)
 local rank={error=1,info=2,debug=3}
 local threshold=rank[level] or 1
 local function inferred_level(message)
  if message:find('ERROR',1,true) or message:find('FAILED',1,true)
   or message:find('PAUSED',1,true) or message:find('UNSUPPORTED',1,true)
   or message:find('UNAVAILABLE',1,true) or message:find('INVALID',1,true)
   or message:find('REJECTED',1,true) or message:find('RATE_LIMIT',1,true)
   or message:find('TIMEOUT',1,true) or message:find('DISABLED',1,true) then return 'error' end
  return 'info'
 end
 return function(message,severity,component)
  severity=severity or inferred_level(message)
  if (rank[severity] or 2)>threshold then return end
  if not component then
   if message:match('^HUD_') or message:match('^PICKUP_HUD_') or message:match('^PICKUP_PRESENTATION_')then component='HUD'
   elseif message:match('^RESUPPLY_') or message:match('^SELF_SUPPLY_') or message:match('^PICKUP_FILTER_')then component='RESUPPLY'
   elseif message:match('^INI_') or message:match('^BINDING_')then component='CONFIG'
   else component='CORE' end
  end
  write('['..component..']['..severity:upper()..'] '..message)
 end
end
