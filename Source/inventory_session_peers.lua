-- Session peer IDs are opaque eight-byte values, not entity handles.
-- Read the full normal squad roster once. Keys ignore array order but retain
-- local/host identity, so a live transaction cannot silently change authority.
local function inventory_session_peers(read,uint,session)
    local function field(at,size)
        local bytes=read(at,size)
        if not bytes or #bytes~=size then error('SESSION_PEER_READ',0)end
        return bytes
    end
    local count=uint(field(session+0x162D8,4),0)
    if count<1 or count>4 then error('SESSION_PEER_COUNT',0)end
    local self=field(session+0xB398,8)
    local host=field(session+0xB3A8,8)
    local raw=field(session+0x162E0,count*8)
    local zero=string.rep(string.char(0),8)
    if self==zero then error('SELF_PEER_MISSING',0)end
    local members,seen,remotes={},{},{}
    for offset=1,#raw,8 do
        local id=raw:sub(offset,offset+7)
        if id==zero or seen[id]then error('SESSION_PEER_IDENTITY',0)end
        members[#members+1]=id;seen[id]=true
        if id~=self then remotes[#remotes+1]=id end
    end
    if not seen[self]then error('SESSION_SELF_MISSING',0)end
    if host~=zero and not seen[host]then error('SESSION_HOST_MISSING',0)end
    table.sort(members)
    local key=table.concat(members)
    return {count=count,self=self,host=host,remotes=remotes,key=key,
        context_key=self..host..key,is_host=self==host or host==zero}
end
