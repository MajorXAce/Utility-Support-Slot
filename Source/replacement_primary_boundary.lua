-- One native replacement owns this earlier character boundary. No memory writes
-- or gameplay native calls: Core validates the work and each full publication entry.
local function replacement_primary_boundary(services,enabled)
    local work
    local allowed={REPLACEMENT_WAIT=true,REPLACEMENT_CONTROL=true,
        REPLACEMENT_RETAINED_UTILITY=true,POST_ATTACH_INCOMING=true,
        ENTRY_FORMATION=true,ENTRY_INCOMING=true}
    local boundary={}
    function boundary.begin(w)
        if enabled==false then return end -- Archived private comparison baseline.
        assert(not work,'REPLACEMENT_BOUNDARY_ALREADY_ACTIVE')
        assert(w and w.pair==services.current_pair() and w.context
            and w.primary_deadline and w.old_id and w.new_id and w.kept_id
            and w.old~=w.new and w.old~=w.kept and w.new~=w.kept
            and services.weapon_identity(w.old)==w.old_id
            and services.weapon_identity(w.new)==w.new_id
            and services.weapon_identity(w.kept)==w.kept_id
            and services.is_utility(w.old_id)==services.is_utility(w.new_id)
            and services.is_utility(w.kept_id)~=services.is_utility(w.new_id),
            'REPLACEMENT_BOUNDARY_SCOPE')
        work=w
    end
    function boundary.admit(owner,g,stage,epoch)
        local w=work
        if not w or not allowed[stage] or epoch~=w.epoch
            or services.clock()>=w.primary_deadline
            or services.current_pair()~=w.pair then return false end
        local p=w.context
        if owner.owner~=p.owner or owner.avatar~=p.avatar or owner.inventory~=p.inventory
            or owner.address~=p.address or owner.primary~=p.primary then return false end
        -- Native countdown continues. Only the observed primary-held phase2
        -- shape is added; all identity/parent/peer/code entry checks still run.
        return owner.active==1 and g.avatar==owner.avatar and g.row_avatar==owner.avatar
            and g.primary==owner.primary and g.held==owner.primary
            and g.selected==1 and g.active==1 and g.prior==3 and g.pending==1
            and g.phase==2 and g.timer_bits==0 and g.row1c>0 and g.row1c<0x7F800000
            and g.bit47==1 and g.state_byte==0 and g.global_guard==0
    end
    function boundary.close()work=nil end
    function boundary.status()return {active=work~=nil}end
    return boundary
end
