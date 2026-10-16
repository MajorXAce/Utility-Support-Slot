-- Share only an observational inventory snapshot within one addon callback.
-- Never seed from the earlier core sample: selection/attachment may have changed
-- it. Never cache ammo, source policy or a native operation's verification read.
local function inventory_observation(sample)
    -- These locals belong to this factory instance. The returned functions
    -- are closures: they share the locals without exposing them through _G.
    -- `read` records an attempted sample, including a nil result. Retrying a
    -- failed read inside the same callback could mix different game states.
    local active, read, cached = false, false, nil
    return {
        begin = function()
            active = true;
            read = false;
            cached = nil
        end,
        sample = function()
            -- Outside the bracketed callback, always obtain a fresh snapshot.
            if not active then return sample() end
            if not read then
                cached = sample();
                read = true
            end
            return cached
        end,
        finish = function()
            -- Release the table reference; do not retain it across updates.
            active = false;
            read = false;
            cached = nil
        end
    }
end
