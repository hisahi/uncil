/*******************************************************************************
 
Uncil -- value header for internal use

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

#ifndef UNCIL_UVALI_H
#define UNCIL_UVALI_H

/* check if V is of a reference type */
#define IS_OF_REFTYPE(V) UNCIL_OF_REFTYPE(V)
/* ptr to leftover space after Unc_Entity header */
#define LEFTOVER(T, e) ((T *)((char *)(e) + ASIZEOF(Unc_Entity)))
/* ptr to Unc_Entity from ptr to leftover space */
#define UNLEFTOVER(e) ((Unc_Entity *)((char *)(e) - ASIZEOF(Unc_Entity)))

#define VINCREF(w, V) \
    do { Unc_Value *ti_ = (V); UNCIL_INCREF(w, ti_); } while (0)
#define VDECREF(w, V) \
    do { Unc_Value *td_ = (V); UNCIL_DECREF(w, td_); } while (0)

/* fetch value */
#define VGETRAW(V) *(V)
/* fetch value type */
#define VGETTYPE(V) ((V)->type)
/* fetch bool */
#if UNCIL_C99
#define VGETBOOL(V) !!((uint_fast8_t)((V)->v.i))
#else
#define VGETBOOL(V) !!((V)->v.i)
#endif
/* fetch int */
#define VGETINT(V) ((V)->v.i)
/* fetch float */
#define VGETFLT(V) ((V)->v.f)
/* fetch optr */
#define VGETPTR(V) ((V)->v.p)
/* ptr to Unc_Entity of value */
#define VGETENT(V) ((V)->v.c)

/* assign value */
#define VSETRAW(D, V) *(D) = (V)

/* assign null value */
#define VINITFAST(D) ((D)->type = Unc_TNull)
/* assign null value */
#define VINITNULL(D) do { register Unc_Value *tN_ = (D);                       \
            tN_->type = Unc_TNull; tN_->v.p = NULL; } while (0)
/* assign bool value */
#define VINITBOOL(D, b) do { Unc_Int vB_ = !!(b);                              \
            register Unc_Value *tB_ = (D);                                     \
            tB_->type = Unc_TBool; tB_->v.i = vB_; } while (0)
/* assign int value */
#define VINITINT(D, q) do { Unc_Int vI_ = (Unc_Int)(q);                        \
            register Unc_Value *tI_ = (D);                                     \
            tI_->type = Unc_TInt; tI_->v.i = vI_; } while (0)
/* assign float value */
#define VINITFLT(D, q) do { Unc_Float vF_ = (Unc_Float)(q);                    \
            register Unc_Value *tF_ = (D);                                     \
            tF_->type = Unc_TFloat; tF_->v.f = vF_; } while (0)
/* assign optr */
#define VINITPTR(D, q) do { void *vP_ = (void *)(q);                           \
            register Unc_Value *tP_ = (D);                                     \
            tP_->type = Unc_TOpaquePtr; tP_->v.p = vP_; } while (0)
/* assign entity */
#define VINITENT(D, t, e) do { register Unc_Value *tE_ = (D);                  \
            tE_->type = (t); VGETENT(tE_) = (Unc_Entity *)(e);                 \
            UNCIL_INCREFE(w, VGETENT(tE_)); } while (0)

/* assign null value and decref old. equivalent to public unc_clear */
#define VSETNULL(w, D) do { Unc_Value *t_N_ = (D); VDECREF(w, t_N_);           \
                            VINITNULL(t_N_); } while (0)
/* assign bool value and decref old */
#define VSETBOOL(w, D, b) do { Unc_Int v_B_ = b;                               \
                            Unc_Value *t_B_ = (D); VDECREF(w, t_B_);           \
                            VINITBOOL(t_B_, v_B_); } while (0)
/* assign int value and decref old */
#define VSETINT(w, D, q) do { Unc_Int v_I_ = q;                                \
                            Unc_Value *t_I_ = (D); VDECREF(w, t_I_);           \
                            VINITINT(t_I_, v_I_); } while (0)
/* assign float value and decref old */
#define VSETFLT(w, D, q) do { Unc_Float v_F_ = q;                              \
                            Unc_Value *t_F_ = (D); VDECREF(w, t_F_);           \
                            VINITFLT(t_F_, v_F_); } while (0)
/* assign optr and decref old */
#define VSETPTR(w, D, p) do { void *v_P_ = p;                                  \
                            Unc_Value *t_P_ = (D); VDECREF(w, t_P_);           \
                            VINITPTR(t_P_, v_P_); } while (0)
/* assign entity and decref old */
#define VSETENT(w, D, t, e) do { Unc_ValueType vt_E_ = t; void *v_E_ = e;      \
                            Unc_Value *t_E_ = (D); VDECREF(w, t_E_);           \
                            VINITENT(t_E_, vt_E_, v_E_); } while (0)

/* done before a value is destroyed. equivalent to unc_clear, except faster.
   should not be used for any other purpose than the aforementioned one */
#define VCLEAR(w, D) do { Unc_Value *t_N_ = (D); VDECREF(w, t_N_);             \
                          VINITFAST(t_N_); } while (0)
/* assign value without incref */
#define VMOVE(w, D, V) do { Unc_Value *v2_ = (D);                              \
                            VDECREF(w, v2_);                                   \
                            VSETRAW(v2_, VGETRAW(V)); } while (0)
/* assign value with incref */
#define VCOPY(w, D, V) do { Unc_Value *v1_ = (V);                              \
                            VINCREF(w, v1_);                                   \
                            VMOVE(w, D, v1_); } while (0)
/* assign value with incref but without decref */
#define VIMPOSE(w, D, V) do { Unc_Value *v0_ = (V);                            \
                              VINCREF(w, v0_);                                 \
                              VSETRAW(D, VGETRAW(v0_)); } while (0)

/* assign null values to uninitialized buffer */
#define VINITMANY(n, p)  do {   Unc_Size i;                                    \
                                for (i = 0; i < n; ++i)                        \
                                    VINITNULL((p) + i); } while (0)

#define STRINGIFY(s) #s
#define EVALSTRINGIFY(s) STRINGIFY(s)
/* operator overload */
#define OPOVERLOAD(s) "__" STRINGIFY(s)
/* string literal as length, const byte * pair */
#define PASSSTRL(s) sizeof(s) - 1, (const byte *)s
/* string literal as length, const char * pair */
#define PASSSTRLC(s) sizeof(s) - 1, s
/* whether entity is "sleeping" */
#define IS_SLEEPING(e) ((e)->mark & ((UCHAR_MAX / 2) + 1))

#endif /* UNCIL_UVALI_H */
