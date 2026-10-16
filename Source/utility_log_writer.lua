-- One shared log writer with filesystem-failure isolation, no byte cap.
local function utility_log_writer(file)
 local failed=false
 return function(s)
  if not file or failed then return end
  local ok=pcall(function()
   assert(file:write(os.date('!%Y-%m-%dT%H:%M:%SZ ')..s..'\n'))
   file:flush()
  end)
  -- Do not retry a broken handle on every update. Verbosity filtering occurs
  -- before this writer; healthy logs continue regardless of their size.
  if not ok then failed=true end
 end
end
