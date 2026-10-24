/*
 * libkmod.so - offline single-player helper for net.kairosoft.android.horse_en
 * (Unity 2022.3.62f2, IL2CPP, arm64).
 *
 * Hooks (RVAs inside libil2cpp.so, verified byte-for-byte before patching):
 *   Human.GetParamUpValue(...)   @0xec5a18  -> training multiplier (scales the returned int[][] of {status,delta})
 *   HorseData.FatigueUp(int)     @0xe5e40c  -> block fatigue gain
 *   HorseData.GetStatus(int)     @0xe57258  -> keep fatigue at 0
 *
 * Freestanding (no NDK headers): only libc symbols declared by hand.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
typedef unsigned long uptr;

#define EXPORT __attribute__((visibility("default")))

extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern int mprotect(void *, unsigned long, int);
extern void *mmap(void *, unsigned long, int, int, int, long);
extern int pthread_create(unsigned long *, const void *, void *(*)(void *), void *);
extern int usleep(unsigned int);

#define PROT_R 1
#define PROT_W 2
#define PROT_X 4
#define RTLD_NOLOAD 4
#define RTLD_NOW 2
#define MAP_PRIV_ANON 0x22

#define RVA_PARAMUP 0xec5a18UL
#define RVA_FATIGUEUP 0xe5e40cUL
#define RVA_GETSTATUS 0xe57258UL
#define RVA_EXPORT_DOMAIN_GET 0xcf93e0UL /* il2cpp_domain_get, used to find the load base */

static const u8 EXP_PARAMUP[16] = {0xff, 0x43, 0x02, 0xd1, 0xfd, 0x7b, 0x03, 0xa9, 0xfc, 0x6f, 0x04, 0xa9, 0xfa, 0x67, 0x05, 0xa9};
static const u8 EXP_FATIGUEUP[16] = {0xfe,0x0f,0x1c,0xf8, 0xf8,0x5f,0x01,0xa9, 0xf6,0x57,0x02,0xa9, 0xf4,0x4f,0x03,0xa9};
static const u8 EXP_GETSTATUS[16] = {0xfe,0x0f,0x1d,0xf8, 0xf6,0x57,0x01,0xa9, 0xf4,0x4f,0x02,0xa9, 0x55,0x8f,0x00,0xd0};

/* ---------------- shared state (controlled from Java via JNI) ---------------- */
static volatile int g_mult = 1;      /* training multiplier, 1..50 */
static volatile int g_nofat = 0;     /* 1 = fatigue remover on */
static volatile int g_state = 0;     /* 0 waiting, 1 all hooks active, 2 failed */
static volatile int g_fail = 0;      /* bitmask of hooks that failed */
static volatile int g_started = 0;

/* ---------------- original-function trampolines ---------------- */
typedef uptr (*fn_ParamUp)(void *self, u64 a1, u64 a2, u64 a3, u64 a4);
typedef uptr (*fn_FatigueUp)(void *self, int delta, void *mi);
typedef int (*fn_GetStatus)(void *self, int idx, void *mi);

static fn_ParamUp o_ParamUp;
static fn_FatigueUp o_FatigueUp;
static fn_GetStatus o_GetStatus;

/* ---------------- hooks ---------------- */
/* Human.GetParamUpValue returns int[][]; each inner int[] = { statusIndex, delta }.
 * It builds a fresh array on every call and is only called from Human.TrainingUp, so
 * scaling the deltas here changes both the applied gain and the on-screen result.
 * Status 2..9 = stamina, speed, dexterity, acceleration, spirit, gate, iq, strong. */
static uptr h_ParamUp(void *self, u64 a1, u64 a2, u64 a3, u64 a4)
{
    uptr r = o_ParamUp(self, a1, a2, a3, a4);
    int m = g_mult;
    if (m > 1 && r) {
        u8 *outer = (u8 *)r;
        int n = *(int *)(outer + 0x18);
        if (n > 0 && n < 64) {
            for (int i = 0; i < n; i++) {
                u8 *inner = *(u8 **)(outer + 0x20 + 8 * i);
                if (!inner) continue;
                if (*(int *)(inner + 0x18) < 2) continue;
                int st = *(int *)(inner + 0x20);
                int d = *(int *)(inner + 0x24);
                if (st >= 2 && st <= 9 && d > 0) {
                    i64 v = (i64)d * (i64)m;
                    if (v > 100000) v = 100000;
                    *(int *)(inner + 0x24) = (int)v;
                }
            }
        }
    }
    return r;
}

static uptr h_FatigueUp(void *self, int delta, void *mi)
{
    if (g_nofat) return 0;
    return o_FatigueUp(self, delta, mi);
}

static int h_GetStatus(void *self, int idx, void *mi)
{
    if (g_nofat && idx == 1 && self) {
        /* HorseData.status_ is an int[] at +0x50; elements start at +0x20 */
        u8 *arr = *(u8 **)((u8 *)self + 0x50);
        if (arr && *(int *)(arr + 0x18) > 1) *(int *)(arr + 0x24) = 0;
        return 0;
    }
    return o_GetStatus(self, idx, mi);
}

/* ---------------- tiny arm64 inline hook ---------------- */
static void flush_icache(void *start, unsigned long len)
{
#ifndef __aarch64__
    (void)start; (void)len; return;
#else
    u64 ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    u64 dline = 4UL << ((ctr >> 16) & 0xf);
    u64 iline = 4UL << (ctr & 0xf);
    u64 a = (u64)start & ~(dline - 1);
    u64 end = (u64)start + len;
    for (; a < end; a += dline) __asm__ volatile("dc cvau, %0" ::"r"(a) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    a = (u64)start & ~(iline - 1);
    for (; a < end; a += iline) __asm__ volatile("ic ivau, %0" ::"r"(a) : "memory");
    __asm__ volatile("dsb ish\n isb" ::: "memory");
#endif
}

#define POOL_SIZE 4096

/* one private page per hook, so a W^X fallback never touches a live trampoline */
static void *tramp_page(int *is_rwx)
{
    void *p = mmap(0, POOL_SIZE, PROT_R | PROT_W | PROT_X, MAP_PRIV_ANON, -1, 0);
    if (p != (void *)-1L && p) { *is_rwx = 1; return p; }
    p = mmap(0, POOL_SIZE, PROT_R | PROT_W, MAP_PRIV_ANON, -1, 0);
    if (p != (void *)-1L && p) { *is_rwx = 0; return p; }
    return 0;
}

static int mem_eq(const u8 *a, const u8 *b, int n)
{
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static void put64(u8 *dst, u64 v)
{
    for (int i = 0; i < 8; i++) dst[i] = (u8)(v >> (8 * i));
}

/* Returns 0 on success. Patches 16 bytes at target: ldr x16,#8 ; br x16 ; .quad hook */
static int install_hook(uptr target, const u8 *expect, void *hook, void **orig_out)
{
    u32 *src = (u32 *)target;
    if (!mem_eq((const u8 *)src, expect, 16)) return 1; /* different build: refuse */

    int is_rwx = 0;
    u32 *t = (u32 *)tramp_page(&is_rwx);
    if (!t) return 2;
    u8 *pool = (u8 *)t + 32; /* literal pool: [0]=return addr, [1+i]=adrp target of instr i */

    for (int i = 0; i < 4; i++) {
        u32 ins = src[i];
        if ((ins & 0x9f000000) == 0x90000000) { /* ADRP */
            u32 rd = ins & 0x1f;
            i64 imm = ((i64)((ins >> 5) & 0x7ffff) << 2) | ((ins >> 29) & 3);
            if (imm & (1 << 20)) imm |= ~((i64)(1 << 21) - 1);
            u64 pc = (u64)&src[i];
            u64 tgt = ((pc >> 12) + imm) << 12;
            put64(pool + 8 * (1 + i), tgt);
            int off = 32 + 8 * (1 + i) - 4 * i;
            t[i] = 0x58000000u | ((u32)(off / 4) << 5) | rd; /* ldr xRd, literal */
        } else if ((ins & 0x1f000000) == 0x10000000 /* ADR */ ||
                   (ins & 0x3b000000) == 0x18000000 /* LDR literal */ ||
                   (ins & 0x7c000000) == 0x14000000 /* B/BL */ ||
                   (ins & 0xff000010) == 0x54000000 /* B.cond */ ||
                   (ins & 0x7e000000) == 0x34000000 /* CBZ/CBNZ */ ||
                   (ins & 0x7e000000) == 0x36000000 /* TBZ/TBNZ */) {
            return 3; /* cannot relocate */
        } else {
            t[i] = ins;
        }
    }
    put64(pool, (u64)target + 16);
    t[4] = 0x58000000u | ((u32)((32 - 16) / 4) << 5) | 16; /* ldr x16, ret_addr */
    t[5] = 0xd61f0200u;                                       /* br x16 */
    if (!is_rwx && mprotect(t, POOL_SIZE, PROT_R | PROT_X) != 0) return 5;
    flush_icache(t, 64);
    *orig_out = (void *)t;

    /* patch the target function */
    u64 page = target & ~0xfffUL;
    unsigned long plen = ((target + 16 + 0xfff) & ~0xfffUL) - page;
    int rwx = (mprotect((void *)page, plen, PROT_R | PROT_W | PROT_X) == 0);
    if (!rwx && mprotect((void *)page, plen, PROT_R | PROT_W) != 0) return 4;
    src[0] = 0x58000050u; /* ldr x16, #8 */
    src[1] = 0xd61f0200u; /* br  x16     */
    put64((u8 *)&src[2], (u64)hook);
    mprotect((void *)page, plen, PROT_R | PROT_X);
    flush_icache(src, 16);
    return 0;
}

/* ---------------- loader thread ---------------- */
static void *loader(void *arg)
{
    (void)arg;
    uptr base = 0;
    for (int i = 0; i < 2400 && !base; i++) { /* up to ~10 minutes */
        void *h = dlopen("libil2cpp.so", RTLD_NOLOAD | RTLD_NOW);
        if (h) {
            void *f = dlsym(h, "il2cpp_domain_get");
            if (f) base = (uptr)f - RVA_EXPORT_DOMAIN_GET;
        }
        if (!base) usleep(250 * 1000);
    }
    if (!base) { g_state = 2; g_fail = 0x80; return 0; }

    int fail = 0;
    if (install_hook(base + RVA_PARAMUP, EXP_PARAMUP, (void *)h_ParamUp, (void **)&o_ParamUp)) fail |= 1;
    if (install_hook(base + RVA_FATIGUEUP, EXP_FATIGUEUP, (void *)h_FatigueUp, (void **)&o_FatigueUp)) fail |= 2;
    if (install_hook(base + RVA_GETSTATUS, EXP_GETSTATUS, (void *)h_GetStatus, (void **)&o_GetStatus)) fail |= 4;
    g_fail = fail;
    g_state = fail ? 2 : 1;
    return 0;
}

/* ---------------- JNI (class zzmod.ModMenu, static natives) ---------------- */
EXPORT void Java_zzmod_ModMenu_nStart(void *env, void *cls)
{
    (void)env; (void)cls;
    if (g_started) return;
    g_started = 1;
    unsigned long tid;
    if (pthread_create(&tid, 0, loader, 0) != 0) { g_state = 2; g_fail = 0x100; }
}

EXPORT void Java_zzmod_ModMenu_nSetMult(void *env, void *cls, int v)
{
    (void)env; (void)cls;
    if (v < 1) v = 1;
    if (v > 50) v = 50;
    g_mult = v;
}

EXPORT void Java_zzmod_ModMenu_nSetNoFatigue(void *env, void *cls, int on)
{
    (void)env; (void)cls;
    g_nofat = on ? 1 : 0;
}

/* returns state | (failmask << 4) */
EXPORT int Java_zzmod_ModMenu_nGetState(void *env, void *cls)
{
    (void)env; (void)cls;
    return g_state | (g_fail << 4);
}
