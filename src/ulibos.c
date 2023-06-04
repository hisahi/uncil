/*******************************************************************************
 
Uncil -- builtin os library impl

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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define UNCIL_DEFINES

#include "udef.h"
#include "ugc.h"
#include "umem.h"
#include "uncil.h"
#include "uosdef.h"
#include "uvsio.h"

#if UNCIL_IS_POSIX
extern char **environ;
#endif

Unc_RetVal uncl_os_getenv(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    const char *sb, *se;
    Unc_RetVal e;
    (void)udata;

    if (!unc_gettype(w, &args.values[0])) {
#if UNCIL_IS_POSIX
        Unc_Value vt = UNC_BLANK;
        const char **env = (const char **)environ;
        e = unc_newtable(w, &v);
        while (*env) {
            const char *s = *env++;
            const char *sv = strchr(s, '=');
            if (!sv) continue;
            e = unc_newstringc(w, &vt, sv + 1);
            if (e) break;
            e = unc_setattrs(w, &v, sv - s, s, &vt);
            if (e) break;
        }
        VCLEAR(w, &vt);
        return unc_returnlocal(w, 0, &v);
#else
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }

    e = unc_getstringc(w, &args.values[0], &sb);
    if (e) return e;

    se = getenv(sb);
    if (se) {
        e = unc_newstringc(w, &v, se);
        if (e) return e;
    }
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_os_system(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    const char *sb;
    Unc_RetVal e;
    (void)udata;

    if (!unc_gettype(w, &args.values[0])) {
        VINITBOOL(&v, system(NULL));
    } else {
        e = unc_getstringc(w, &args.values[0], &sb);
        if (e) return e;
        VINITINT(&v, system(sb));
    }
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_os_time(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    (void)udata;

    VINITFLT(&v, (Unc_Float)time(NULL));
    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_os_difftime(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v;
    Unc_Float f0, f1;
    (void)udata;

    e = unc_getfloat(w, &args.values[0], &f0);
    if (e) return e;
    e = unc_getfloat(w, &args.values[1], &f1);
    if (e) return e;

    VINITINT(&v, (Unc_Float)difftime((time_t)f0, (time_t)f1));
    return unc_returnlocal(w, 0, &v);
}

#if UNCIL_IS_UNIX
#include <unistd.h>
#endif

#if UNCIL_IS_LINUX
#include <sys/sysinfo.h>
#endif

Unc_RetVal uncl_os_nprocs(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    (void)udata;

#if UNCIL_IS_LINUX
    VINITINT(&v, get_nprocs_conf());
#else
    VINITNULL(&v);
#endif

    return unc_returnlocal(w, 0, &v);
}

Unc_RetVal uncl_os_freemem(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    (void)udata;

#if UNCIL_IS_UNIX
    VINITINT(&v, sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE));
#else
    VINITNULL(&v);
#endif

    return unc_returnlocal(w, 0, &v);
}

#define STRCOMPARE(sn, sb, s) ((sn == (sizeof(s) - 1)) &&                      \
                               !unc0_memcmp(s, sb, sn))

#if UNCIL_IS_LINUX || UNCIL_IS_BSD || \
    (UNCIL_IS_POSIX && _POSIX_VERSION >= 200112L)
#define CAN_UNAME 1
#include <sys/utsname.h>
#endif

Unc_RetVal uncl_os_version(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sb;
    (void)udata;

    if (!unc_gettype(w, &args.values[0])) {
        Unc_RetVal e;
        e = unc_newstringc(w, &v, UNCIL_TARGET);
        return unc_returnlocal(w, e, &v);
    } else {
        Unc_RetVal e;
        e = unc_getstring(w, &args.values[0], &sn, &sb);
        if (e) return e;
        if (STRCOMPARE(sn, sb, "major")) {
#if CAN_UNAME
            struct utsname utsn;
            if (uname(&utsn) >= 0) {
                int m = 0;
                if (sscanf(utsn.release, "%d", &m) > 0) {
                    VINITINT(&v, m);
                } else
                    VINITNULL(&v);
            } else
                VINITNULL(&v);
#else
            VINITNULL(&v);
#endif
        } else if (STRCOMPARE(sn, sb, "minor")) {
#if CAN_UNAME
            struct utsname utsn;
            if (uname(&utsn) >= 0) {
                int m = 0;
                if (sscanf(utsn.release, "%*d.%d", &m) > 0) {
                    VINITINT(&v, m);
                } else
                    VINITNULL(&v);
            } else
                VINITNULL(&v);
#else
            VINITNULL(&v);
#endif
        } else if (STRCOMPARE(sn, sb, "name")) {
#if CAN_UNAME
            struct utsname utsn;
            if (uname(&utsn) >= 0) {
                e = unc0_usxprintf(w, &v, "%s %s", utsn.sysname, utsn.release);
                if (e) return e;
            } else
                VINITNULL(&v);
#else
            VINITNULL(&v);
#endif
        } else if (STRCOMPARE(sn, sb, "host")) {
#if CAN_UNAME
            struct utsname utsn;
            if (uname(&utsn) >= 0) {
                e = unc_newstringc(w, &v, utsn.nodename);
                if (e) return e;
            } else
                VINITNULL(&v);
#else
            VINITNULL(&v);
#endif
        } else if (STRCOMPARE(sn, sb, "posix")) {
#if UNCIL_IS_POSIX
            VINITBOOL(&v, 1);
#else
            VINITBOOL(&v, 0);
#endif
        } else {
            VINITNULL(&v);
        }
        return unc_returnlocal(w, 0, &v);
    }
}

#define FN(x) &uncl_os_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(getenv),        0, 1, 0, UNC_CFUNC_DEFAULT },
    { FN(system),        0, 1, 0, UNC_CFUNC_DEFAULT },
    { FN(time),          0, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(difftime),      2, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(nprocs),        0, 0, 0, UNC_CFUNC_DEFAULT },
    { FN(freemem),       0, 0, 0, UNC_CFUNC_DEFAULT },
    { FN(version),       0, 1, 0, UNC_CFUNC_DEFAULT },
};

Unc_RetVal uncilmain_os(Unc_View *w) {
    return unc_exportcfunctions(w, PASSARRAY(lib), 0, NULL, NULL);
}
