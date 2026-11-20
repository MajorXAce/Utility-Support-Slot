/* Bounded menu diagnostics (provider and tab ownership evidence). game thread only, no allocation, no pointers or paths in any line.
 *
 * Two kinds of line, both rate-limited by CONSTRUCTION, not by timers:
 *   provider line: written only when the compact provider state CHANGES (which source owns the menu, registration/readiness, refusal reason, lease). Never per frame.
 *   open line:     written once per escape-menu opening ("generation"), with the tab evidence needed to tell the reporter-relevant cases apart (see uss_menu_diag_case).
 * A session cap (USS_MENU_DIAG_MAX_LINES) bounds the total; the cap itself is announced once. These lines are INFO and do not need debug configuration.
 *
 * What the evidence can and cannot say: tab labels are read from the native tab bar (one 4-byte read per tab slot, at most 8), the template label 0xC67C7FAF is shared by USS and by
 * the original Mod Options Menu, so the label ALONE never proves who owns a tab; ownership of the USS tab is taken only from the title identity the observer already verifies
 * (host.uss_tab). "foreign_template_tab" therefore means: a tab other than ours carries the shared template, which is a collision RISK flag, not an ownership claim. */
#ifndef USS_MENU_DIAGNOSTICS_H
#define USS_MENU_DIAGNOSTICS_H
#include <stdio.h>
#include "uss_menu_arbiter.h"
#include "uss_menu_ui_policy.h"
#include "uss_native_menu_observation.h"

enum { USS_MENU_DIAG_MAX_LINES = 48, USS_MENU_DIAG_LINE = 320 };
#define USS_MENU_TEMPLATE_LABEL UINT32_C(0xc67c7faf)   /* the label USS and the original Mod Options Menu both use for a text-only tab */
typedef void (*UssMenuDiagLog)(void* user, const char* line);

/* Everything a provider line says. Strings must be static literals (they are compared by value, never retained). */
typedef struct UssMenuProviderDiag {
    unsigned source, external, options, bindings;
    char c_status[24], lua_status[24];
} UssMenuProviderDiag;
typedef struct UssMenuDiagnostics {
    UssMenuUiPolicy last_ui;unsigned have_ui,ui_usable;
    UssMenuProviderDiag last; unsigned have_last;
    unsigned lines, capped, open_reported, generation;
} UssMenuDiagnostics;
/* What one opening shows. tab_count 0 means the host could not be observed. */
typedef struct UssMenuOpenEvidence {
    unsigned observed, tab_count, uss_tab, template_at_3, foreign_template_tab, rows_intact, ownership_lost, language_changing, topmost;
} UssMenuOpenEvidence;

static inline void uss_menu_diag_copy(char* out, size_t size, const char* text)
{
    size_t i = 0; if (!text) text = "NONE";
    for (; i + 1 < size && text[i]; ++i) out[i] = text[i];
    out[i] = 0;
}
static inline void uss_menu_diag_emit(UssMenuDiagnostics* d, UssMenuDiagLog log, void* user, const char* line)
{
    if (!d || !log) return;
    if (d->lines >= USS_MENU_DIAG_MAX_LINES) {
        if (!d->capped) { d->capped = 1; log(user, "USS menu diagnostics: line cap reached for this session; further menu lines suppressed"); }
        return;
    }
    ++d->lines; log(user, line);
}
/* Short fixed status of the Lua companion for the provider line. `last_rejected` is a UssLuaPostResult; names are literals so they can be compared by value. */
static inline const char* uss_menu_diag_lua_status(unsigned accepted, unsigned rejected, unsigned last_rejected, int posting_now)
{
    static const char* const rej[USS_LUA_POST_RESULT_COUNT] = {"REJ_NONE", "REJ_NULL", "REJ_BAD_SIZE", "REJ_BAD_MAGIC", "REJ_BAD_PROTOCOL", "REJ_BAD_FLAGS", "REJ_BAD_FIELDS", "REJ_REPLAY"};
    if (rejected && last_rejected < USS_LUA_POST_RESULT_COUNT && !posting_now) return rej[last_rejected];
    if (posting_now) return "POSTING";
    return accepted ? "STALE" : "NO_POST";
}
/* Snapshot current Lua transport state under its copy lock; diagnostics never renew a lease. */
static inline const char* uss_menu_diag_lua_snapshot(UssLuaMenuMailbox* box, uint64_t now_ms, unsigned* options, unsigned* bindings)
{
    UssLuaMenuPost p; uint64_t received; unsigned revoked, accepted, rejection;
    AcquireSRWLockExclusive(&box->lock);
    p = box->slot; received = box->received_ms; revoked = box->invalidated;
    accepted = box->counts[USS_LUA_POST_ACCEPTED]; rejection = box->last_rejected;
    ReleaseSRWLockExclusive(&box->lock);
    *options = p.options_registered; *bindings = p.bindings_registered;
    if (revoked) return uss_menu_diag_lua_status(accepted, 1, rejection, 0);
    if (!accepted) return "NO_POST";
    if (now_ms < received || now_ms - received > USS_MENU_HANDOVER_TIMEOUT_MS) return "STALE";
    if (!p.options_registered || !p.bindings_registered) return "WAIT_REGISTRATION";
    if (!p.input_valid) return "INPUT_UNAVAILABLE";
    if (!p.ready) return "WAIT_READY_OR_VALUE";
    if (p.action_blocked) return "INPUT_BLOCKED";
    return "READY";
}
/* Provider state line, only on change. `c_status`/`lua_status` are short fixed reason names (e.g. NOT_LOADED, WAIT_API, EXPORTS_UNAVAILABLE, API_UNAVAILABLE, WAIT_READY,
 * REGISTRATION_REFUSED, READY, NO_POST, POSTING, STALE, REJECTED_<result>). Returns 1 if a line was written. */
static inline int uss_menu_diag_provider(UssMenuDiagnostics* d, UssMenuDiagLog log, void* user, unsigned source, unsigned external,
    unsigned options, unsigned bindings, const char* c_status, const char* lua_status)
{
    UssMenuProviderDiag now; char line[USS_MENU_DIAG_LINE];
    memset(&now, 0, sizeof now);
    now.source = source; now.external = external; now.options = options; now.bindings = bindings;
    uss_menu_diag_copy(now.c_status, sizeof now.c_status, c_status); uss_menu_diag_copy(now.lua_status, sizeof now.lua_status, lua_status);
    if (!d || (d->have_last && !memcmp(&d->last, &now, sizeof now))) return 0;
    d->last = now; d->have_last = 1;
    snprintf(line, sizeof line, "USS menu provider: owner=%s external=%u options_registered=%u bindings_registered=%u c_provider=%s lua_provider=%s",
        uss_menu_source_name(source), external, options, bindings, now.c_status, now.lua_status);
    uss_menu_diag_emit(d, log, user, line);
    return 1;
}
/* The one-line classification of an opening. Order matters: the most specific evidence wins. Names are for support, not proof of the reporter's cause. */
static inline const char* uss_menu_diag_case(const UssMenuOpenEvidence* e, unsigned external, unsigned provider_wait)
{
    if (!e || !e->observed) return "HOST_UNOBSERVED";
    if (e->language_changing) return "LANGUAGE_CHANGING";
    if (external) return "EXTERNAL_PROVIDER_OWNS_MENU";
    if (provider_wait) return "PROVIDER_API_WAIT_RETRYABLE";
    if (e->uss_tab != UINT32_MAX && e->ownership_lost) return "STANDALONE_OWNERSHIP_LOST";
    if (e->uss_tab != UINT32_MAX && !e->rows_intact) return "STANDALONE_ROWS_UNAVAILABLE";
    if (e->uss_tab == UINT32_MAX && e->foreign_template_tab) return "FOREIGN_TEMPLATE_TAB_PRESENT_BEFORE_APPEND";
    if (e->uss_tab != UINT32_MAX && e->foreign_template_tab) return "STANDALONE_AND_FOREIGN_TEMPLATE_TABS";
    if (e->uss_tab != UINT32_MAX) return "STANDALONE_ACTIVE";
    return "STANDALONE_PENDING";
}
/* Read-only evidence from an already-observed host: the observer's own snapshot plus at most 8 four-byte label reads. */
static inline void uss_menu_diag_collect(const UssMenuObservationIo* io, const UssMenuHostSnapshot* host, UssMenuOpenEvidence* out)
{
    unsigned i; uint32_t label;
    memset(out, 0, sizeof *out); out->uss_tab = UINT32_MAX;
    if (!io || !io->read || !host || !host->valid || !host->escape_present || !host->screen) return;
    out->observed = 1; out->tab_count = host->tab_count; out->uss_tab = host->uss_tab; out->ownership_lost = host->foreign_owner;
    out->language_changing = host->language_changing; out->topmost = host->topmost; out->rows_intact = host->ours_intact;
    for (i = 0; i < host->tab_count && i < 8; ++i) {
        if (!io->read(io->user, (uintptr_t)host->screen + 1248 + 57320 + 4 * (uintptr_t)i, &label, 4)) continue;
        if (i == 3) out->template_at_3 = label == USS_MENU_TEMPLATE_LABEL;
        if (label == USS_MENU_TEMPLATE_LABEL && i != host->uss_tab) out->foreign_template_tab = 1;
    }
}
/* Once per opening. `outcome` is a static literal naming what USS does next or did (e.g. APPEND, DEFERRED_TO_EXTERNAL, RETIRE, RESTORE, NONE). */
static inline int uss_menu_diag_open(UssMenuDiagnostics* d, UssMenuDiagLog log, void* user, const UssMenuOpenEvidence* e,
    unsigned source, unsigned external, unsigned provider_wait, const char* outcome)
{
    char line[USS_MENU_DIAG_LINE]; char uss_tab[8];
    if (!d || !e || d->open_reported) return 0;
    d->open_reported = 1; ++d->generation;
    if (e->uss_tab == UINT32_MAX) snprintf(uss_tab, sizeof uss_tab, "none"); else snprintf(uss_tab, sizeof uss_tab, "%u", e->uss_tab);
    snprintf(line, sizeof line, "USS menu open gen=%u case=%s owner=%s external=%u tabs=%u uss_tab=%s template_at_3=%u foreign_template_tab=%u rows_intact=%u outcome=%s",
        d->generation, uss_menu_diag_case(e, external, provider_wait), uss_menu_source_name(source), external, e->tab_count, uss_tab, e->template_at_3,
        e->foreign_template_tab, e->rows_intact, outcome ? outcome : "NONE");
    uss_menu_diag_emit(d, log, user, line);
    return 1;
}
/* The escape menu closed: the next opening gets its own line. */
static inline void uss_menu_diag_closed(UssMenuDiagnostics* d) { if (d) d->open_reported = 0; }
/* Presence is a UI restriction, never a claim that a full external input source works. */
static inline void uss_menu_diag_ui(UssMenuDiagnostics* d,UssMenuDiagLog log,void* user,const UssMenuUiPolicy* ui,unsigned usable)
{
    char line[USS_MENU_DIAG_LINE];
    if(!d || !ui || (d->have_ui && !memcmp(&d->last_ui,ui,sizeof *ui) && d->ui_usable==usable))return;
    d->last_ui=*ui;d->have_ui=1;d->ui_usable=usable;
    snprintf(line,sizeof line,"USS menu UI: lua_options_present=%u companion_fresh=%u usable=%u standalone_deferred=%u close_reopen_required=%u generation=%u",
        ui->presence,ui->companion_fresh,usable,ui->deferred,ui->close_required,ui->generation);
    uss_menu_diag_emit(d,log,user,line);
}
#endif
