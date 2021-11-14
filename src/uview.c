/*******************************************************************************
 
Uncil -- view impl

Copyright (c) 2021 Sampo Hippeläinen (hisahi)

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

#define UNCIL_DEFINES

#include "udebug.h"
#include "ugc.h"
#include "umt.h"
#include "uncil.h"
#include "uprog.h"
#include "utxt.h"
#include "uval.h"
#include "uvali.h"
#include "uview.h"

Unc_World *unc__incept(Unc_Alloc allocfn, void *udata) {
    Unc_World *world;
    Unc_Allocator alloc;
    int e;
    unc__initalloc(&alloc, NULL, allocfn, udata);
    world = unc__malloc(&alloc, 0, sizeof(Unc_World));
    if (!world) return NULL;
    
    world->alloc = alloc;
    world->alloc.world = world;
    world->etop = NULL;
    world->vnid = 0;
    world->viewc = 0;
    world->view = NULL;
    world->viewlast = NULL;
    world->wmode = 0;
    if ((e = UNC_LOCKINITF(world->viewlist_lock))) goto unc__incept_fail_l0;
    if ((e = UNC_LOCKINITF(world->public_lock))) goto unc__incept_fail_l1;
    if ((e = UNC_LOCKINITF(world->entity_lock))) goto unc__incept_fail_l2;
    unc__inithtbls(&alloc, &world->pubs);
    VINITNULL(&world->met_str);
    VINITNULL(&world->met_blob);
    VINITNULL(&world->met_arr);
    VINITNULL(&world->met_dict);
    VINITNULL(&world->io_file);
    world->ccxt.alloc = NULL;
    unc__inithtbls(&alloc, &world->modulecache);
    unc__gcdefaults(&world->gc);
    VINITNULL(&world->modulepaths);
    VINITNULL(&world->moduledlpaths);
    unc__initenctable(&alloc, &world->encs);
    if (0) goto unc__incept_fail;
    ATOMICLSET(world->refs, 0);
    ATOMICSSET(world->finalize, 0);
    return world;

unc__incept_fail:
    UNC_LOCKFINAF(world->entity_lock);
unc__incept_fail_l2:
    UNC_LOCKFINAF(world->public_lock);
unc__incept_fail_l1:
    UNC_LOCKFINAF(world->viewlist_lock);
unc__incept_fail_l0:
    unc__mfree(&alloc, world, sizeof(Unc_World));
    return NULL;
}

void unc__haltview(Unc_View *w) {
    ATOMICSSET(w->flow, UNC_VIEW_FLOW_HALT);
}

INLINE void unc__waitsubviews(Unc_World *w) {
    Unc_View *v;
    if (ATOMICSXCG(w->finalize, 1)) return;
#if UNCIL_MT_OK
    v = w->view;
    while (v) {
        if (v->vtype == Unc_ViewTypeSub) {
            if (VGETTYPE(&v->threadme) == Unc_TOpaque)
                unc__waitonviewthread(v);
            else {
                while (v->vtype >= 0) {
                    UNC_LOCKF(v->runlock);
                    UNC_UNLOCKF(v->runlock);
                    UNC_YIELD();
                }
            }
        }
        v = v->nextview;
    }
#endif
    ATOMICSSET(w->finalize, 2);
    v = w->view;
    while (v) {
        if (v->vtype >= 0) {
            unc__haltview(v);
            v = v->nextview;
        } else {
            Unc_View *vv = v->nextview;
            unc__freeview(v);
            v = vv;
        }
    }
    UNC_UNLOCKF(w->viewlist_lock);
}

void unc__doomsday(Unc_View *v, Unc_World *w) {
    Unc_Allocator alloc = w->alloc;
    Unc_Entity *e, *ee;
    if (v) UNC_UNLOCKF(w->viewlist_lock);
    e = w->etop;
    if (v) unc__dropenctable(v, &w->encs);
    while (e) {
        if (e->type == Unc_TOpaque)
            unc__graceopaque(v, LEFTOVER(Unc_Opaque, e));
        e = e->down;
    }
    if (w->ccxt.alloc) unc__dropcontext(&w->ccxt);
    if (v) {
        unc__drophtbls(v, &w->pubs);
        unc__drophtbls(v, &w->modulecache);
        unc_clear(v, &w->modulepaths);
        unc_clear(v, &w->moduledlpaths);
    }
    e = w->etop;
    while (e) {
        ee = e->down;
        unc__scrap(e, &alloc, NULL);
        unc__efree(e, &alloc);
        e = ee;
    }
    UNC_LOCKFINAF(w->entity_lock);
    UNC_LOCKFINAF(w->public_lock);
    UNC_LOCKFINAF(w->viewlist_lock);
    unc__mfree(&alloc, w, sizeof(Unc_World));
}

#define INITIAL_REGION_SIZE 8
#define INITIAL_FRAMES_SIZE 4

Unc_View *unc__newview(Unc_World *w, Unc_ViewType vtype) {
    Unc_Allocator *alloc = &w->alloc;
    Unc_View *view;
    if (w->finalize) return NULL;
    view = unc__malloc(alloc, 0, sizeof(Unc_View));
    if (!view) return NULL;
    
    view->world = w;
    if (unc__stackinit(view, &view->sreg, 16))
        goto fail0;
    if (unc__stackinit(view, &view->sval, 16))
        goto fail1;
    if (!(view->frames.base = unc__malloc(alloc, 0, INITIAL_FRAMES_SIZE *
                                                    sizeof(Unc_Frame))))
        goto fail2;
    view->frames.top = view->frames.base;
    view->frames.end = view->frames.base + INITIAL_FRAMES_SIZE;
    if (!(view->region.base = unc__malloc(alloc, 0, INITIAL_REGION_SIZE *
                                                    sizeof(Unc_Value *))))
        goto fail3;
    view->region.top = view->region.base;
    view->region.end = view->region.base + INITIAL_FRAMES_SIZE;
    view->pc = NULL;
    view->cfunc = NULL;
    ATOMICSSET(view->paused, 0);
    view->entityload = 0;
    {
        int i;
        for (i = 0; i < ARRAYSIZE(view->sleepers); ++i)
            view->sleepers[i] = NULL;
    }
    view->sleeper_next = 0;
    view->recurse = 0;
    view->recurselimit = UNCIL_DEFAULT_RECURSE_LIMIT;
    VINITNULL(&view->exc);
    view->import = 0;
    view->program = NULL;
    VINITNULL(&view->fmain);
    view->exports = NULL;
    view->has_lasterr = 0;
    ATOMICSSET(view->flow, UNC_VIEW_FLOW_RUN);
    view->mframes = NULL;
    view->curdir_n = 0;
    view->curdir = NULL;
    view->swith.base = view->swith.top = view->swith.end = NULL;
    view->rwith.base = view->rwith.top = view->rwith.end = NULL;
    UNC_LOCKF(w->viewlist_lock);
    if (w->finalize) {
        UNC_UNLOCKF(w->viewlist_lock);
        goto fail4;
    }
    view->pubs = &w->pubs;
    view->met_str = w->met_str;
    view->met_blob = w->met_blob;
    view->met_arr = w->met_arr;
    view->met_dict = w->met_dict;
    view->uncfname = NULL;
    view->trampoline = NULL;
    if (!(view->vtype = vtype))
        ATOMICLINC(w->refs);
    VINITNULL(&view->coroutine);
#if UNCIL_MT_OK
    VINITNULL(&view->threadme);
    if (UNC_LOCKINITF(view->runlock)) {
        UNC_UNLOCKF(w->viewlist_lock);
        goto fail4;
    }
#endif
    /* try to assign an ID */
    if (w->vnid == w->viewc) {
        if (!(w->vnid + 1))
            goto fail5;
        view->vid = w->vnid++;
        if ((view->nextview = w->view))
            view->nextview->prevview = view;
        view->prevview = NULL;
        w->view = view;
    } else {
        /* look for free ID */
        Unc_Size s = 0;
        Unc_View *lv = w->viewlast;
        view->vid = w->vnid;
        while (lv) {
            if (lv->vid == s) {
                lv = lv->prevview;
                ++s;
            } else {
                view->vid = s;
                break;
            }
        }
        if (!lv || view->vid == w->vnid) {
            NEVER_();
            goto fail5;
        }
        view->prevview = lv;
        if ((view->nextview = lv->nextview))
            lv->nextview->prevview = view;
        else
            w->viewlast = view;
        lv->nextview = view;
    }
    if (!w->viewlast) w->viewlast = view;
    ++w->viewc;
    UNC_UNLOCKF(w->viewlist_lock);
    VINCREF(view, &view->met_str);
    VINCREF(view, &view->met_blob);
    VINCREF(view, &view->met_arr);
    VINCREF(view, &view->met_dict);
    return view;
fail5:
#if UNCIL_MT_OK
    UNC_LOCKFINAF(view->runlock);
#endif
fail4:
    unc__mfree(alloc, view->region.base, sizeof(Unc_Value *) *
                                (view->region.end - view->region.base));
fail3:
    unc__mfree(alloc, view->frames.base, sizeof(Unc_Frame) *
                                (view->frames.end - view->frames.base));
fail2:
    unc__stackfree(view, &view->sval);
fail1:
    unc__stackfree(view, &view->sreg);
fail0:
    unc__mfree(alloc, view, sizeof(Unc_View));
    return NULL;
}

void unc__freeview(Unc_View *v) {
    Unc_Size i;
    Unc_World *w = v->world;
    Unc_Allocator alloc = w->alloc;
    ATOMICSSET(v->paused, 1);
    (void)UNC_LOCKFP(v, w->viewlist_lock);
    unc__stackwunwind(v, &v->swith, 0, 0);
    VDECREF(v, &v->exc);
    VDECREF(v, &v->fmain);
    VDECREF(v, &v->coroutine);
#if UNCIL_MT_OK
    VDECREF(v, &v->threadme);
#endif
    if (v->pubs && v->pubs != &w->pubs) unc__drophtbls(v, v->pubs);
    if (v->exports) unc__drophtbls(v, v->exports);
    if (v->program) unc__progdecref(v->program, &alloc);
    for (i = 0; i < UNC_SLEEPER_VALUES; ++i)
        if (v->sleepers[i])
            unc__wreck(v->sleepers[i], w);
    unc__stackfree(v, &v->sreg);
    unc__stackfree(v, &v->sval);
    unc__stackfree(v, &v->swith);
    unc__mfree(&alloc, v->rwith.base, sizeof(size_t) *
                                (v->rwith.end - v->rwith.base));
    unc__mfree(&alloc, v->frames.base, sizeof(Unc_Frame) *
                                (v->frames.end - v->frames.base));
    unc__mfree(&alloc, v->region.base, sizeof(Unc_Value *) *
                                (v->region.end - v->region.base));
    if (v->prevview) v->prevview->nextview = v->nextview;
    else if (w->view == v) w->view = v->nextview;
    if (v->nextview) v->nextview->prevview = v->prevview;
    else if (w->viewlast == v) w->viewlast = v->prevview;
    if (!v->vtype && !ATOMICLDEC(w->refs))
        unc__waitsubviews(w);
#if UNCIL_MT_OK
    UNC_LOCKFINAF(v->runlock);
#endif
    if (!--w->viewc) unc__doomsday(v, w);
    else UNC_UNLOCKF(w->viewlist_lock);
    unc__mfree(&alloc, v, sizeof(Unc_View));
}

void unc__wsetprogram(Unc_View *w, Unc_Program *p) {
    Unc_Program *op = w->program;
    w->program = unc__progincref(p);
    if (op) unc__progdecref(op, &w->world->alloc);
    w->bcode = p->code;
    w->bdata = p->data;
}
