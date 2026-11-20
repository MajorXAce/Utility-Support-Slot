/* Menu bridge harness: the REAL mailbox, arbiter, provider-neutral handover policy and diagnostics composed exactly as the call site composes them.
 * Authored fakes: the clock, the C provider's sample stream, the native tab-bar reads. Not covered here: real LuaJIT execution or real provider menus, the native chrome (Tests/menu_compat_087_mom_step_test.c covers the standalone/MOM tab rule). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned long long receiver_now;
#define USS_LUA_RECEIPT_CLOCK() receiver_now
#include "../src/uss_menu_external.h"
#include "../src/uss_menu_diagnostics.h"

static UssMenuHandover handover; static UssMenuArbiter arbiter; static UssLuaMenuMailbox box; static UssSettings standalone;
static UssMenuChoice last_choice; static uint64_t now;
static void reset(void) { uss_menu_handover_init(&handover); memset(&arbiter, 0, sizeof arbiter); uss_lua_mailbox_init(&box); standalone = uss_settings_defaults(); now = 0; receiver_now = 0; memset(&last_choice, 0, sizeof last_choice); }
static UssLuaMenuPost good(uint64_t instance, uint64_t sequence, unsigned mode, unsigned down)
{
    UssLuaMenuPost p; memset(&p, 0, sizeof p);
    p.magic = USS_LUA_MENU_MAGIC; p.protocol = USS_LUA_MENU_PROTOCOL; p.size = sizeof p; p.instance = instance; p.sequence = sequence;
    p.flags = USS_LUA_MENU_OPTIONS_PRESENT; p.generation = 1;
    p.options_registered = p.bindings_registered = p.ready = p.input_valid = 1; p.mode = mode; p.utility_down = down; return p;
}
static unsigned post(const UssLuaMenuPost* p) { return uss_lua_mailbox_post(&box, p, sizeof *p); }
static UssMenuProviderSample c_sample(uint64_t provider, uint64_t sequence, unsigned mode, unsigned down)
{
    UssMenuProviderSample s = { USS_MENU_HANDOVER_PROTOCOL, sizeof s, provider, sequence, 1, 1, 1, mode, 1, down, 0 }; return s;
}
static UssMenuProviderSample none_c(void) { UssMenuProviderSample z; memset(&z, 0, sizeof z); return z; }   /* what the call site passes when the C provider is unavailable */
static UssMenuHandoverView frame(uint64_t dt, const UssMenuProviderSample* c, unsigned transaction, unsigned blocked, unsigned local_down)
{
    now += dt; receiver_now = now; return uss_menu_external_step(&handover, &arbiter, &box, c, now, transaction, blocked, &standalone, local_down, &last_choice);
}
/* release the post-transition release latch: one frame with the provider reporting not-down */
static void settle_lua(uint64_t instance, uint64_t* seq, unsigned mode)
{
    UssLuaMenuPost p = good(instance, ++*seq, mode, 0); UssMenuProviderSample z = none_c(); post(&p); frame(10, &z, 0, 0, 0);
}
static unsigned lines; static char log_lines[64][USS_MENU_DIAG_LINE];
static void logger(void* user, const char* line) { (void)user; assert(lines < 64); snprintf(log_lines[lines], USS_MENU_DIAG_LINE, "%s", line); ++lines; }

static DWORD WINAPI poster(LPVOID arg)
{
    unsigned long long n = (unsigned long long)(uintptr_t)arg, i; for (i = 1; i <= n; ++i) { UssLuaMenuPost p = good(7, i, 1 + (unsigned)(i & 1), (unsigned)((i >> 3) & 1)); (void)post(&p); } return 0;
}
static int fake_read(void* user, uintptr_t at, void* out, size_t length)
{
    const uint32_t* labels = user; uintptr_t base = 0x40000000u + 1248 + 57320; unsigned i = (unsigned)((at - base) / 4);
    assert(length == 4 && at >= base && at < base + 32 && ((at - base) % 4) == 0); memcpy(out, &labels[i], 4); return 1;
}
int main(void)
{
    UssMenuHandoverView v; UssLuaMenuPost p; UssMenuProviderSample zero = none_c(), taken; unsigned invalid = 99, i, r; uint64_t seq;
    /* ---- mailbox validation: every rejection class, nothing past `size` is read, no sample escapes ---- */
    reset();
    assert(uss_lua_mailbox_post(&box, NULL, 64) == USS_LUA_POST_NULL);
    p = good(1, 1, 1, 0); assert(uss_lua_mailbox_post(&box, &p, 63) == USS_LUA_POST_BAD_SIZE && uss_lua_mailbox_post(&box, &p, 65) == USS_LUA_POST_BAD_SIZE);
    p = good(1, 1, 1, 0); p.magic ^= 1; assert(post(&p) == USS_LUA_POST_BAD_MAGIC);
    p = good(1, 1, 1, 0); p.protocol = 1; assert(post(&p) == USS_LUA_POST_BAD_PROTOCOL);
    p = good(1, 1, 1, 0); p.size = 60; assert(post(&p) == USS_LUA_POST_BAD_PROTOCOL);
    p = good(1, 1, 1, 0); p.flags = 2; assert(post(&p) == USS_LUA_POST_BAD_FLAGS);
    p = good(0, 1, 1, 0); assert(post(&p) == USS_LUA_POST_BAD_FIELDS);
    p = good(1, 0, 1, 0); assert(post(&p) == USS_LUA_POST_BAD_FIELDS);
    p = good(1, 1, 3, 0); assert(post(&p) == USS_LUA_POST_BAD_FIELDS);
    p = good(1, 1, 0, 0); assert(post(&p) == USS_LUA_POST_BAD_FIELDS);
    p = good(1, 1, 1, 2); assert(post(&p) == USS_LUA_POST_BAD_FIELDS);
    p = good(1, 1, 1, 1); p.action_blocked = 1; assert(post(&p) == USS_LUA_POST_BAD_FIELDS);   /* pressed AND blocked is self-contradictory */
    p = good(1, 1, 1, 0); p.ready = 2; assert(post(&p) == USS_LUA_POST_BAD_FIELDS);
    p = good(1, 1, 1, 0); p.generation = 0; assert(post(&p) == USS_LUA_POST_BAD_FIELDS);
    /* a malformed record never becomes a sample; it yields exactly one INVALID sample (revoke) */
    assert(uss_lua_mailbox_take(&box, &taken, &invalid) == 1 && invalid == 1 && !uss_menu_provider_valid(&taken));
    assert(uss_lua_mailbox_take(&box, &taken, &invalid) == 0);
    p = good(5, 1, 2, 1); assert(post(&p) == USS_LUA_POST_ACCEPTED);
    assert(uss_lua_mailbox_take(&box, &taken, &invalid) == 1 && !invalid && uss_menu_provider_valid(&taken) && taken.mode == 2 && taken.utility_down == 1 && (taken.provider & USS_LUA_PROVIDER_BIT) && taken.sequence == 1);
    assert(uss_lua_mailbox_take(&box, &taken, &invalid) == 0);                       /* taken once */
    assert(post(&p) == USS_LUA_POST_REPLAY && uss_lua_mailbox_take(&box, &taken, &invalid) == 0);    /* replay: ignored, not a heartbeat, not a revoke */
    p = good(5, 2, 1, 0); assert(post(&p) == USS_LUA_POST_ACCEPTED);
    p = good(5, 1, 1, 0); assert(post(&p) == USS_LUA_POST_REPLAY);                     /* older sequence of the same instance */
    p = good(6, 1, 1, 0); assert(post(&p) == USS_LUA_POST_ACCEPTED);                   /* a new instance may restart at 1 */
    p = good(5, 9, 1, 0); assert(post(&p) == USS_LUA_POST_ACCEPTED);                   /* the OLD instance's later records are not special: last-writer rule */
    /* a good record posted after a malformed one in the same interval still reaches the next take */
    reset(); p = good(1, 1, 1, 0); post(&p); p.magic = 0; post(&p); p = good(1, 2, 1, 0); post(&p);
    assert(uss_lua_mailbox_take(&box, &taken, &invalid) == 1 && invalid); assert(uss_lua_mailbox_take(&box, &taken, &invalid) == 1 && !invalid && taken.sequence == 2);
    puts("mailbox: sizes, magic, protocol, flags, every field, replay, replacement, revoke-then-deliver PASS");

    /* Revocation discards only records ordered before it, including an unread press. */
    reset(); p = good(1, 1, 2, 0); post(&p); frame(1, &zero, 0, 0, 0);
    p = good(1, 2, 2, 1); post(&p); frame(1, &zero, 0, 0, 0); assert(handover.utility_down);
    p = good(1, 3, 2, 1); post(&p); p.magic = 0; post(&p);
    v = frame(1, &zero, 1, 0, 0); assert(!handover.usable && !v.utility_down && v.external);
    assert(!uss_lua_mailbox_take(&box, &taken, &invalid));
    p = good(1, 4, 2, 1); post(&p); v = frame(1, &zero, 0, 0, 0);
    assert(!v.utility_down && v.release_required && last_choice.sample == NULL);
    p = good(1, 5, 2, 0); post(&p); v = frame(1, &zero, 0, 0, 0); assert(!v.release_required);
    p = good(1, 6, 2, 1); post(&p); v = frame(1, &zero, 0, 0, 0); assert(v.utility_down);
    /* Consumer delay cannot renew a post's lease. Exact boundary is inclusive. */
    reset(); receiver_now = 100; p = good(1, 1, 2, 0); post(&p); now = 1099;
    v = frame(1, &zero, 0, 0, 0); assert(v.external && handover.received_ms == 100);
    v = frame(1, &zero, 1, 0, 0); assert(!handover.usable && !v.utility_down && v.external);
    reset(); receiver_now = 100; p = good(1, 1, 2, 1); post(&p); now = 1100;
    v = frame(1, &zero, 0, 0, 0); assert(!v.external && !v.utility_down);
    reset(); receiver_now = 100; p = good(1, 1, 2, 1); post(&p);
    v = frame(1, &zero, 0, 0, 0); assert(!v.external); /* receiver/consumer clock reversal */
    puts("revocation ordering, active-transaction loss, release recovery, receipt freshness/boundary/reversal, outward pointer lifetime PASS");

    /* Presence needs protocol 2 and consistent fields; a retired table generation cannot replay as newer input. */
    reset();p=good(1,1,2,0);p.flags=0;assert(post(&p)==USS_LUA_POST_BAD_FIELDS);
    p=good(1,2,2,0);p.generation=2;assert(post(&p)==USS_LUA_POST_ACCEPTED);
    p=good(1,3,2,1);assert(post(&p)==USS_LUA_POST_REPLAY);
    assert(box.received_ms==0 && box.slot.generation==2);
    puts("presence consistency and generation replay rejection PASS");

    /* ---- real concurrency: a poster thread against the game-thread take ---- */
    reset(); { HANDLE t = CreateThread(NULL, 0, poster, (LPVOID)(uintptr_t)200000, 0, NULL); uint64_t last = 0; unsigned long long taken_count = 0; DWORD wait;
        do { wait = WaitForSingleObject(t, 0);
             while (uss_lua_mailbox_take(&box, &taken, &invalid)) { assert(!invalid && uss_menu_provider_valid(&taken) && taken.sequence > last && taken.provider == (7 | USS_LUA_PROVIDER_BIT)); last = taken.sequence; ++taken_count; }
        } while (wait == WAIT_TIMEOUT);
        while (uss_lua_mailbox_take(&box, &taken, &invalid)) { last = taken.sequence; ++taken_count; }
        assert(last == 200000 && taken_count >= 1 && taken_count <= 200000); CloseHandle(t);
        printf("mailbox concurrency: 200000 posts, %llu coalesced takes, strictly increasing, every sample valid, final sequence delivered PASS\n", taken_count); }

    /* ---- composition with the real handover: provider states ---- */
    reset(); v = frame(10, &zero, 0, 0, 0); assert(!v.external && last_choice.source == USS_SOURCE_NONE);
    p = good(1, 1, 2, 0); p.bindings_registered = 0; post(&p); v = frame(10, &zero, 0, 0, 0); assert(!v.external);                 /* options only */
    p = good(1, 2, 2, 0); p.options_registered = 0; post(&p); v = frame(10, &zero, 0, 0, 0); assert(!v.external);                 /* bindings only */
    p = good(1, 3, 2, 0); p.ready = 0; post(&p); v = frame(10, &zero, 0, 0, 0); assert(!v.external);                                 /* registered but not ready */
    p = good(1, 4, 2, 0); p.input_valid = 0; post(&p); v = frame(10, &zero, 0, 0, 0); assert(!v.external);                           /* no usable input path */
    p = good(1, 5, 2, 0); post(&p); v = frame(10, &zero, 0, 0, 0); assert(v.external && v.mode == USS_BIND_DEDICATED && last_choice.source == USS_SOURCE_LUA);   /* both: late start works */
    printf("states: none, options-only, bindings-only, not-ready, no-input stay standalone; both+ready takes over (%u/%u)\n", v.external, v.mode);
    seq = 5; settle_lua(1, &seq, 2); p = good(1, ++seq, 2, 1); post(&p); v = frame(10, &zero, 0, 0, 0); assert(v.utility_down);          /* press after release latch */
    p = good(1, ++seq, 2, 0); post(&p); v = frame(10, &zero, 0, 0, 0); assert(!v.utility_down);                                              /* release */
    p = good(1, ++seq, 2, 1); p.action_blocked = 0; post(&p); frame(10, &zero, 0, 0, 0);
    v = frame(10, &zero, 0, 1, 0); assert(!v.utility_down && v.release_required);                                                           /* UI blocking revokes */
    /* retryable registration refusal: first refused, then accepted, then Apply changes the mode */
    reset(); p = good(2, 1, 1, 0); p.options_registered = 0; post(&p); v = frame(10, &zero, 0, 0, 0); assert(!v.external);
    p = good(2, 2, 1, 0); post(&p); v = frame(10, &zero, 0, 0, 0); assert(v.external && v.mode == USS_BIND_DEFAULT); seq = 2; settle_lua(2, &seq, 1);
    p = good(2, ++seq, 2, 0); post(&p); v = frame(10, &zero, 1, 0, 0); assert(v.mode == USS_BIND_DEFAULT);          /* Apply during an active weapon transaction waits */
    v = frame(10, &zero, 0, 0, 0); assert(v.mode == USS_BIND_DEDICATED && !v.utility_down);                       /* then applies (the release latch clears at once when the key is up) */
    /* lease: replays cannot keep it alive; expiry revokes a held press; a transaction defers the source change but not the input suppression */
    reset(); p = good(3, 1, 2, 0); post(&p); frame(10, &zero, 0, 0, 0); seq = 1; settle_lua(3, &seq, 2); p = good(3, ++seq, 2, 1); post(&p); v = frame(10, &zero, 0, 0, 0); assert(v.utility_down);
    for (i = 0; i < 4; ++i) { post(&p); v = frame(300, &zero, 0, 0, 0); }                                                                    /* replays at 300, 600, 900, 1200 ms */
    assert(!v.external && !v.utility_down);
    reset(); p = good(3, 1, 2, 0); post(&p); frame(10, &zero, 0, 0, 0); seq = 1; settle_lua(3, &seq, 2); p = good(3, ++seq, 2, 1); post(&p); frame(10, &zero, 0, 0, 0);
    v = frame(1100, &zero, 1, 0, 0); assert(v.external && !v.utility_down);                                   /* lost mid-transaction: still external, input suppressed */
    v = frame(10, &zero, 0, 0, 0); assert(!v.external);                                                      /* then standalone again */
    /* generation replacement: a restarted companion is a new provider; a held press from the new one needs a release first */
    reset(); p = good(10, 1, 2, 0); post(&p); frame(10, &zero, 0, 0, 0); seq = 1; settle_lua(10, &seq, 2);
    p = good(11, 1, 2, 1); assert(post(&p) == USS_LUA_POST_ACCEPTED); v = frame(10, &zero, 0, 0, 0); assert(v.external && !v.utility_down && v.release_required);
    p = good(11, 2, 2, 0); post(&p); frame(10, &zero, 0, 0, 0); p = good(11, 3, 2, 1); post(&p); v = frame(10, &zero, 0, 0, 0); assert(v.utility_down);
    /* a malformed record from a healthy owner revokes immediately */
    p = good(11, 4, 2, 1); p.mode = 9; post(&p); v = frame(10, &zero, 0, 0, 0); assert(!v.external && !v.utility_down);
    puts("composition: partial provider states, late start, retry, Apply, press/release, UI block, lease, replay, expiry, replacement, malformed revoke PASS");

    /* ---- arbitration: exactly one owner ---- */
    reset(); { UssMenuProviderSample c = c_sample(0x1000, 1, USS_BIND_DEFAULT, 0); unsigned switches_at_100;
        seq = 0; for (i = 0; i < 200; ++i) { p = good(20, ++seq, USS_BIND_DEDICATED, 0); post(&p); c.sequence++; v = frame(20, &c, 0, 0, 0); }
        assert(v.external && v.mode == USS_BIND_DEFAULT && arbiter.owner == USS_SOURCE_C && arbiter.switches == 1 && arbiter.suppressed_lua >= 199);   /* C wins; Lua set aside; no flapping */
        switches_at_100 = arbiter.switches; assert(switches_at_100 == 1);
        /* C dies (call site passes an invalid sample): input revoked at once, then Lua takes over within a few frames and needs a release */
        for (i = 0; i < 6; ++i) { p = good(20, ++seq, USS_BIND_DEDICATED, 0); post(&p); v = frame(20, &zero, 0, 0, 0); }
        assert(v.external && v.mode == USS_BIND_DEDICATED && arbiter.owner == USS_SOURCE_LUA && arbiter.switches == 2);
        /* C comes back while Lua is healthy: Lua keeps the menu (no flip-flop), the usable C sample is only counted */
        for (i = 0; i < 100; ++i) { p = good(20, ++seq, USS_BIND_DEDICATED, 0); post(&p); c.sequence++; v = frame(20, &c, 0, 0, 0); }
        assert(arbiter.owner == USS_SOURCE_LUA && arbiter.switches == 2 && arbiter.suppressed_c >= 99 && v.mode == USS_BIND_DEDICATED); }
    /* a malformed Lua post cannot disturb a healthy C owner */
    reset(); { UssMenuProviderSample c = c_sample(0x2000, 1, USS_BIND_DEFAULT, 0); unsigned k; for (k = 0; k < 20; ++k) { c.sequence++; frame(10, &c, 0, 0, 0); }
        p = good(1, 1, 1, 0); p.flags = 7; post(&p); c.sequence++; v = frame(10, &c, 0, 0, 0); assert(v.external && arbiter.owner == USS_SOURCE_C); }
    /* existing C-only handover is unchanged: the composed step equals the bare policy frame by frame */
    reset(); { UssMenuHandover bare; UssMenuProviderSample c = c_sample(0x3000, 1, USS_BIND_DEDICATED, 0); unsigned k; uss_menu_handover_init(&bare);
        for (k = 0; k < 300; ++k) {
            UssMenuProviderSample* s = (k % 97 == 50) ? &zero : &c; UssMenuHandoverView a, b; unsigned trans = (k % 41) == 7, blocked = (k % 53) == 11;
            c.sequence = k + 1; c.utility_down = (k / 5) & 1; now += 16;
            a = uss_menu_external_step(&handover, &arbiter, &box, s, now, trans, blocked, &standalone, 0, NULL);
            b = uss_menu_handover_step(&bare, s, now, trans, blocked, &standalone, 0);
            assert(a.external == b.external && a.mode == b.mode && a.utility_down == b.utility_down && a.release_required == b.release_required); } }
    puts("arbitration: C preferred, one owner, no flapping, Lua takes over after loss, malformed Lua cannot disturb C, C-only path identical to the bare policy PASS");

    /* ---- every partial state leaves USS standalone (so it stays reachable); the strict-menu collision applies there (see report) ---- */
    reset(); { unsigned standalone_states = 0; unsigned variants[4][4] = {{1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 0, 1}, {1, 1, 1, 0}}; /* options, bindings, ready, input */
        for (i = 0; i < 4; ++i) { reset(); p = good(40 + i, 1, 2, 0); p.options_registered = variants[i][0]; p.bindings_registered = variants[i][1]; p.ready = variants[i][2]; p.input_valid = variants[i][3];
            post(&p); v = frame(10, &zero, 0, 0, 0); if (!v.external) ++standalone_states; }
        assert(standalone_states == 4); puts("fallback: options-only, bindings-only, not-ready and no-input all leave the standalone menu in charge (4/4)"); }

    /* Diagnostic registration/readiness comes from the active Lua copy, not C discovery. */
    reset(); {unsigned options,bindings; const char* status;
      status=uss_menu_diag_lua_snapshot(&box,0,&options,&bindings);assert(!strcmp(status,"NO_POST"));
      p=good(1,1,2,0);post(&p);status=uss_menu_diag_lua_snapshot(&box,0,&options,&bindings);
      assert(options==1 && bindings==1 && !strcmp(status,"READY"));
      assert(!strcmp(uss_menu_diag_lua_snapshot(&box,1001,&options,&bindings),"STALE"));
      p.sequence++;p.ready=0;post(&p);assert(!strcmp(uss_menu_diag_lua_snapshot(&box,0,&options,&bindings),"WAIT_READY_OR_VALUE"));
      p.sequence++;p.input_valid=0;post(&p);assert(!strcmp(uss_menu_diag_lua_snapshot(&box,0,&options,&bindings),"INPUT_UNAVAILABLE"));
      p.sequence++;p.bindings_registered=0;post(&p);assert(!strcmp(uss_menu_diag_lua_snapshot(&box,0,&options,&bindings),"WAIT_REGISTRATION"));
      p.magic=0;post(&p);uss_lua_mailbox_take(&box,&taken,&invalid);
      assert(!strcmp(uss_menu_diag_lua_snapshot(&box,0,&options,&bindings),"REJ_BAD_MAGIC")); }
    puts("Lua diagnostic snapshot: registrations, readiness/value wait, read refusal, expiry and drained revocation PASS");

    /* ---- diagnostics: change-only, once per opening, bounded, classified ---- */
    { UssMenuDiagnostics d; UssMenuOpenEvidence e; uint32_t labels[8] = {0xd876b36eu, 0x78934e12u, 0x8c02bd80u, USS_MENU_TEMPLATE_LABEL, USS_MENU_TEMPLATE_LABEL, 0, 0, 0}; UssMenuHostSnapshot host; UssMenuObservationIo io;
      memset(&d, 0, sizeof d); lines = 0;
      for (i = 0; i < 1000; ++i) uss_menu_diag_provider(&d, logger, NULL, USS_SOURCE_NONE, 0, 0, 0, "WAIT_API", "NO_POST");
      assert(lines == 1); assert(uss_menu_diag_provider(&d, logger, NULL, USS_SOURCE_LUA, 1, 1, 1, "WAIT_API", "POSTING") && lines == 2);
      memset(&host, 0, sizeof host); host.valid = host.escape_present = host.topmost = 1; host.screen = 0x40000000u; host.tab_count = 5; host.uss_tab = 4; host.ours_intact = 1;
      memset(&io, 0, sizeof io); io.read = fake_read; io.user = labels; uss_menu_diag_collect(&io, &host, &e);
      assert(e.observed && e.tab_count == 5 && e.uss_tab == 4 && e.template_at_3 && e.foreign_template_tab);
      assert(!strcmp(uss_menu_diag_case(&e, 0, 0), "STANDALONE_AND_FOREIGN_TEMPLATE_TABS"));
      host.uss_tab = UINT32_MAX; host.tab_count = 4; labels[4] = 0; uss_menu_diag_collect(&io, &host, &e); assert(!strcmp(uss_menu_diag_case(&e, 0, 0), "FOREIGN_TEMPLATE_TAB_PRESENT_BEFORE_APPEND"));
      assert(!strcmp(uss_menu_diag_case(&e, 0, 1), "PROVIDER_API_WAIT_RETRYABLE") && !strcmp(uss_menu_diag_case(&e, 1, 1), "EXTERNAL_PROVIDER_OWNS_MENU"));
      labels[3] = 0; host.tab_count = 3; uss_menu_diag_collect(&io, &host, &e); assert(!strcmp(uss_menu_diag_case(&e, 0, 0), "STANDALONE_PENDING"));
      host.uss_tab = 3; host.tab_count = 4; host.foreign_owner = 1; uss_menu_diag_collect(&io, &host, &e); assert(!strcmp(uss_menu_diag_case(&e, 0, 0), "STANDALONE_OWNERSHIP_LOST"));
      host.foreign_owner = 0; host.ours_intact = 0; uss_menu_diag_collect(&io, &host, &e); assert(!strcmp(uss_menu_diag_case(&e, 0, 0), "STANDALONE_ROWS_UNAVAILABLE"));
      host.ours_intact = 1; uss_menu_diag_collect(&io, &host, &e); assert(!strcmp(uss_menu_diag_case(&e, 0, 0), "STANDALONE_ACTIVE"));
      host.language_changing = 1; uss_menu_diag_collect(&io, &host, &e); assert(!strcmp(uss_menu_diag_case(&e, 0, 0), "LANGUAGE_CHANGING"));
      host.language_changing = 0; lines = 0; memset(&d, 0, sizeof d);
      for (i = 0; i < 50; ++i) { assert(uss_menu_diag_open(&d, logger, NULL, &e, USS_SOURCE_NONE, 0, 0, "APPEND")); assert(!uss_menu_diag_open(&d, logger, NULL, &e, USS_SOURCE_NONE, 0, 0, "APPEND")); uss_menu_diag_closed(&d); }
      assert(lines == USS_MENU_DIAG_MAX_LINES + 1 && strstr(log_lines[USS_MENU_DIAG_MAX_LINES], "line cap reached"));   /* bounded, and the cap itself is announced once */
      for (r = 0; r < lines; ++r) { assert(!strstr(log_lines[r], "0x") && !strstr(log_lines[r], ":\\") && strlen(log_lines[r]) < USS_MENU_DIAG_LINE - 1); }
      printf("diagnostics: %u lines kept (cap %u); sample: %s\n", lines, USS_MENU_DIAG_MAX_LINES, log_lines[0]);
      lines = 0; memset(&d, 0, sizeof d); memset(&e, 0, sizeof e); e.observed = 1; e.tab_count = 4; e.uss_tab = UINT32_MAX; e.template_at_3 = 1; e.foreign_template_tab = 1; e.rows_intact = 0;
      uss_menu_diag_open(&d, logger, NULL, &e, USS_SOURCE_NONE, 0, 0, "APPEND"); printf("sample open line (strict-MOM shape before append): %s\n", log_lines[0]);
      lines = 0; memset(&d, 0, sizeof d); uss_menu_diag_provider(&d, logger, NULL, USS_SOURCE_LUA, 1, 1, 1, "NOT_LOADED", "POSTING");
      printf("sample provider line: %s\n", log_lines[0]); }
    puts("USS LUA MENU BRIDGE HARNESS: PASS");
    return 0;
}
