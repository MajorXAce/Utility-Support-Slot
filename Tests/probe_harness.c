/* Own synthetic host only. Include the actual implementation, not a policy copy. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "../src/probe.c"

static HfApi fake;
static HfSession observed;
static HfTickCallback retained;
static unsigned logs, reads, registrations;
static int register_ok;
static int retain_on_failure;
static int deliver_during_registration;
static const char* build;
static char mod_token;
#define MOD ((HfMod*)&mod_token)

static void log_line(HfMod* mod, const char* format, ...)
{
    (void)format;
    assert(mod == MOD);
    ++logs;
}
static const char* build_id(void) { return build; }
static void session_read(HfMod* mod, HfSession* out)
{
    assert(mod == MOD);
    ++reads;
    *out = observed;
}
static int register_tick(HfMod* mod, HfTickCallback callback, void* user)
{
    assert(mod == MOD && user == NULL);
    ++registrations;
    if (register_ok || retain_on_failure) {
        retained = callback;
        if (deliver_during_registration) callback(mod, 0.016f, user);
    }
    return register_ok;
}
static void reset(void)
{
    /* Each case represents a new DLL/session. Test-only reset, no live restart API. */
    memset(&fake, 0, sizeof(fake));
    memset(&observed, 0, sizeof(observed));
    memset(&initial_session, 0, sizeof(initial_session));
    sdk = NULL;
    remaining = 0;
    retained = NULL;
    logs = reads = registrations = 0;
    register_ok = 1;
    retain_on_failure = 0;
    deliver_during_registration = 0;
    build = "25480438";
    observed.version = 17;
    fake.version = HF_MOD_SDK_VERSION;
    fake.size = (uint32_t)sizeof(fake);
    fake.log = log_line;
    fake.build_id = build_id;
    fake.session = session_read;
    fake.on_game_thread = register_tick;
    /* Every memory/write/hook/gameplay API remains NULL: accidental use fails. */
}
static void rejected(void)
{
    assert(hellforge_mod_init(&fake, MOD) != 0);
    assert(registrations == 0 && retained == NULL);
}
int main(void)
{
    unsigned i;
    reset(); assert(hellforge_mod_init(NULL, MOD) != 0);
    reset(); assert(hellforge_mod_init(&fake, NULL) != 0);
    reset(); fake.version=USS_MIN_RUNTIME_SDK-1; rejected();
    reset(); ++fake.version; assert(hellforge_mod_init(&fake,MOD)==0);
    reset(); fake.size=(uint32_t)USS_REQUIRED_API_BYTES-1; rejected();
    reset(); ++fake.size; assert(hellforge_mod_init(&fake,MOD)==0);
    reset(); fake.log = NULL; rejected();
    reset(); fake.build_id = NULL; rejected();
    if (!HF_USS_ENABLE_PROBE) {
        reset(); fake.session = NULL; fake.on_game_thread = NULL;
        assert(hellforge_mod_init(&fake, MOD) == 0);
        assert(logs == 1 && reads == 0 && registrations == 0);
        puts("inactive: guards and no registration/session access PASS");
    } else {
        reset(); fake.session = NULL; rejected();
        reset(); fake.on_game_thread = NULL; rejected();
        reset(); build = NULL; rejected();
        reset(); build = "unknown"; rejected();
        reset(); observed.role = HF_SESSION_HOST; rejected();
        reset(); observed.role = HF_SESSION_JOINER; rejected();
        reset(); observed.role = 99; rejected();
        reset(); register_ok = 0;
        assert(hellforge_mod_init(&fake, MOD) == 0);
        assert(registrations == 1 && retained == NULL && remaining == 0 && logs == 2);
        reset(); register_ok = 0; retain_on_failure = 1;
        assert(hellforge_mod_init(&fake, MOD) == 0);
        assert(retained != NULL);
        for (i = 0; i < 100; ++i) retained(MOD, 0.016f, NULL);
        assert(reads == 1 && logs == 2 && remaining == 0 && registrations == 1);
        reset(); register_ok = 0; retain_on_failure = 1; deliver_during_registration = 1;
        assert(hellforge_mod_init(&fake, MOD) == 0);
        for (i = 0; i < 100; ++i) retained(MOD, 0.016f, NULL);
        assert(reads == 2 && logs == 3 && remaining == 0 && registrations == 1);
        reset(); assert(hellforge_mod_init(&fake, MOD) == 0);
        assert(retained != NULL && registrations == 1 && reads == 1);
        for (i = 0; i < 100; ++i) retained(MOD, 0.016f, NULL);
        assert(reads == 4 && logs == 4 && remaining == 0 && registrations == 1);
        reset(); deliver_during_registration = 1;
        assert(hellforge_mod_init(&fake, MOD) == 0);
        assert(reads == 2 && logs == 2 && remaining == 2);
        for (i = 0; i < 100; ++i) retained(MOD, 0.016f, NULL);
        assert(reads == 4 && logs == 4 && registrations == 1);
        reset(); assert(hellforge_mod_init(&fake, MOD) == 0);
        ++observed.version;
        for (i = 0; i < 100; ++i) retained(MOD, 0.016f, NULL);
        assert(reads == 2 && logs == 2 && remaining == 0 && registrations == 1);
        for (i = 1; i <= 3; ++i) {
            reset(); assert(hellforge_mod_init(&fake, MOD) == 0);
            observed.role = i; /* host, joiner, unknown */
            retained(MOD, 0.016f, NULL);
            observed.role = HF_SESSION_SOLO;
            retained(MOD, 0.016f, NULL);
            assert(reads == 2 && logs == 2 && remaining == 0 && registrations == 1);
        }
        puts("enabled: refusal, budget, context stop, retained no-op, no retry PASS");
    }
    printf("ABI HfApi=%zu HfSession=%zu pointer=%zu\n",
           sizeof(HfApi), sizeof(HfSession), sizeof(void*));
    return 0;
}
