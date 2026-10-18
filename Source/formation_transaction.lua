-- Pair setup and multiplayer draws wait across updates for primary readiness.
-- Keep the original operation order and publish the actual incoming member before
-- drawing. A refusal before entry can wait; an uncertain/failed native entry cannot.
local function queue_formation(p,order,selected,pickup_setting,draw_target,draw_id)
 if not publication then return false end
 local utility_exit=order=='UTILITY_EXIT'
 local retained=order=='RETAINED_MEMBER'
 local replacement=order=='REPLACEMENT'
 local managed_draw=order=='PAIR_DRAW' or retained
 local state=publication.status()
 if state.mission and state.eligible and (state.disabled or state.suspended)then
  paused=true;emit('PAUSED PAIR_PUBLICATION_UNAVAILABLE');return true
 end
 if utility_exit or managed_draw then
  -- This is an existing pair whose local support membership would otherwise
  -- change without an incoming publication. Unsupported scopes retain Core's
  -- original local restoration; do not invent a new network contract here.
  if not state.mission or not (state.host or state.joined_client) or not state.eligible then return false end
  if not state.formed or state.disabled or state.suspended then
   paused=true;emit('PAUSED PAIR_PUBLICATION_UNAVAILABLE');return true
  end
  if managed_draw then
   if (not retained and selected~=3) or (selected~=1 and selected~=2 and selected~=3) then return false end
   local expected=draw_target==pair.utility and utility_id
    or draw_target==pair.support and pair.support_id
   if not expected or draw_id~=expected then return false end
  end
 elseif not publication.pending_formation(p)then return false end
 local utility_first=order=='UTILITY_FIRST'
 local steps=managed_draw and {
  {target=draw_target,id=draw_id,label=draw_target==pair.utility and 'DRAW_UTILITY' or 'DRAW_SUPPORT',stage='POST_ATTACH_INCOMING'},
 } or utility_exit and {
  {target=pair.support,id=pair.support_id,label='STORE_UTILITY_ON_WEAPON_CHANGE',stage='POST_ATTACH_INCOMING'},
 } or utility_first and {
  {target=pair.utility,id=utility_id,label='CAPTURE_UTILITY_FIRST',stage='UTILITY_FIRST_CAPTURE'},
  {target=pair.support,id=pair.support_id,label='RESTORE_UTILITY_FIRST_SUPPORT',stage='POST_ATTACH_INCOMING'},
 } or {{target=pair.support,id=pair.support_id,label='STORE_NEW_UTILITY',stage='POST_ATTACH_INCOMING'}}
 if replacement then
  -- Publish the new lifetime before restoring its requested member/selection.
  -- A current-slot step sends only: it must not repeat native attachment.
  steps={{target=pair.support,id=pair.support_id,label='STORE_REPLACEMENT_UTILITY',
   stage='POST_ATTACH_INCOMING',current=p.support==pair.support}}
  if draw_target==pair.utility then
   steps[#steps+1]={target=pair.utility,id=utility_id,label='REPLACEMENT_RESTORE',stage='POST_ATTACH_INCOMING'}
  end
 end
 if not managed_draw and not replacement and not utility_exit and not utility_first and pickup_setting then
  steps[#steps+1]={target=pair.utility,id=utility_id,label='DRAW_UTILITY',stage='POST_ATTACH_INCOMING'}
 end
 formation_pending={pair=pair,context=p,epoch=state.epoch,host_role=state.host,utility_id=utility_id,
  ordinary_id=pair.support_id,support=p.support,steps=steps,index=1,selected=selected,origin_selected=p.active,
  deadline=tonumber(kernel.uss_win1_GetTickCount64())+2000,order=order,pickup=pickup_setting,utility_exit=utility_exit,managed_draw=managed_draw,replacement=replacement}
 -- The pair remains owned by this work record until completion or cancellation.
 pending_utility=nil
 emit('FORMATION_DEFERRED_BEGIN ORDER='..order..' SELECTED='..selected)
 return true
end
-- Drop/consumption has already restored the survivor locally. Hold only this
-- finite operation across updates; the actual Core pair is cleared immediately.
local function queue_lone_publication(p,target,id,outgoing,outgoing_id)
 if not publication then return end
 local recovery=publication.begin_lone(p,target,id,outgoing,outgoing_id)
 if not recovery then
  local state=publication.status()
  if state.mission and state.eligible then paused=true;emit('PAUSED LONE_PUBLICATION_UNAVAILABLE')end
  return -- Solo keeps its original local behavior.
 end
 formation_pending={lone=recovery,context=p,selected=p.active,
  deadline=tonumber(kernel.uss_win1_GetTickCount64())+2000}
 emit('LONE_PUBLICATION_BEGIN TARGET='..target..' OUTGOING='..outgoing)
end
local function service_lone_publication(p,now,input,work)
 local ok,why=pcall(function()
  local recovery=work.lone
  if now>=work.deadline or input or paused or down or pair
   or not publication.replacement_scope_ready(recovery.epoch)
   or not same_context(work.context,p) or not can_switch(p)
   or p.primary~=work.context.primary or p.secondary~=work.context.secondary
   or p.support~=recovery.target or identity(recovery.target)~=recovery.id then
   error('LONE_CONTEXT_OR_INPUT_CHANGED',0)
  end
  local survivor=attached_entity(recovery.target,recovery.id)
  if not survivor or survivor.parent~=p.avatar then error('LONE_SURVIVOR_PARENT_CHANGED',0)end
  if not work.primary_requested then
   if p.active~=work.selected then error('LONE_SELECTION_CHANGED',0)end
   if p.active~=1 and not choose(p,1)then error('LONE_PRIMARY_SELECTION_FAILED',0)end
   work.primary_requested=true;p=sample()
  end
  if not p or p.active~=1 then error('LONE_SELECTION_CHANGED',0)end
  local ready,reason=publication.lone_primary_ready(p,recovery)
  if not ready then
   if reason=='PRIMARY_BOUNDARY_NOT_READY' then return end
   error(reason,0)
  end
  local published,reason=publication.lone_boundary(p,recovery)
  if not published then error(reason,0)end
  -- Selection is restored only after a returned send and a fresh local check.
  local after=sample()
  if not after or not same_context(work.context,after) or after.support~=recovery.target
   or after.active~=1 or after.primary~=work.context.primary
   or after.secondary~=work.context.secondary or not can_switch(after) then error('LONE_FINAL_CONTEXT_CHANGED',0)end
  if work.selected~=1 and not choose(after,work.selected)then error('LONE_FINAL_SELECTION_FAILED',0)end
  formation_pending=nil
  emit('LONE_PUBLICATION_COMPLETE TARGET='..recovery.target..' RESTORED_SELECTED='..work.selected)
 end)
 if not ok then
  formation_pending=nil;paused=true
  emit('LONE_PUBLICATION_CANCEL REASON='..tostring(why)..' RETRY=false')
 end
 return true
end
local function service_formation(p,now,input)
 local work=formation_pending
 if not work then return false end
 if work.lone then return service_lone_publication(p,now,input,work)end
 local function cancel(reason)
  formation_pending=nil
  if pair==work.pair then paused=true end
  emit('FORMATION_DEFERRED_CANCEL REASON='..tostring(reason))
 end
 local ok,why=pcall(function()
  local state=publication.status()
  if work.utility_exit and input then error('INPUT_DURING_UTILITY_EXIT',0)end
  if (work.managed_draw or work.replacement) and input then error('INPUT_DURING_PAIR_DRAW',0)end
  if now>=work.deadline then error('TIMEOUT',0)end
  if not state.mission or not (state.host or state.joined_client) or state.host~=work.host_role or state.epoch~=work.epoch
   or state.disabled or state.suspended or ((work.managed_draw or work.replacement) and not state.eligible)
   or paused or down or pair~=work.pair
   or not same_context(work.context,p) or not can_switch(p)
   or utility_id~=work.utility_id or pair.support_id~=work.ordinary_id
   or identity(pair.utility)~=work.utility_id or identity(pair.support)~=work.ordinary_id
   or p.support~=work.support then error('CONTEXT_OR_PICKUP_CHANGED',0)end
  if (work.utility_exit or work.managed_draw or work.replacement) and (p.primary~=work.context.primary or p.secondary~=work.context.secondary
   or(not work.primary_requested and p.active~=(work.origin_selected or work.selected)))then
   error('UTILITY_EXIT_SELECTION_CHANGED',0)
  end
  for _,member in ipairs({{pair.utility,work.utility_id},{pair.support,work.ordinary_id}})do
   local item=attached_entity(member[1],member[2])
   if not item or (item.parent~=0 and item.parent~=p.avatar)
    or((work.utility_exit or work.managed_draw) and item.parent~=p.avatar)then error('PARENT_CHANGED',0)end
  end
  if not work.primary_requested then
   -- Request once. Never force primary again after intervening player input.
   if p.active~=1 and not choose(p,1)then error('PRIMARY_SELECTION_FAILED',0)end
   work.primary_requested=true
   p=sample()
  end
  if not p or p.active~=1 then error('SELECTION_CHANGED',0)end
  local ready,reason=publication.formation_primary_ready(p)
  if not ready then
   if reason=='PRIMARY_BOUNDARY_NOT_READY' then return end
   error(reason,0)
  end
  local step=work.steps[work.index]
  if not work.attached then
   -- Consume attachment before entry; failures cancel, never retry it.
   work.attached=true
   work.outgoing=p.support
   if step.current then
    if p.support~=step.target then error('CURRENT_MEMBER_CHANGED',0)end
   elseif not switch_entity(p,step.target,step.id,false,step.label,work.managed_draw or work.replacement)then
    error('ATTACH_FAILED',0)
   end
   work.support=step.target
  end
  local after=sample()
  if not after or not same_context(work.context,after) or after.support~=step.target
   or after.active~=1 then error('ATTACH_CONTEXT_CHANGED',0)end
  -- switch_entity may already have published. boundary deduplicates a returned
  -- member and refuses uncertain entry; it only waits on a pre-entry guard refusal.
  local published,reason=publication.boundary(after,false,step.stage)
  if not published then
   if reason=='PRIMARY_BOUNDARY_NOT_READY' then return end
   error(reason,0)
  end
  work.index=work.index+1;work.attached=false
  if work.index<=#work.steps then return end
  if not check_pair(after)then error('FINAL_PAIR_CHANGED',0)end
  if after.active~=work.selected and not choose(after,work.selected)then error('FINAL_SELECTION_FAILED',0)end
  pair.real=step.target;pair.selected=work.selected
  pair.utility_drawn=step.target==pair.utility and work.selected==3
  last_support={entity=pair.support,id=pair.support_id};pending_utility=nil
  formation_pending=nil
  if work.managed_draw then
   -- Acknowledge only after final selection; never report primary as the draw.
   local acknowledge=_G.UtilitySupportSlotContext.acknowledge_inventory_change
   if acknowledge and work.selected==3 then acknowledge(work.outgoing,step.target,step.label)end
  end
  if work.replacement_result then
   local result=work.replacement_result
   trace_replacement('COMPLETE',nil,result.incoming,result.old,result.kept)
   emit('REPLACE_TYPED DROPPED='..result.old..' RETAINED='..result.kept..' NEW_REAL='..step.target)
  end
  if work.managed_draw then
   emit('PAIR_DRAW_COMPLETE TARGET='..step.target..' ACTIVE='..work.selected)
   return
  end
  if work.utility_exit then
   emit('UTILITY_EXIT_COMPLETE SUPPORT='..pair.support..' RESTORED_SELECTED='..work.selected)
   return
  end
  if work.replacement then emit('REPLACEMENT_PUBLICATION_COMPLETE ACTIVE='..work.selected);return end
  emit('PAIR_READY ORDER='..work.order..' UTILITY='..pair.utility..' SUPPORT='..pair.support
   ..' SWITCH_WEAPON_ON_PICKUP='..tostring(work.pickup))
 end)
 if not ok then cancel(why)end
 return true
end
