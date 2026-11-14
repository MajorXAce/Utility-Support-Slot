/* Typed forwarding only. No address resolver, state machine or game targets.
 * Callers own current context/pins/thread/attempt guards and post-call readback. */
#ifndef USS_NATIVE_CALLS_H
#define USS_NATIVE_CALLS_H
#include <stdint.h>
#include <stddef.h>
_Static_assert(sizeof(void*) == 8, "USS native interface requires x64");

typedef void (*UssAttachFn)(void*,uint32_t,uint32_t,uint32_t);
typedef void (*UssSelectStateFn)(void*,uint32_t,uint32_t);
typedef void (*UssSelectWeaponFn)(void*,uint32_t,uint32_t);
typedef void (*UssDropEntityFn)(void*,uint32_t,uint32_t,uint8_t,void*,uint8_t);
typedef void (*UssDropSlotFn)(void*,uint32_t,uint32_t,uint32_t);
typedef void (*UssSendInventoryFn)(int64_t,uint32_t,uint32_t,uint32_t,uint32_t,
                                    uint32_t,uint32_t,uint64_t);
typedef void (*UssZoneCloseFn)(void*,uint32_t,uint32_t,uint8_t); /* events manager, entity, zone, open(0 = close) */
typedef void (*UssFilterCommandFn)(uint32_t*,uint32_t,uint32_t); /* physics actors, count, filter catalog hash */
typedef struct UssNativeCalls {
    UssAttachFn attach;
    UssSelectStateFn select_state;
    UssSelectWeaponFn select_weapon;
    UssDropEntityFn drop_entity;
    UssDropSlotFn drop_slot;
    UssSendInventoryFn send_inventory;
    UssZoneCloseFn zone_close;
} UssNativeCalls;

typedef enum UssCallResult {
    USS_CALL_NOT_ENTERED = 0,
    USS_CALL_RETURNED /* No claim about applied state or network delivery. */
} UssCallResult;

typedef struct UssInventoryMessage {
    uint32_t avatar, primary, secondary, support, backpack, row_field24;
    uint64_t metadata; /* Exact copied row+40 bytes, never converted through double. */
} UssInventoryMessage;

static inline UssCallResult uss_attach_support(const UssNativeCalls* calls,void* inventory,
                                       uint32_t avatar,uint32_t entity)
{
    if (!calls || !calls->attach || !inventory) return USS_CALL_NOT_ENTERED;
    calls->attach(inventory,avatar,entity,3);
    return USS_CALL_RETURNED;
}
static inline UssCallResult uss_select_slot(const UssNativeCalls* calls,void* action_state,
                                    void* inventory,uint32_t avatar,uint32_t slot)
{
    /* Check both entries before either call; do not leave a known missing half. */
    if (!calls || !calls->select_state || !calls->select_weapon ||
        !action_state || !inventory) return USS_CALL_NOT_ENTERED;
    calls->select_state(action_state,slot,0);
    calls->select_weapon(inventory,avatar,slot);
    return USS_CALL_RETURNED;
}
static inline UssCallResult uss_drop_stored_entity(const UssNativeCalls* calls,void* inventory,
                                           uint32_t avatar,uint32_t entity)
{
    if (!calls || !calls->drop_entity || !inventory) return USS_CALL_NOT_ENTERED;
    calls->drop_entity(inventory,avatar,entity,(uint8_t)0,NULL,(uint8_t)1);
    return USS_CALL_RETURNED;
}
static inline UssCallResult uss_drop_support_slot(const UssNativeCalls* calls,void* inventory,
                                          uint32_t avatar)
{
    if (!calls || !calls->drop_slot || !inventory) return USS_CALL_NOT_ENTERED;
    calls->drop_slot(inventory,avatar,3,1);
    return USS_CALL_RETURNED;
}
static inline UssCallResult uss_send_inventory(const UssNativeCalls* calls,
                                       const UssInventoryMessage* message)
{
    if (!calls || !calls->send_inventory || !message) return USS_CALL_NOT_ENTERED;
    calls->send_inventory(INT64_C(-2),message->avatar,message->primary,message->secondary,
        message->support,message->backpack,message->row_field24,message->metadata);
    return USS_CALL_RETURNED;
}
static inline UssCallResult uss_close_pickup_zone(const UssNativeCalls* calls,void* events,uint32_t entity,uint32_t zone)
{
    if (!calls || !calls->zone_close || !events || !entity) return USS_CALL_NOT_ENTERED;
    calls->zone_close(events,entity,zone,(uint8_t)0);
    return USS_CALL_RETURNED;
}
#endif
