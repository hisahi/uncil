/*******************************************************************************
 
Uncil -- builtin random library impl

Copyright (c) 2021-2023 Sampo Hippeläinen (hisahi)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define UNCIL_DEFINES

#include "udef.h"
#include "ugc.h"
#include "umodule.h"
#include "uncil.h"
#include "uobj.h"
#include "uosdef.h"
#include "uvali.h"

#if UNCIL_C99
#include <stdint.h>
#endif

#if UNCIL_IS_LINUX
#define SAFERNG 1
#include <sys/random.h>
static int saferng(Unc_Size n, byte *b) {
    Unc_Size i = 0;
    while (i < n) i += getrandom(b + i, n - i, 0);
    return 0;
}
#elif UNCIL_IS_MACOS
#define SAFERNG 1
#include <sys/random.h>
static int saferng(Unc_Size n, byte *b) {
    return getentropy(b, n);
}
#elif UNCIL_IS_BSD
#define SAFERNG 1
#include <stdlib.h>
static int saferng(Unc_Size n, byte *b) {
    while (n) {
        uint32_t u = arc4random();
        *b++ = (byte)u;
        if (!--n) break;
        u >>= 8;
        *b++ = (byte)u;
        if (!--n) break;
        u >>= 8;
        *b++ = (byte)u;
        if (!--n) break;
        u >>= 8;
        *b++ = (byte)u;
        --n;
    }
}
#elif 0 && UNCIL_IS_WINDOWS
#error "Not yet implemented"
#define SAFERNG 1
#define SAFERNGINIT 1
static void saferng_doinit(void) {

}

static int saferng(Unc_Size n, byte *b) {

}
#else
#define SAFERNG 0
#endif

#if SAFERNGINIT
static int saferng_init = 0;
#endif

#ifdef UINT64_MAX
struct unc_rng_state {
    uint64_t state;
};
CONSTEXPR uint64_t pcg_mul = UINT64_C(0x5851F42D4C957F2D);
CONSTEXPR uint64_t pcg_add = UINT64_C(0xDD5C0E28D9236311);

static uint64_t unc0_stdrandseed(void) {
    uint64_t u;
    byte buf[sizeof(u)];
#if SAFERNG
    if (saferng(sizeof(buf), buf))
#endif
    {
        int i;
        srand((unsigned)time(NULL));
        for (i = 0; i < sizeof(buf); ++i)
            buf[i] = (byte)(rand() >> 5);
    }
    memcpy(&u, buf, sizeof(u));
    return u;
}

static void unc0_stdrand(struct unc_rng_state *data, Unc_Size n, byte *b) {
    /* based on O'Neill 2014: PCG: A Family of Simple Fast Space-Efficient
       Statistically Good Algorithms for Random Number Generation */
    uint64_t st = data->state, x;
    unsigned count;
    uint32_t r;
    while (n) {
        x = st;
        count = (unsigned)(x >> 59);
        st = x * pcg_mul + pcg_add;
        x ^= x >> 18;
        r = (uint32_t)(x >> 27);
        r = (r >> count) | (r << (32 - count));
        *b++ = (byte)r;
        if (!--n) break;
        r >>= 8;
        *b++ = (byte)r;
        if (!--n) break;
        r >>= 8;
        *b++ = (byte)r;
        if (!--n) break;
        r >>= 8;
        *b++ = (byte)r;
        --n;
    }
    data->state = st;
}

static void unc0_stdrandinit(struct unc_rng_state *data) {
    byte b;
    data->state = pcg_add + unc0_stdrandseed();
    (void)unc0_stdrand(data, 1, &b);
}
#else
#include <stdlib.h>
#include <time.h>

struct unc_rng_state {
    char _;
};

static void unc0_stdrand(void *data, Unc_Size n, byte *b) {
    Unc_Size i;
    (void)data;
    for (i = 0; i < n; ++i)
        b[i] = (byte)(rand() >> (3 + (2 ^ (i & 3))));
}

static void unc0_stdrandinit(struct unc_rng_state *data) {
    (void)data;
    srand((unsigned)time(NULL));
}
#endif

static Unc_RetVal unc_randwrapper(Unc_View *w, Unc_Value fn,
                                  Unc_Int n, byte *b, void *data) {
    if (!fn.type) {
        unc0_stdrand(data, n, b);
        return 0;
    }
    if (unc_iscallable(w, &fn)) {
        Unc_RetVal e;
        Unc_Pile ret;
        Unc_Value v = UNC_BLANK;
        unc_setint(w, &v, n);
        if ((e = unc_push(w, 1, &v)))
            return e;
        e = unc_call(w, &fn, 1, &ret);
        if (e) {
            return e;
        } else {
            Unc_Tuple tuple;
            size_t pn;
            byte *pb;
            unc_returnvalues(w, &ret, &tuple);
            if (tuple.count != 1) {
                unc_discard(w, &ret);
                return unc_throwexc(w, "type", "random source should return "
                                               "a single blob");
            }
            if (unc_lockblob(w, &tuple.values[0], &pn, &pb)) {
                unc_discard(w, &ret);
                return unc_throwexc(w, "type", "random source should return "
                                               "a single blob");
            }
            if (pn != n) {
                unc_unlock(w, &tuple.values[0]);
                unc_discard(w, &ret);
                return unc_throwexc(w, "value", "random source should return "
                                                "a single blob of the "
                                                "requested size");
            }
            unc0_memcpy(b, pb, n);
            unc_unlock(w, &tuple.values[0]);
            unc_discard(w, &ret);
            return 0;
        }
    }
    return unc_throwexc(w, "type", "random source must be callable");
}

#define INV_UINT_MAX (1 / ((Unc_Float)(UNC_UINT_MAX / 2 + 1)))
static Unc_RetVal unc_randwrapper_float(Unc_View *w, Unc_Value fn,
                                 struct unc_rng_state *rng, Unc_Float *f) {
    Unc_UInt u;
    byte b[sizeof(Unc_UInt)];
    Unc_RetVal e = unc_randwrapper(w, fn, sizeof(b), b, rng);

    if (e) return e;
    unc0_memcpy(&u, b, sizeof(Unc_UInt));
    u /= 2;
    *f = u * INV_UINT_MAX;
    return 0;
}

Unc_RetVal uncl_rand_random(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;
    Unc_RetVal e;
    struct unc_rng_state *rng;
    (void)udata;
    
    e = unc_lockopaque(w, unc_boundvalue(w, 0), NULL, (void **)&rng);
    if (e) return e;
    e = unc_randwrapper_float(w, args.values[0], rng, &uf);
    unc_unlock(w, unc_boundvalue(w, 0));
    if (e) return e;
    unc_setfloat(w, &v, uf);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_rand_randomint(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_Float uf;
    Unc_Int i0, i1;
    Unc_RetVal e;
    struct unc_rng_state *rng;
    (void)udata;
    
    e = unc_getint(w, &args.values[0], &i0);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &i1);
    if (e) return e;
    if (i0 >= i1) return unc_throwexc(w, "value", "a must be less than b");
    
    e = unc_lockopaque(w, unc_boundvalue(w, 0), NULL, (void **)&rng);
    if (e) return e;
    e = unc_randwrapper_float(w, args.values[2], rng, &uf);
    unc_unlock(w, unc_boundvalue(w, 0));
    if (e) return e;
    unc_setint(w, &v, i0 + (i1 - i0) * uf);
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_rand_randombytes(Unc_View *w, Unc_Tuple args,
                                     void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    byte *b;
    Unc_Int i;
    struct unc_rng_state *rng;

    e = unc_lockopaque(w, unc_boundvalue(w, 0), NULL, (void **)&rng);
    if (e) return e;
    e = unc_getint(w, &args.values[0], &i);
    if (e) goto fail;
    if (i < 0) {
        e = unc_throwexc(w, "value",
            "cannot request a negative number of bytes");
        goto fail;
    }
    e = unc_newblob(w, &v, i, &b);
    if (e) goto fail;
    unc0_stdrand(rng, i, b);
    unc_unlock(w, &v);
    e = unc_returnlocal(w, 0, &v);
fail:
    unc_unlock(w, unc_boundvalue(w, 0));
    return e;
}

Unc_RetVal uncl_rand_securerandom(Unc_View *w, Unc_Tuple args,
                                      void *udata) {
#if SAFERNG
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    byte *b;
    Unc_Int i;

    e = unc_getint(w, &args.values[0], &i);
    if (e) return e;
    if (i < 0)
        return unc_throwexc(w, "value", 
            "cannot request a negative number of bytes");
    e = unc_newblob(w, &v, i, &b);
    if (e) return e;
    e = saferng(i, b);
    if (e) {
        VCLEAR(w, &v);
        return unc_throwexc(w, "system", "secure RNG returned an error");
    }

    unc_unlock(w, &v);
    return unc_returnlocal(w, 0, &v);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal uncl_rand_shuffle(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value *av;
    Unc_Size a, an, aj;
    Unc_Float uf;
    Unc_RetVal e;
    struct unc_rng_state *rng;
    (void)udata;
    
    e = unc_lockarray(w, &args.values[0], &an, &av);
    if (e) return e;
    e = unc_lockopaque(w, unc_boundvalue(w, 0), NULL, (void **)&rng);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }

    e = 0;
    for (a = 0; a < an - 1; ++a) {
        e = unc_randwrapper_float(w, args.values[1], rng, &uf);
        if (e) break;
        aj = a + (Unc_Size)(uf * (an - a));
        if (a != aj) {
            Unc_Value tmp = av[a];
            av[a] = av[aj];
            av[aj] = tmp;
        }
    }

    unc_unlock(w, unc_boundvalue(w, 0));
    unc_unlock(w, &args.values[0]);
    return e;
}

#define FN(x) &uncl_rand_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(random),        0, 1, 0, UNC_CFUNC_DEFAULT },
    { FN(randomint),     2, 1, 0, UNC_CFUNC_DEFAULT },
    { FN(randombytes),   1, 0, 0, UNC_CFUNC_DEFAULT },
    { FN(shuffle),       1, 1, 0, UNC_CFUNC_DEFAULT },
};

Unc_RetVal uncilmain_random(Unc_View *w) {
    Unc_RetVal e;
    Unc_Value rng = UNC_BLANK;
    struct unc_rng_state *rngs;

#if SAFERNGINIT
    if (!saferng_init) {
        saferng_doinit();
        saferng_init = 1;
    }
#endif
    e = unc_exportcfunction(w, "securerandom", &uncl_rand_securerandom,
                            1, 0, 0, UNC_CFUNC_DEFAULT, NULL,
                            0, NULL, 0, NULL, NULL);
    if (e) return e;

    e = unc_newopaque(w, &rng, NULL,
                      sizeof(struct unc_rng_state), (void **)&rngs,
                      NULL, 0, NULL, 0, NULL);
    if (e) return e;
    unc0_stdrandinit(rngs);
    e = unc_exportcfunctions(w, PASSARRAY(lib), 1, &rng, 0);
    unc_unlock(w, &rng);
    VCLEAR(w, &rng);
    return e;
}
