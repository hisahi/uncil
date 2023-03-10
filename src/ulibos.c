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
#include "uncil.h"
#include "uosdef.h"
#include "uvsio.h"

#if UNCIL_IS_POSIX
extern char **environ;
#endif

Unc_RetVal unc0_lib_os_getenv(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    const char *sb, *se;
    int e;
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
        unc_clear(w, &vt);
        if (!e) e = unc_pushmove(w, &v, NULL);
        if (e) unc_clear(w, &v);
        return e;
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
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_os_system(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    const char *sb;
    int e;
    (void)udata;

    if (!unc_gettype(w, &args.values[0])) {
        VINITBOOL(&v, system(NULL));
    } else {
        e = unc_getstringc(w, &args.values[0], &sb);
        if (e) return e;
        VINITINT(&v, system(sb));
    }
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_os_time(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    (void)udata;

    VINITFLT(&v, (Unc_Float)time(NULL));
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_os_difftime(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v;
    Unc_Float f0, f1;
    (void)udata;

    e = unc_getfloat(w, &args.values[0], &f0);
    if (e) return e;
    e = unc_getfloat(w, &args.values[1], &f1);
    if (e) return e;

    VINITINT(&v, (Unc_Float)difftime((time_t)f0, (time_t)f1));
    return unc_push(w, 1, &v, NULL);
}

#if UNCIL_IS_UNIX
#include <unistd.h>
#endif

#if UNCIL_IS_LINUX
#include <sys/sysinfo.h>
#endif

Unc_RetVal unc0_lib_os_nprocs(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    (void)udata;

#if UNCIL_IS_LINUX
    VINITINT(&v, get_nprocs_conf());
#else
    VINITNULL(&v);
#endif

    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc0_lib_os_freemem(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v;
    (void)udata;

#if UNCIL_IS_UNIX
    VINITINT(&v, sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE));
#else
    VINITNULL(&v);
#endif

    return unc_push(w, 1, &v, NULL);
}

#define STRCOMPARE(sn, sb, s) ((sn == (sizeof(s) - 1)) && !memcmp(s, sb, sn))

#if UNCIL_IS_LINUX || UNCIL_IS_BSD || \
    (UNCIL_IS_POSIX && _POSIX_VERSION >= 200112L)
#define CAN_UNAME 1
#include <sys/utsname.h>
#endif

Unc_RetVal unc0_lib_os_version(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK;
    Unc_Size sn;
    const char *sb;
    (void)udata;

    if (!unc_gettype(w, &args.values[0])) {
        int e;
        e = unc_newstringc(w, &v, UNCIL_TARGET);
        if (e) return e;
        return unc_pushmove(w, &v, NULL);
    } else {
        int e;
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
        return unc_pushmove(w, &v, NULL);
    }
}

Unc_RetVal uncilmain_os(Unc_View *w) {
    Unc_RetVal e;
    e = unc_exportcfunction(w, "getenv", &unc0_lib_os_getenv,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "system", &unc0_lib_os_system,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "time", &unc0_lib_os_time,
                            UNC_CFUNC_CONCURRENT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "difftime", &unc0_lib_os_difftime,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "nprocs", &unc0_lib_os_nprocs,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "freemem", &unc0_lib_os_freemem,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "version", &unc0_lib_os_version,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    return 0;
}
