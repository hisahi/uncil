/*******************************************************************************
 
Uncil -- builtin CBOR library impl

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

#include <limits.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "udebug.h"
#include "umem.h"
#include "uncil.h"
#include "uvsio.h"

#if UNCIL_SANDBOXED
#define UNCIL_NOLIBIO 1
#endif

#if !UNCIL_NOLIBIO
#include "ulibio.h"
#endif

#if UNCIL_C99
#include <stdint.h>
typedef uint_least32_t u32_t;
#define U32_C(x) UINT32_C(x)
#elif UINT_MAX / 65537UL >= 65535UL
typedef unsigned int u32_t;
#define U32_C(x) x##U
#else
typedef unsigned long u32_t;
#define U32_C(x) x##UL
#endif

#if UNCIL_C99
#include <stdint.h>
typedef uint_least64_t u64_t;
#define U64_C(x) UINT64_C(x)
#elif (ULONG_MAX / 6700417UL) / 65537UL >= 42007935UL
typedef unsigned long u64_t;
#define U64_C(x) x##UL
#else
typedef struct s_u64_ {
    u32_t l, h;
} u64_t;
#define U64SYNTH 1
#define U64_C(x) { x & U32_C(0xFFFFFFFF), x >> 32 }
#endif

#if U64SYNTH
static u64_t unc0_u64_0(void) {
    u64_t u = { 0 };
    return u;
}

#define U64_0 unc0_u64_0()
#define U64SETL(u, v) (u.l = (v), u.h = 0)
#else
#define U64_0 0
#define U64SETL(u, v) u = (v)
#endif

#define CBOR_EOL 1

struct cbor_decode_blob {
    Unc_Size n;
    Unc_Size r;
    byte *c;
};

/* -1 = eof, -2 = error, etc. */
static int cbor_decode_blob_do(void *p) {
    struct cbor_decode_blob *s = p;
    if (!s->n) return -1;
    --s->n;
    ++s->r;
    return *s->c++;
}

struct cbor_decode_context {
    int (*getch)(void *);
    void *getch_data;
    Unc_Size recurse;
    Unc_View *view;
    Unc_Value *tagproto;
    int fle;
};

#if !UNCIL_NOLIBIO
struct cbor_decode_file {
    struct ulib_io_file *fp;
    int err;
};

static int cbor_decode_file_do(void *p_) {
    struct cbor_decode_file *p = p_;
    int c;
    if (p->err) return p->err;
    c = unc0_io_fgetc(p->fp);
    if (c < 0) {
        p->err = -1;
        return unc0_io_ferror(p->fp) ? -2 : -1;
    }
    return c;
}
#endif

#define READCHAR(c) (*c->getch)(c->getch_data)

INLINE Unc_RetVal cbordec_err(struct cbor_decode_context *c,
                              const char *type, const char *msg) {
    return unc_throwexc(c->view, type, msg);
}

#if U64SYNTH
INLINE Unc_RetVal cbordec_len32(struct cbor_decode_context *c,
                                int z, u32_t *result) {
    int x;
    u32_t u = 0;
    ASSERT(z == 1 || z == 2 || z == 4);
    do {
        u <<= 8;
        x = READCHAR(c);
        if (x < 0)
            return cbordec_err(c, "value",
                "unexpected EOF when decoding integral CBOR value");
        u += x;
    } while (--z);
    *result = u;
    return 0;
}

static Unc_RetVal cbordec_len(struct cbor_decode_context *c,
                              int z, u64_t *result) {
    ASSERT(z == 1 || z == 2 || z == 4 || z == 8);
    if (z > 4) {
        Unc_RetVal e = cbordec_len32(c, 4, &result->h);
        if (e) return e;
        z -= 4;
    } else
        result->h = 0;
    return cbordec_len32(c, z, &result->l);
}
#else
static Unc_RetVal cbordec_len(struct cbor_decode_context *c,
                              int z, u64_t *result) {
    int x;
    u64_t u = 0;
    ASSERT(z == 1 || z == 2 || z == 4 || z == 8);
    do {
        u <<= 8;
        x = READCHAR(c);
        if (x < 0)
            return cbordec_err(c, "value",
                "unexpected EOF when decoding integral CBOR value");
        u += x;
    } while (--z);
    *result = u;
    return 0;
}
#endif

INLINE Unc_RetVal cbordec_intp(struct cbor_decode_context *c, Unc_Value *v,
                               int z) {
    u64_t u;
    Unc_RetVal e = cbordec_len(c, z, &u);
    if (e) return e;
#if U64SYNTH
    if (u.h > (UNC_INT_MAX >> 32) || (u.h == (UNC_INT_MAX >> 32)
                                   && u.l >= (u32_t)UNC_INT_MAX))
#else
    if (u > UNC_INT_MAX)
#endif
        return cbordec_err(c, "value",
            "CBOR integer too large to represent as Uncil integer");
#if U64SYNTH
    unc_setint(c->view, v, (Unc_Int)(((Unc_UInt)u.h << 32) | u.l));
#else
    unc_setint(c->view, v, u);
#endif
    return 0;
}

INLINE Unc_RetVal cbordec_intn(struct cbor_decode_context *c, Unc_Value *v,
                               int z) {
    u64_t u;
    Unc_RetVal e = cbordec_len(c, z, &u);
    if (e) return e;
#if U64SYNTH
    if (u.h > ((u32_t)-UNC_INT_MIN >> 32)
                                  || (u.h == ((u32_t)-UNC_INT_MIN >> 32)
                                   && u.l >= (u32_t)-UNC_INT_MIN))
#else
    if (u >= -(Unc_UInt)UNC_INT_MIN)
#endif
        return cbordec_err(c, "value",
            "CBOR integer too large to represent as Uncil integer");
#if U64SYNTH
    unc_setint(c->view, v, -(Unc_Int)(((Unc_UInt)u.h << 32) | u.l) - 1);
#else
    unc_setint(c->view, v, -(Unc_Int)u - 1);
#endif
    return 0;
}

static Unc_RetVal cbordec(struct cbor_decode_context *c, Unc_Value *v);

INLINE Unc_RetVal cbordec_blob(struct cbor_decode_context *c, Unc_Value *v,
                               u64_t l) {
    Unc_RetVal e;
    byte *p;
#if U64SYNTH
    u32_t i, z;
    if (l.h) return cbordec_err(c, "system", "CBOR blob too large");
    z = l.l;
#else
    u64_t i, z;
    z = l;
#endif
    e = unc_newblob(c->view, v, z, &p);
    if (e) return e;
    for (i = 0; i < z; ++i) {
        int x = READCHAR(c);
        if (x < 0) {
            e = cbordec_err(c, "value", "CBOR blob unexpected EOF");
            unc_unlock(c->view, v);
            return e;
        }
        *p++ = (byte)x;
    }
    unc_unlock(c->view, v);
    return e;
}

INLINE Unc_RetVal cbordec_blobchnk(struct cbor_decode_context *c,
                                   Unc_Value *v) {
    byte *p;
    Unc_Size z = 0, nz;
    Unc_RetVal e = unc_newblob(c->view, v, z, &p);
    if (e) return e;
    for (;;) {
        int x = READCHAR(c);
        u64_t ulen = U64_0;
        if (x < 0) {
            e = cbordec_err(c, "value", "CBOR blob chain unexpected EOF");
            break;
        }
        if (x == 0xFF) {
            e = 0;
            break;
        }
        if ((x >> 5) != 2) {
            e = cbordec_err(c, "value", "CBOR blob chain: expected blob, "
                                        "but value was of another type");
            break;
        }
        x &= 31;
        if (x >= 28) {
            e = cbordec_err(c, "value",
                "CBOR blob chain: invalid length for blob in chain");
            break;
        }
        if (x >= 24) {
            e = cbordec_len(c, 1 << (x - 24), &ulen);
            if (e) break;
        } else
            U64SETL(ulen, x);
#if U64SYNTH
        if (ulen.h) {
            e = cbordec_err(c, "system", "CBOR blob too large");
            break;
        }
        nz = z + ulen.l;
#else
        nz = z + ulen;
#endif
        if (nz < z) {
            e = cbordec_err(c, "system", "CBOR blob too large");
            break;
        }
        if (z == nz) continue;
        e = unc_resizeblob(c->view, v, nz, &p);
        if (e) break;
        while (z < nz) {
            x = READCHAR(c);
            if (x < 0) {
                e = cbordec_err(c, "value", "CBOR blob unexpected EOF");
                break;
            }
            p[z++] = x;
        }
    }
    unc_unlock(c->view, v);
    return e;
}

INLINE Unc_RetVal cbordec_str(struct cbor_decode_context *c, Unc_Value *v,
                              u64_t l) {
    Unc_RetVal e;
    byte *p, *pp;
#if U64SYNTH
    u32_t i, z;
    if (l.h) return cbordec_err(c, "system", "CBOR string too large");
    z = l.l;
#else
    u64_t i, z;
    z = l;
#endif
    p = pp = unc_malloc(c->view, z);
    if (!p) return UNCIL_ERR_MEM;
    for (i = 0; i < z; ++i) {
        int x = READCHAR(c);
        if (x < 0) {
            e = cbordec_err(c, "value", "CBOR string unexpected EOF");
            unc_unlock(c->view, v);
            return e;
        }
        *p++ = (byte)x;
    }
    e = unc_newstringmove(c->view, v, z, (char *)pp);
    if (e) unc_mfree(c->view, pp);
    return e;
}

INLINE Unc_RetVal cbordec_strchnk(struct cbor_decode_context *c,
                                  Unc_Value *v) {
    byte *p;
    Unc_Size z = 0, nz;
    Unc_RetVal e;
    p = unc_malloc(c->view, z);
    if (!p) return UNCIL_ERR_MEM;
    for (;;) {
        int x = READCHAR(c);
        u64_t ulen = U64_0;
        if (x < 0) {
            e = cbordec_err(c, "value", "CBOR string chain unexpected EOF");
            break;
        }
        if (x == 0xFF) {
            e = unc_newstringmove(c->view, v, z, (char *)p);
            if (!e) return e;
            break;
        }
        if ((x >> 5) != 3) {
            e = cbordec_err(c, "value", "CBOR string chain: expected string, "
                                        "but value was of another type");
            break;
        }
        x &= 31;
        if (x >= 28) {
            e = cbordec_err(c, "value",
                "CBOR string chain: invalid length for string in chain");
            break;
        }
        if (x >= 24) {
            Unc_RetVal e = cbordec_len(c, 1 << (x - 24), &ulen);
            if (e) break;
        } else
            U64SETL(ulen, x);
#if U64SYNTH
        if (ulen.h) {
            e = cbordec_err(c, "system", "CBOR blob too large");
            break;
        }
        nz = z + ulen.l;
#else
        nz = z + ulen;
#endif
        if (nz < z) {
            e = cbordec_err(c, "system", "CBOR blob too large");
            break;
        }
        if (z == nz) continue;
        p = unc_mrealloc(c->view, p, nz);
        if (!p) {
            e = UNCIL_ERR_MEM;
            break;
        }
        while (z < nz) {
            x = READCHAR(c);
            if (x < 0) {
                e = cbordec_err(c, "value", "CBOR string unexpected EOF");
                break;
            }
            p[z++] = x;
        }
    }
    unc_mfree(c->view, p);
    return e;
}

INLINE Unc_RetVal cbordec_arr(struct cbor_decode_context *c, Unc_Value *v,
                              int forever, u64_t l) {
    Unc_RetVal e;
    Unc_Size z0, z1, j = 0;
    Unc_Value *p;
#if U64SYNTH
    u32_t i, z;
    if (l.h) return cbordec_err(c, "system", "CBOR string too large");
    z = l.l;
#else
    u64_t i, z;
    z = l;
#endif
    z0 = forever ? 0 : z;
    z1 = z0;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = unc_newarray(c->view, v, z0, &p);
    if (e) return e;
    e = 0;
    for (i = 0; forever || i < z; ++i) {
        if (j >= z0) {
            z1 = z0 + 32;
            if (z0 > z1) {
                e = cbordec_err(c, "system", "CBOR array too large");
                break;
            }
            e = unc_resizearray(c->view, v, z1, &p);
            if (e)
                break;
            z0 = z1;
        }
        e = cbordec(c, &p[j]);
        if (e) {
            if (e == CBOR_EOL) {
                if (!forever)
                    e = cbordec_err(c, "value",
                        "CBOR unexpected terminator in definite length array");
                else {
                    e = unc_resizearray(c->view, v, j, &p);
                }
            }
            break;
        }
        ++j;
    }
    unc_unlock(c->view, v);
    ++c->recurse;
    return e;
}

INLINE Unc_RetVal cbordec_map(struct cbor_decode_context *c, Unc_Value *v,
                              int forever, u64_t l) {
    Unc_RetVal e;
    Unc_Value pk = UNC_BLANK, pv = UNC_BLANK;
#if U64SYNTH
    u32_t i, z;
    if (l.h) return cbordec_err(c, "system", "CBOR string too large");
    z = l.l;
#else
    u64_t i, z;
    z = l;
#endif
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = unc_newtable(c->view, v);
    if (e) return e;
    e = 0;
    for (i = 0; forever || i < z; ++i) {
        e = cbordec(c, &pk);
        if (e) {
            if (e == CBOR_EOL)
                e = 0;
            break;
        }
        e = cbordec(c, &pv);
        if (e) {
            if (e == CBOR_EOL)
                e = cbordec_err(c, "value",
                    "CBOR map unexpected terminator after key");
            break;
        }
        e = unc_setindex(c->view, v, &pk, &pv);
        if (e)
            break;
        VCLEAR(c->view, &pv);
        VCLEAR(c->view, &pk);
    }
    ++c->recurse;
    VCLEAR(c->view, &pv);
    VCLEAR(c->view, &pk);
    return e;
}

static Unc_RetVal cbordec_tag(struct cbor_decode_context *c, Unc_Value *v,
                              u64_t tag) {
    Unc_Value tmp = UNC_BLANK;
    Unc_RetVal e;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = cbordec(c, &tmp);
    ++c->recurse;
    if (e) return e;
    e = unc_newobject(c->view, v, c->tagproto);
    if (e) return e;
    e = unc_setattrc(c->view, v, "data", &tmp);
    if (e) return e;
#if U64SYNTH
    if (tag.h > 0 || tag.l > (Unc_UInt)UNC_INT_MAX)
        return cbordec_err(c, "value",
            "CBOR integer too large to represent as Uncil integer");
    unc_setint(c->view, v, (Unc_Int)tag.l);
#else
    unc_setint(c->view, v, (Unc_Int)tag);
#endif
    return unc_setattrc(c->view, v, "tag", &tmp);
}

INLINE Unc_RetVal cbordec_f16(struct cbor_decode_context *c, Unc_Value *v) {
    int x, neg;
    unsigned int u;
    Unc_Float f;
    x = READCHAR(c);
    if (x < 0)
        return cbordec_err(c, "value", "CBOR float16 unexpected EOF");
    u = (unsigned)x << 8;
    x = READCHAR(c);
    if (x < 0)
        return cbordec_err(c, "value", "CBOR float16 unexpected EOF");
    u |= x;
    neg = u >> 15;
    x = (u >> 10) & 31;
    u &= 0x3FF;
    if (x == 0)
        x = 1;
    else if (x == 31)
        f = u ? unc0_fnan() : unc0_finfty();
    else
        f = unc0_mldexp(u | 0x400, x - 25);
    if (neg)
        f = -f;
    unc_setfloat(c->view, v, f);
    return 0;
}

INLINE Unc_RetVal cbordec_f32(struct cbor_decode_context *c, Unc_Value *v) {
    int i, fle = c->fle;
    float f32;
    byte b[sizeof(float)];
    ASSERT(sizeof(float) == 4);
    for (i = 0; i < sizeof(b); ++i) {
        int x = READCHAR(c);
        if (x < 0)
            return cbordec_err(c, "value", "CBOR float32 unexpected EOF");
        b[fle ? sizeof(b) - 1 - i : i] = (byte)x;
    }
    unc0_memcpy(&f32, b, sizeof(b));
    unc_setfloat(c->view, v, f32);
    return 0;
}

INLINE Unc_RetVal cbordec_f64(struct cbor_decode_context *c, Unc_Value *v) {
    int i, fle = c->fle;
    double f64;
    byte b[sizeof(double)];
    ASSERT(sizeof(double) == 8);
    for (i = 0; i < sizeof(b); ++i) {
        int x = READCHAR(c);
        if (x < 0)
            return cbordec_err(c, "value", "CBOR float64 unexpected EOF");
        b[fle ? sizeof(b) - 1 - i : i] = (byte)x;
    }
    unc0_memcpy(&f64, b, sizeof(b));
    unc_setfloat(c->view, v, f64);
    return 0;
}

static Unc_RetVal cbordec(struct cbor_decode_context *c, Unc_Value *v) {
    int x = READCHAR(c);
    int type, len;
    u64_t ulen = U64_0;
    
    if (x < 0)
        return cbordec_err(c, "value",
            "unexpected EOF when decoding CBOR item");
    type = x >> 5, len = x & ((1 << 5) - 1);
    switch (type) {
    case 0:
        if (len < 24) {
            unc_setint(c->view, v, len);
            return 0;
        } else if (len >= 28) {
            return cbordec_err(c, "value",
                "invalid CBOR integer length (must be < 28)");
        }
        return cbordec_intp(c, v, 1 << (len - 24));
    case 1:
        if (len < 24) {
            unc_setint(c->view, v, -len - 1);
            return 0;
        } else if (len >= 28) {
            return cbordec_err(c, "value",
                "invalid CBOR integer length (must be < 28)");
        }
        return cbordec_intn(c, v, 1 << (len - 24));
    case 2:
        if (len >= 24) {
            if (len == 31) {
                return cbordec_blobchnk(c, v);
            } else if (len < 28) {
                Unc_RetVal e = cbordec_len(c, 1 << (len - 24), &ulen);
                if (e) return e;
            } else {
                return cbordec_err(c, "value",
                    "invalid CBOR length (reserved 28-30)");
            }
        } else
            U64SETL(ulen, len);
        return cbordec_blob(c, v, ulen);
    case 3:
        if (len >= 24) {
            if (len == 31) {
                return cbordec_strchnk(c, v);
            } else if (len < 28) {
                Unc_RetVal e = cbordec_len(c, 1 << (len - 24), &ulen);
                if (e) return e;
            } else {
                return cbordec_err(c, "value",
                    "invalid CBOR length (reserved 28-30)");
            }
        } else
            U64SETL(ulen, len);
        return cbordec_str(c, v, ulen);
    case 4:
        if (len >= 24) {
            if (len == 31) {
                return cbordec_arr(c, v, 1, U64_0);
            } else if (len < 28) {
                Unc_RetVal e = cbordec_len(c, 1 << (len - 24), &ulen);
                if (e) return e;
            } else {
                return cbordec_err(c, "value",
                    "invalid CBOR length (reserved 28-30)");
            }
        } else
            U64SETL(ulen, len);
        return cbordec_arr(c, v, 0, ulen);
    case 5:
        if (len >= 24) {
            if (len == 31) {
                return cbordec_map(c, v, 1, U64_0);
            } else if (len < 28) {
                Unc_RetVal e = cbordec_len(c, 1 << (len - 24), &ulen);
                if (e) return e;
            } else {
                return cbordec_err(c, "value",
                    "invalid CBOR length (reserved 28-30)");
            }
        } else
            U64SETL(ulen, len);
        return cbordec_map(c, v, 0, ulen);
    case 6:
        if (len >= 24) {
            if (len < 28) {
                Unc_RetVal e = cbordec_len(c, 1 << (len - 24), &ulen);
                if (e) return e;
            } else {
                return cbordec_err(c, "value",
                    "invalid CBOR tag length (reserved 28-30 or 31)");
            }
        } else
            U64SETL(ulen, len);
        return cbordec_tag(c, v, ulen);
    case 7:
    default:
        if (len < 20)
            return cbordec_err(c, "value",
                "invalid CBOR val=7 length (reserved 0-19)");
        else if (len == 20) {
            unc_setbool(c->view, v, 0);
            return 0;
        } else if (len == 21) {
            unc_setbool(c->view, v, 1);
            return 0;
        } else if (len == 22) {
            unc_setnull(c->view, v);
            return 0;
        } else if (len == 23) { /* undefined */
            unc_setnull(c->view, v);
            return 0;
        } else if (len == 24)
            return cbordec_err(c, "value",
                "invalid CBOR val=7 length (reserved 24)");
        else if (len == 25)
            return cbordec_f16(c, v);
        else if (len == 26)
            return cbordec_f32(c, v);
        else if (len == 27)
            return cbordec_f64(c, v);
        else if (len < 31)
            return cbordec_err(c, "value",
                "invalid CBOR val=7 length (reserved 28-30)");
        else
            return CBOR_EOL;
    }
}

struct cbor_encode_blob {
    struct unc0_strbuf buf;
    int err;
};

/* 0 = OK, <>0 = error */
static int cbor_encode_blob_do(Unc_Size n, const byte *c, void *p) {
    struct cbor_encode_blob *s = p;
    return (s->err = unc0_strbuf_putn(&s->buf, n, c));
}

#if !UNCIL_NOLIBIO
struct cbor_encode_file {
    struct ulib_io_file *fp;
    int err;
    Unc_View *view;
};

static int cbor_encode_file_do(Unc_Size n, const byte *c, void *p_) {
    struct cbor_encode_file *p = p_;
    return (p->err = unc0_io_fwrite_p(p->view, p->fp, (const byte *)c, n));
}
#endif

struct cbor_encode_context {
    int (*out)(Unc_Size, const byte *, void *);
    void *out_data;
    Unc_Size recurse;
    Unc_View *view;
    Unc_Value *tagproto;
    Unc_Value mapper;
    int fle;
    void **ents;
    Unc_Size ents_n, ents_c;
};

INLINE Unc_RetVal cborenc_err(struct cbor_encode_context *c,
                              const char *type, const char *msg) {
    return unc_throwexc(c->view, type, msg);
}

#if UNCIL_C99
typedef uint_least64_t Unc_CBORInt;
#elif UNC_UINT_BIT >= 64
typedef Unc_UInt Unc_CBORInt;
#else
typedef Unc_Size Unc_CBORInt;
#endif

INLINE Unc_RetVal cborenc_val(struct cbor_encode_context *c,
                              int type, Unc_CBORInt value) {
    int j;
    if (value < 24) {
        byte x = (type << 5) | value;
        return (*c->out)(sizeof(x), &x, c->out_data);
    } else if (value < 256) {
        byte x[2];
        x[0] = (type << 5) | 24;
        x[1] = value;
        return (*c->out)(sizeof(x), x, c->out_data);
    } else if (value < 65536) {
        byte x[3];
        x[0] = (type << 5) | 25;
        for (j = 1; j >= 0; --j)
            x[sizeof(x) - j - 1] = (value >> (j << 3)) & 0xFF;
        return (*c->out)(sizeof(x), x, c->out_data);
    } else {
        Unc_Size v16 = value >> 16;
        v16 >>= 16;
        if (!v16) {
            /* fits in 32 bits */
            byte x[5];
            x[0] = (type << 5) | 26;
            for (j = 3; j >= 0; --j)
                x[sizeof(x) - j - 1] = (value >> (j << 3)) & 0xFF;
            return (*c->out)(sizeof(x), x, c->out_data);
        }
        v16 >>= 16;
        v16 >>= 16;
        if (!v16) {
            /* fits in 64 bits */
            byte x[9];
            x[0] = (type << 5) | 27;
            for (j = 7; j >= 0; --j)
                x[sizeof(x) - j - 1] = (value >> (j << 3)) & 0xFF;
            return (*c->out)(sizeof(x), x, c->out_data);
        }
        return cborenc_err(c, "value", "CBOR value too large to encode");
    }
}

INLINE Unc_RetVal cborenc_raw(struct cbor_encode_context *c,
                              int type, int value) {
    byte x = (type << 5) | value;
    return (*c->out)(1, &x, c->out_data);
}

INLINE Unc_RetVal cborenc_indef(struct cbor_encode_context *c, int type) {
    return cborenc_raw(c, type, 31);
}

INLINE Unc_RetVal cborenc_eol(struct cbor_encode_context *c) {
    return cborenc_indef(c, 7);
}

static Unc_RetVal cborenc_signent(struct cbor_encode_context *c, void *p) {
    Unc_Size i = c->ents_n;
    if (i) {
        --i;
        do {
            if (c->ents[i] == p)
                return unc_throwexc(c->view, "value", "loop detected");
        } while (i--);
    }
    if (c->ents_n == c->ents_c) {
        Unc_Size entsz = c->ents_c + 16;
        void **ents2 = unc_mrealloc(c->view, c->ents, entsz * sizeof(void *));
        if (!ents2) return UNCIL_ERR_MEM;
        c->ents = ents2;
        c->ents_c = entsz;
    }
    c->ents[c->ents_n++] = p;
    return 0;
}

static Unc_RetVal cborenc(struct cbor_encode_context *c, Unc_Value *v);

INLINE Unc_RetVal cborenc_int(struct cbor_encode_context *c, Unc_Int i) {
    if (i < 0) {
        return cborenc_val(c, 1, (Unc_CBORInt)(Unc_UInt)-i);
    } else {
        return cborenc_val(c, 0, (Unc_CBORInt)i);
    }
}

INLINE Unc_RetVal cborenc_f32(struct cbor_encode_context *c, float f) {
    Unc_RetVal e;
    int fle = c->fle;
    byte b[sizeof(float)];
    if ((e = cborenc_raw(c, 7, 26))) return e;
    ASSERT(sizeof(float) == 4);
    unc0_memcpy(b, &f, sizeof(b));
    if (fle) unc0_memrev(b, sizeof(b));
    return (*c->out)(sizeof(b), b, c->out_data);
}

INLINE Unc_RetVal cborenc_f64(struct cbor_encode_context *c, double f) {
    Unc_RetVal e;
    int fle = c->fle;
    byte b[sizeof(double)];
    if (f == (float)f) return cborenc_f32(c, (float)f);
    if ((e = cborenc_raw(c, 7, 27))) return e;
    ASSERT(sizeof(double) == 8);
    unc0_memcpy(b, &f, sizeof(b));
    if (fle) unc0_memrev(b, sizeof(b));
    return (*c->out)(sizeof(b), b, c->out_data);
}

INLINE Unc_RetVal cborenc_blob(struct cbor_encode_context *c, Unc_Value *v) {
    Unc_Size sn;
    byte *sp;
    Unc_RetVal e;
    e = unc_lockblob(c->view, v, &sn, &sp);
    if (e) return e;
    if ((e = cborenc_val(c, 2, sn))) goto cborenc_blob_done;
    e = (*c->out)(sn, sp, c->out_data);
cborenc_blob_done:
    unc_unlock(c->view, v);
    return e;
}

INLINE Unc_RetVal cborenc_str(struct cbor_encode_context *c, Unc_Value *v) {
    Unc_Size sn;
    const char *sp;
    Unc_RetVal e;
    e = unc_getstring(c->view, v, &sn, &sp);
    if (e) return e;
    if ((e = cborenc_val(c, 3, sn))) return e;
    return (*c->out)(sn, (const byte *)sp, c->out_data);
}

INLINE Unc_RetVal cborenc_arr(struct cbor_encode_context *c, Unc_Value *v) {
    Unc_Size sn;
    Unc_Value *ss;
    Unc_RetVal e;
    if ((e = cborenc_signent(c, VGETENT(v))))
        return e;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = unc_lockarray(c->view, v, &sn, &ss);
    if (e) return e;
    if ((e = cborenc_val(c, 4, sn))) goto cborenc_arr_done;
    if (sn) {
        Unc_Size i;
        for (i = 0; i < sn; ++i) {
            if ((e = cborenc(c, &ss[i]))) goto cborenc_arr_done;
        }
    }
    ++c->recurse;
    --c->ents_n;
cborenc_arr_done:
    unc_unlock(c->view, v);
    return e;
}

INLINE Unc_RetVal cborenc_obj(struct cbor_encode_context *c, Unc_Value *v) {
    Unc_RetVal e;
    Unc_Value iter = UNC_BLANK;
    if ((e = cborenc_signent(c, VGETENT(v))))
        return e;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    e = unc_getiterator(c->view, v, &iter);
    if (e) return e;
    if ((e = cborenc_indef(c, 5))) goto cborenc_obj_done;
    if (unc_converttobool(c->view, v) == 1) {
        for (;;) {
            Unc_Pile pile;
            Unc_Tuple tuple;
            e = unc_call(c->view, &iter, 0, &pile);
            if (e) goto cborenc_obj_done;
            unc_returnvalues(c->view, &pile, &tuple);
            if (!tuple.count) {
                unc_discard(c->view, &pile);
                break;
            }
            if (tuple.count != 2) {
                e = UNCIL_ERR_INTERNAL;
                unc_discard(c->view, &pile);
                goto cborenc_obj_done;
            }
            if ((e = cborenc(c, &tuple.values[0]))) goto cborenc_obj_done;
            if ((e = cborenc(c, &tuple.values[1]))) goto cborenc_obj_done;
            unc_discard(c->view, &pile);
        }
    }
    e = cborenc_eol(c);
    ++c->recurse;
    --c->ents_n;
cborenc_obj_done:
    VCLEAR(c->view, &iter);
    return e;
}

INLINE Unc_RetVal cborenc_tag(struct cbor_encode_context *c, Unc_Value *v) {
    Unc_RetVal e;
    Unc_Int ik;
    Unc_Value iv = UNC_BLANK;
    if (!c->recurse)
        return UNCIL_ERR_TOODEEP;
    --c->recurse;
    if ((e = unc_getattrc(c->view, v, "tag", &iv))) return e;
    if ((e = unc_getint(c->view, &iv, &ik))) goto cborenc_tag_done;
    if ((e = unc_getattrc(c->view, v, "data", &iv))) goto cborenc_tag_done;
    if ((e = cborenc_val(c, 6, (Unc_CBORInt)(Unc_UInt)ik)))
        goto cborenc_tag_done;
    e = cborenc(c, &iv);
    ++c->recurse;
cborenc_tag_done:
    VCLEAR(c->view, &iv);
    return e;
}

static Unc_RetVal cborenc(struct cbor_encode_context *c, Unc_Value *v) {
    Unc_RetVal e;
    switch (unc_gettype(c->view, v)) {
    case Unc_TNull:
        return cborenc_raw(c, 74, 22);
    case Unc_TBool:
        return cborenc_raw(c, 74, unc_getbool(c->view, v, 0) ? 21 : 20);
    case Unc_TInt:
    {
        Unc_Int ui;
        e = unc_getint(c->view, v, &ui);
        if (e) return e;
        return cborenc_int(c, ui);
    }
    case Unc_TFloat:
    {
        Unc_Float f;
        e = unc_getfloat(c->view, v, &f);
        if (e) return e;
        return cborenc_f64(c, f);
    }
    case Unc_TBlob:
        return cborenc_blob(c, v);
    case Unc_TString:
        return cborenc_str(c, v);
    case Unc_TArray:
        return cborenc_arr(c, v);
    case Unc_TTable:
        return cborenc_obj(c, v);
    case Unc_TObject:
    {
        /* semantic tag? */
        Unc_Value pr = UNC_BLANK;
        int stag;
        unc_getprototype(c->view, v, &pr);
        stag = unc_issame(c->view, &pr, c->tagproto);
        VCLEAR(c->view, &pr);
        if (stag) {
            /* semantic tag! */
            return cborenc_tag(c, v);
        }
    }
    default:
        return cborenc_err(c, "type",
            "cannot encode value of this type as CBOR");
    }
}

INLINE int is_float_le(void) {
    float f_f = 1.0f;
    byte f_b[sizeof(f_f)];
    unc0_memcpy(f_b, &f_f, sizeof(float));
    return !f_b[0];
}

Unc_RetVal uncl_cbor_decode(Unc_View *w, Unc_Tuple args, void *ud_) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct cbor_decode_context cxt;
    struct cbor_decode_blob rs;
    (void)ud_;

    e = unc_lockblob(w, &args.values[0], &rs.n, &rs.c);
    if (e) return e;
    rs.r = 0;

    cxt.view = w;
    cxt.getch = &cbor_decode_blob_do;
    cxt.getch_data = &rs;
    cxt.tagproto = unc_boundvalue(w, 0);
    cxt.recurse = unc_recurselimit(w);
    cxt.fle = is_float_le();
    
    e = cbordec(&cxt, &v);
    if (e == CBOR_EOL)
        e = unc_throwexc(w, "value", "unexpected CBOR terminator");
    unc_unlock(w, &args.values[0]);
    return unc_returnlocal(w, e, &v);
}

#if !UNCIL_NOLIBIO
Unc_RetVal uncl_cbor_decodefile(Unc_View *w, Unc_Tuple args, void *ud_) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct cbor_decode_context cxt;
    struct cbor_decode_file buf;
    struct ulib_io_file *pfile;
    (void)ud_;

    e = unc0_io_lockfile(w, &args.values[0], &pfile, 0);
    if (e) return e;

    buf.fp = pfile;
    buf.err = 0;

    cxt.view = w;
    cxt.getch = &cbor_decode_file_do;
    cxt.getch_data = &buf;
    cxt.tagproto = unc_boundvalue(w, 0);
    cxt.recurse = unc_recurselimit(w);
    cxt.fle = is_float_le();
    
    e = cbordec(&cxt, &v);
    if (e == CBOR_EOL)
        e = unc_throwexc(w, "value", "unexpected CBOR terminator");
    else if (unc0_io_ferror(pfile))
        e = unc0_io_makeerr_errno(w, "cbor.decodefile()");
    unc0_io_unlockfile(w, &args.values[0]);
    return unc_returnlocal(w, e, &v);
}
#endif

Unc_RetVal uncl_cbor_encode(Unc_View *w, Unc_Tuple args, void *ud_) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;
    struct cbor_encode_context cxt = { NULL, NULL, 0, NULL, NULL, UNC_BLANK };
    struct cbor_encode_blob rs;
    (void)ud_;
    cxt.view = w;
    cxt.out = &cbor_encode_blob_do;
    cxt.out_data = &rs;
    cxt.recurse = unc_recurselimit(w);
    cxt.fle = is_float_le();
    cxt.tagproto = unc_boundvalue(w, 0);
    if (unc_gettype(w, &args.values[1]) && !unc_iscallable(w, &args.values[1]))
        return unc_throwexc(w, "type", "mapper must be callable or null");
    unc_copy(w, &cxt.mapper, &args.values[1]);
    cxt.ents = NULL;
    cxt.ents_n = cxt.ents_c = 0;

    unc0_strbuf_init(&rs.buf, &w->world->alloc, Unc_AllocBlob);
    rs.err = 0;

    e = cborenc(&cxt, &args.values[0]);
    if (rs.err) e = rs.err;
    if (!e) e = unc0_buftoblob(w, &v, &rs.buf);
    e = unc_returnlocal(w, e, &v);
    unc0_strbuf_free(&rs.buf);
    unc_mfree(w, cxt.ents);
    VCLEAR(w, &cxt.mapper);
    VCLEAR(w, &v);
    return e;
}

#if !UNCIL_NOLIBIO
Unc_RetVal uncl_cbor_encodefile(Unc_View *w, Unc_Tuple args, void *ud_) {
    Unc_RetVal e;
    struct cbor_encode_context cxt = { NULL, NULL, 0, NULL, NULL, UNC_BLANK };
    struct ulib_io_file *pfile;
    struct cbor_encode_file buf;
    (void)ud_;

    cxt.view = w;
    cxt.out = &cbor_encode_file_do;
    cxt.out_data = &buf;
    cxt.recurse = unc_recurselimit(w);
    cxt.fle = is_float_le();
    cxt.tagproto = unc_boundvalue(w, 0);
    if (unc_gettype(w, &args.values[1]) && !unc_iscallable(w, &args.values[1]))
        return unc_throwexc(w, "type", "mapper must be callable or null");
    unc_copy(w, &cxt.mapper, &args.values[1]);
    cxt.ents = NULL;
    cxt.ents_n = cxt.ents_c = 0;

    buf.err = 0;
    buf.view = w;

    e = unc0_io_lockfile(w, &args.values[0], &pfile, 0);
    if (e) return e;
    buf.fp = pfile;
    
    e = cborenc(&cxt, &args.values[0]);
    if (buf.err < 0)
        e = UNCIL_ERR_INTERNAL;
    else if (unc0_io_ferror(pfile))
        e = unc0_io_makeerr_errno(w, "cbor.encodefile()");
    unc0_io_unlockfile(w, &args.values[0]);
    unc_mfree(w, cxt.ents);
    VCLEAR(w, &cxt.mapper);
    return e;
}
#endif

#define FN(x) &uncl_cbor_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(decode),        1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(encode),        1, 1, 0, UNC_CFUNC_CONCURRENT },
#if !UNCIL_NOLIBIO
    { FN(decodefile),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(encodefile),    1, 1, 0, UNC_CFUNC_CONCURRENT },
#endif
};

Unc_RetVal uncilmain_cbor(Unc_View *w) {
    Unc_RetVal e;
    Unc_Value cbor_tag = UNC_BLANK;

    e = unc_newobject(w, &cbor_tag, NULL);
    if (e) return e;

#if !UNCIL_NOLIBIO
    e = unc0_io_init(w);
    if (e) return e;
#endif

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "cbor.semantictag");
        if (e) return e;
        e = unc_setattrc(w, &cbor_tag, "__name", &ns);
        if (e) return e;
        VCLEAR(w, &ns);
    }

    e = unc_setpublicc(w, "semantictag", &cbor_tag);
    if (e) return e;

    return unc_exportcfunctions(w, PASSARRAY(lib), 1, &cbor_tag, NULL);
}
