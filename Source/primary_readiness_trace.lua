-- Bounded diagnostics for one existing primary wait. No memory reads, native calls or gameplay guards.
-- tick_ms comes from the guard's monotonic clock; milliseconds are local only.
local function primary_readiness_trace(start_ms,origin)
    local rows,conditions={},{}
    local names={'AVATAR','CHAR_SELECTED','INVENTORY_SELECTED','HELD_PRIMARY',
        'ROW1C','GLOBAL_GUARD','STATE_BYTE','TRANSITION'}
    local count,max_gap,previous_tick,first_ready,truncated=0,0,nil,nil,false
    local last_refused='NONE'
    local trace={}
    function trace.sample(g)
        local elapsed=g.tick_ms-start_ms
        local settled=g.phase==0 and g.timer_bits==0 and g.bit47==0
        local holstering=g.phase==1 and (g.prior==2 or g.prior==3)
            and g.pending==1 and g.bit47==1 and g.timer_bits>0 and g.timer_bits<0x7F800000
        -- Mirror the named conditions for explanation only. g.ready, produced
        -- by the original guard, remains the sole gameplay admission decision.
        local passed={g.row_avatar==g.avatar,g.selected==1,g.active==1,
            g.held==g.primary,g.row1c==0,g.global_guard==0,g.state_byte==0,
            settled or holstering}
        local blocked={}
        for i,name in ipairs(names)do
            local c=conditions[name] or {};conditions[name]=c
            if not passed[i]then
                blocked[#blocked+1]=name;c.last_blocked=elapsed;c.clear_after_last=nil
            elseif c.last_blocked and not c.clear_after_last then c.clear_after_last=elapsed end
        end
        local blockers=#blocked>0 and table.concat(blocked,',')or 'NONE'
        if not g.ready then last_refused=blockers
        elseif not first_ready then first_ready=elapsed end
        if previous_tick then max_gap=math.max(max_gap,g.tick_ms-previous_tick)end
        previous_tick=g.tick_ms;count=count+1
        local row={elapsed=elapsed,guard=g,blockers=blockers}
        -- 81 rows cover a nominal25ms/2000ms wait. Retain the latest sample if
        -- unexpected repeated sampling reaches the cap; never grow indefinitely.
        if #rows<81 then rows[#rows+1]=row else rows[81]=row;truncated=true end
    end
    function trace.finish(outcome)
        local result={'REPLACEMENT_PRIMARY_TIMING OUTCOME='..outcome..' SAMPLES='..count
            ..' ORIGIN='..(origin or 'UNSPECIFIED')
            ..' FIRST_SAMPLE_MS='..tostring(rows[1] and rows[1].elapsed)
            ..' FIRST_READY_MS='..tostring(first_ready)..' MAX_SAMPLE_GAP_MS='..max_gap
            ..' LAST_REFUSED_BLOCKERS='..last_refused..' TRUNCATED='..tostring(truncated)
            ..' NOMINAL_INTERVAL_MS=25 SAMPLE_LIMIT=81'}
        for _,row in ipairs(rows)do
            local g=row.guard
            local fields={'MS='..row.elapsed,'READY='..tostring(g.ready),'BLOCKERS='..row.blockers}
            for _,key in ipairs({'avatar','row_avatar','primary','active','held','selected','prior','phase','pending',
                'timer_bits','row1c','bit47','state_byte','global_guard','early_replacement'})do
                fields[#fields+1]=key:upper()..'='..tostring(g[key])
            end
            result[#result+1]='REPLACEMENT_PRIMARY_SAMPLE '..table.concat(fields,' ')
        end
        for _,name in ipairs(names)do
            local c=conditions[name]
            if c and c.last_blocked then
                result[#result+1]='REPLACEMENT_PRIMARY_CONDITION NAME='..name
                    ..' LAST_BLOCKED_MS='..c.last_blocked
                    ..' CLEAR_AFTER_LAST_MS='..tostring(c.clear_after_last)
            end
        end
        return result
    end
    return trace
end
