/*******************************************************************************
 
Uncil -- builtin io library impl

Copyright (c) 2021-2022 Sampo Hippeläinen (hisahi)

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

#include "uosdef.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNCIL_DEFINES

#include "udebug.h"
#include "udef.h"
#include "ugc.h"
#include "ulibio.h"
#include "uncil.h"
#include "utxt.h"
#include "uutf.h"
#include "uvali.h"
#include "uvsio.h"

#define UNCIL_IO_FILE_ENC_BINARY 0
#define UNCIL_IO_FILE_ENC_UTF8 1

#define UNCIL_IO_FILE_FLAG_READABLE 1
#define UNCIL_IO_FILE_FLAG_WRITABLE 2
#define UNCIL_IO_FILE_FLAG_SEEKABLE 4
#define UNCIL_IO_FILE_FLAG_TESTSEEKABLE 8
#define UNCIL_IO_FILE_FLAG_DELETEONCLOSE 16

#if UNCIL_IS_POSIX
#include <fcntl.h>
#endif

#if UNCIL_IS_WINDOWS
#include <fcntl.h>
#include <io.h>
#endif

#if UNCIL_IS_POSIX || UNCIL_IS_WINDOWS
#define STDIO_FILE_THREADSAFE
#endif

#if _POSIX_VERSION >= 200112L && !UNCIL_IS_CYGWIN
#define FSEEKO 1
#define _FILE_OFFSET_BITS 64
#endif

#if _POSIX_VERSION >= 200112L || _XOPEN_SOURCE >= 500
#define MKSTEMP_FN "/tmp/uncil_tmpXXXXXX"
#endif

static int unc0_io_fopen(struct ulib_io_file *file,
                         const char *fn, char *mode) {
    FILE *f;
    errno = 0;
    f = fopen(fn, mode);
    if (!f) return -1;
    file->f = f;
    return 0;
}

static int unc0_io_pipe(struct ulib_io_file *files2) {
#if UNCIL_IS_POSIX
    FILE *cf;
    int fds[2], e = pipe(fds);
    if (e) return UNCIL_ERR_IO_GENERIC;
    cf = fdopen(fds[0], "rb");
    if (!cf) {
        close(fds[1]);
        close(fds[0]);
        return UNCIL_ERR_IO_GENERIC;
    }
    files2[0].f = cf;
    (void)setvbuf(cf, NULL, _IONBF, 0);
    cf = fdopen(fds[1], "wb");
    if (!cf) {
        close(fds[1]);
        close(fds[0]);
        return UNCIL_ERR_IO_GENERIC;
    }
    files2[1].f = cf;
    (void)setvbuf(cf, NULL, _IONBF, 0);
    return 0;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

#define TMPFILE_MEM 1
#define TMPFILE_FAIL 2

static int unc0_io_tmpfile(Unc_Allocator *alloc,
                           struct ulib_io_file *file,
                           char *mode) {
    /* check if mode is w+(b) */
    int flags = 0;
    const char *m = mode;
    char c;
    while ((c = *m++)) {
        if (c == 'w') flags |= 1;
        else if (c == '+') flags |= 2;
        else {
            flags = 0;
            break;
        }
    }

    errno = 0;
    if (flags == 3) {
        FILE *f = tmpfile();
        if (!f) return -1;
        file->f = f;
        return 0;
    } else {
#ifdef MKSTEMP_FN
        char tmp[] = MKSTEMP_FN;
        int fd = mkstemp(tmp);
        if (fd == -1) return TMPFILE_FAIL;
        file->tfn = unc0_mmalloc(alloc, Unc_AllocLibrary, sizeof(tmp));
        if (!file->tfn) {
            close(fd);
            return TMPFILE_MEM;
        }
        strcpy(file->tfn, tmp);
        file->f = fdopen(fd, mode);
        if (!file->f) {
            unc0_mmfree(alloc, file->tfn);
            close(fd);
            return TMPFILE_MEM;
        }
        file->flags |= UNCIL_IO_FILE_FLAG_DELETEONCLOSE;
        return 0;
#else
        /* not the best fallback */
        char *buf = tmpnam(NULL);
        FILE *f;
        if (!buf) return TMPFILE_FAIL;
        file->tfn = unc0_mmalloc(alloc, Unc_AllocLibrary, strlen(buf + 1));
        if (!file->tfn) return TMPFILE_MEM;
        f = fopen(buf, mode);
        if (!f) {
            unc0_mmfree(alloc, file->tfn);
            return -1;
        }
        strcpy(file->tfn, buf);
        file->f = f;
        file->flags |= UNCIL_IO_FILE_FLAG_DELETEONCLOSE;
        return 0;
#endif
    }
}

/* out will be a buffer with space for at least TMPFILENAME_MAX characters
   (so TMPFILENAME_MAX+1 long), or at least 12 characters long whichever
   is shortest.
   please define TMPFILENAME_MAX! */
static int unc0_io_tmpfilenamed(Unc_Allocator *alloc,
                                struct ulib_io_file *file,
                                char *out, char *mode) {
#ifdef MKSTEMP_FN
#define TMPFILENAME_MAX (sizeof(MKSTEMP_FN) - 1)
    int fd;
    strcpy(out, MKSTEMP_FN);
    fd = mkstemp(out);
    if (fd == -1) return TMPFILE_FAIL;
    file->tfn = unc0_mmalloc(alloc, Unc_AllocLibrary, sizeof(MKSTEMP_FN));
    if (!file->tfn) {
        close(fd);
        return TMPFILE_MEM;
    }
    strcpy(file->tfn, out);
    file->f = fdopen(fd, mode);
    if (!file->f) {
        unc0_mmfree(alloc, file->tfn);
        close(fd);
        return TMPFILE_MEM;
    }
    file->flags |= UNCIL_IO_FILE_FLAG_DELETEONCLOSE;
    return 0;
#else
#define TMPFILENAME_MAX L_tmpnam
    FILE *f;
    if (!tmpnam(out)) return TMPFILE_FAIL;
    file->tfn = unc0_mmalloc(alloc, Unc_AllocLibrary, strlen(out + 1));
    if (!file->tfn) return TMPFILE_MEM;
    f = fopen(out, mode);
    if (!f) {
        unc0_mmfree(alloc, file->tfn);
        return -1;
    }
    strcpy(file->tfn, out);
    file->f = f;
    file->flags |= UNCIL_IO_FILE_FLAG_DELETEONCLOSE;
    return 0;
#endif
}

static int unc0_io_fseek(struct ulib_io_file *file, Unc_Int n, int offset,
                         Unc_View *w) {
    int e = 0;
    clearerr(file->f);
#if FSEEKO
    if ((Unc_Int)(off_t)n != n)
        return unc_throwexc(w, "system", "seek value is too large");
    e = fseeko(file->f, (off_t)n, offset);
#else
    if (n < LONG_MIN || n > LONG_MAX)
        return unc_throwexc(w, "system", "seek value is too large");
    e = fseek(file->f, (long)n, offset);
#endif
    return e;
}

static int unc0_io_ftell(struct ulib_io_file *file, Unc_Int *n) {
    Unc_Int r;
    ASSERT(file->f);
    clearerr(file->f);
#if FSEEKO
    r = ftello(file->f);
    if (r == -1L)
        return EOF;
    *n = r;
#else
    r = ftell(file->f);
    if (r == -1L)
        return EOF;
    *n = r;
#endif
    return 0;
}

static int unc0_io_setvbuf(struct ulib_io_file *file, char *buf,
                           int mode, size_t size) {
    return setvbuf(file->f, buf, mode, size);
}

static int unc0_io_isopen(struct ulib_io_file *file) {
    return file->f != NULL;
}

int unc0_io_feof(struct ulib_io_file *file) {
    ASSERT(file->f);
    return feof(file->f);
}

int unc0_io_ferror(struct ulib_io_file *file) {
    ASSERT(file->f);
    return ferror(file->f);
}

int unc0_io_fgetc(struct ulib_io_file *file) {
    clearerr(file->f);
    return getc(file->f);
}
/*
int unc0_io_fputc(int c, struct ulib_io_file *file) {
    clearerr(file->f);
    return putc(c, file->f);
}*/

int unc0_io_fgetc_p(Unc_View *w, struct ulib_io_file *file) {
    int e;
    clearerr(file->f);
    unc_vmpause(w);
    e = getc(file->f);
    unc_vmresume(w);
    return e;
}

int unc0_io_fputc_p(Unc_View *w, int c, struct ulib_io_file *file) {
    int e;
    clearerr(file->f);
    unc_vmpause(w);
    e = putc(c, file->f);
    unc_vmresume(w);
    return e;
}

/*
int unc0_io_fread(struct ulib_io_file *file, byte *b, Unc_Size o, Unc_Size n) {
    size_t e;
    ASSERT(file->f);
    clearerr(file->f);
    e = fread(b + o, 1, n, file->f);
    return e < n && unc0_io_ferror(file) ? EOF : 0;
}

int unc0_io_fwrite(struct ulib_io_file *file, const byte *b, Unc_Size n) {
    ASSERT(file->f);
    clearerr(file->f);
    errno = 0;
    if (n > (Unc_Size)SIZE_MAX) {
        Unc_Size us = SIZE_MAX;
        while (n > us) {
            if (!fwrite(b, us, 1, file->f))
                return 1;
            n -= us;
            b += us;
        }
        return 0;
    }
    return !fwrite(b, n, 1, file->f);
}

int unc0_io_fflush(struct ulib_io_file *file) {
    ASSERT(file->f);
    clearerr(file->f);
    return fflush(file->f);
}

static int unc0_io_fclose(Unc_Allocator *alloc, struct ulib_io_file *file) {
    int e = 0;
    if (file->f) {
        clearerr(file->f);
        e = fclose(file->f);
        if (file->flags & UNCIL_IO_FILE_FLAG_DELETEONCLOSE) {
            int errno_store = errno;
            file->flags &= ~UNCIL_IO_FILE_FLAG_DELETEONCLOSE;
            remove(file->tfn);
            unc0_mmfree(alloc, file->tfn);
            errno = errno_store;
        }
        file->f = NULL;
    }
    return e;
}*/

int unc0_io_fread_p(Unc_View *w, struct ulib_io_file *file,
                    byte *b, Unc_Size o, Unc_Size n) {
    size_t e;
    ASSERT(file->f);
    clearerr(file->f);
    unc_vmpause(w);
    e = fread(b + o, 1, n, file->f);
    unc_vmresume(w);
    return e < n && unc0_io_ferror(file) ? EOF : 0;
}

int unc0_io_fwrite_p(Unc_View *w, struct ulib_io_file *file,
                     const byte *b, Unc_Size n) {
    int e;
    ASSERT(file->f);
    clearerr(file->f);
    errno = 0;
    unc_vmpause(w);
    if (n > (Unc_Size)SIZE_MAX) {
        Unc_Size us = SIZE_MAX;
        while (n > us) {
            if (!fwrite(b, us, 1, file->f)) {
                unc_vmresume(w);
                return 1;
            }
            n -= us;
            b += us;
        }
        return 0;
    }
    e = !fwrite(b, n, 1, file->f);
    unc_vmresume(w);
    return e;
}

int unc0_io_fflush_p(Unc_View *w, struct ulib_io_file *file) {
    int e;
    ASSERT(file->f);
    clearerr(file->f);
    unc_vmpause(w);
    e = fflush(file->f);
    unc_vmresume(w);
    return e;
}

int unc0_io_fclose_p(Unc_View *w, Unc_Allocator *alloc,
                     struct ulib_io_file *file) {
    int e = 0;
    if (file->f) {
        unc_vmpause(w);
        clearerr(file->f);
        e = fclose(file->f);
        if (file->flags & UNCIL_IO_FILE_FLAG_DELETEONCLOSE) {
            int errno_store = errno;
            file->flags &= ~UNCIL_IO_FILE_FLAG_DELETEONCLOSE;
            remove(file->tfn);
            unc_vmresume(w);
            unc0_mmfree(alloc, file->tfn);
            errno = errno_store;
        } else
            unc_vmresume(w);
        file->f = NULL;
    }
    return e;
}

int unc0_io_makeerr(Unc_View *w, const char *prefix, int err) {
    return unc0_std_makeerr(w, "io", prefix, err);
}

static Unc_RetVal unc0_io_file_destr(Unc_View *w, size_t n, void *data) {
    struct ulib_io_file *file = data;
    if (unc0_io_fclose_p(w, &w->world->alloc, file))
        return unc0_io_makeerr(w, "io.file.close()", errno);
    return 0;
}

int unc0_io_fwrap(Unc_View *w, Unc_Value *v, FILE *file, int write) {
    struct ulib_io_file *pfile;
    int e = unc_newopaque(w, v, &w->world->io_file,
                          sizeof(struct ulib_io_file), (void **)&pfile,
                          &unc0_io_file_destr, 0, NULL, 0, NULL);
    if (e) return e;
    pfile->f = file;
    pfile->encoding = UNCIL_IO_FILE_ENC_BINARY;
    pfile->encodingtext = UNCIL_IO_FILE_ENC_UTF8;
    pfile->flags = write ? UNCIL_IO_FILE_FLAG_WRITABLE
                         : UNCIL_IO_FILE_FLAG_READABLE;
    return 0;
}

int unc0_io_lockfile(Unc_View *w, Unc_Value *v,
                     struct ulib_io_file **pf, int ignoreerr) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    if (!(w->world->mmask & UNC_MMASK_M_IO))
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
    unc_getprototype(w, v, &p);
    if (unc_gettype(w, v) != Unc_TOpaque
            || !unc_issame(w, &p, &w->world->io_file)) {
        unc_clear(w, &p);
        if (ignoreerr) return 1;
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, v, NULL, (void **)&file);
    if (e) return e;
    if (!file) {
        ASSERT(0);
        if (ignoreerr) return 1;
        return unc_throwexc(w, "type", "argument is not a file");
    }
    *pf = file;
    return 0;
}

void unc0_io_unlockfile(Unc_View *w, Unc_Value *v) {
    unc_unlock(w, v);
}

Unc_RetVal unc0_lib_io_file_close(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    e = unc0_io_fclose_p(w, &w->world->alloc, file);
    unc_unlock(w, &args.values[0]);
    if (e)
        return e == EOF ? unc0_io_makeerr(w, "io.file.close()", errno) : e;
    return 0;
}

Unc_RetVal unc0_lib_io_file_seek(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    Unc_Int i;
    int e, origin = SEEK_SET;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    e = unc_getint(w, &args.values[1], &i);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (args.count >= 3 && unc_gettype(w, &args.values[2])) {
        Unc_Int os;
        Unc_Size sn;
        const char *sb;
        if (!unc_getint(w, &args.values[2], &os)) {
            if (os < 0 || os >= 3) {
                unc_unlock(w, &args.values[0]);
                return unc_throwexc(w, "value", "invalid seek origin");
            }
            origin = os;
        } else if (!unc_getstring(w, &args.values[2], &sn, &sb)) {
            if (sn == 3 && !memcmp(sb, "set", sn))
                origin = SEEK_SET;
            else if (sn == 3 && !memcmp(sb, "cur", sn))
                origin = SEEK_CUR;
            else if (sn == 3 && !memcmp(sb, "end", sn))
                origin = SEEK_END;
            else {
                unc_unlock(w, &args.values[0]);
                return unc_throwexc(w, "value", "invalid seek origin");
            }
        } else {
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "value", "invalid seek origin");
        }
    }
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
#ifdef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    e = unc0_io_fseek(file, i, origin, w);
#ifndef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    if (e) return unc0_io_makeerr(w, "io.file.seek()", errno);
    return 0;
}

Unc_RetVal unc0_lib_io_file_tell(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    Unc_Int i;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
#ifdef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    e = unc0_io_ftell(file, &i);
#ifndef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    if (e) return e == EOF ? unc0_io_makeerr(w, "io.file.tell()", errno) : e;
    unc_setint(w, &v, i);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_flush(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
#ifdef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    e = unc0_io_fflush_p(w, file);
#ifndef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    if (e) return unc0_io_makeerr(w, "io.file.flush()", errno);
    return 0;
}

Unc_RetVal unc0_lib_io_file_isopen(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    unc_setbool(w, &v, unc0_io_isopen(file));
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_iseof(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    unc_setbool(w, &v, unc0_io_feof(file));
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_canread(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
#if UNCIL_IS_POSIX
    {
        int fl = fcntl(fileno(file->f), F_GETFL);
        fl &= O_RDONLY | O_WRONLY | O_RDWR;
        unc_setbool(w, &v, fl == O_RDONLY || fl == O_RDWR);
    }
#else
    unc_setbool(w, &v, file->flags & UNCIL_IO_FILE_FLAG_READABLE);
#endif
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_canwrite(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
#if UNCIL_IS_POSIX
    {
        int fl = fcntl(fileno(file->f), F_GETFL);
        fl &= O_RDONLY | O_WRONLY | O_RDWR;
        unc_setbool(w, &v, fl == O_WRONLY || fl == O_RDWR);
    }
#else
    unc_setbool(w, &v, file->flags & UNCIL_IO_FILE_FLAG_WRITABLE);
#endif
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_canseek(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
#if UNCIL_IS_POSIX
    unc_setbool(w, &v, lseek(fileno(file->f), 0, SEEK_CUR) == (off_t)-1);
#else
    if (file->flags & UNCIL_IO_FILE_FLAG_TESTSEEKABLE) {
        file->flags &= ~(UNCIL_IO_FILE_FLAG_SEEKABLE
                        | UNCIL_IO_FILE_FLAG_TESTSEEKABLE);
        if (ftell(file->f) != -1L)
            file->flags |= UNCIL_IO_FILE_FLAG_SEEKABLE;
    }
    unc_setbool(w, &v, file->flags & UNCIL_IO_FILE_FLAG_SEEKABLE);
#endif
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_getbinary(Unc_View *w, Unc_Tuple args,
                                      void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    unc_setbool(w, &v, file->encoding == UNCIL_IO_FILE_ENC_BINARY);
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_getencoding(Unc_View *w, Unc_Tuple args,
                                        void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    e = unc_newstringc(w, &v, "utf8");
    unc_unlock(w, &args.values[0]);
    if (e)
        return e;
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_setbinary(Unc_View *w, Unc_Tuple args,
                                        void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    e = unc_getbool(w, &args.values[1], 0);
    if (UNCIL_IS_ERR(e)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "type", "encoding must be a string");
    }
    file->encoding = e ? UNCIL_IO_FILE_ENC_BINARY : file->encodingtext;
    unc_unlock(w, &args.values[0]);
    return 0;
}

Unc_RetVal unc0_lib_io_file_setencoding(Unc_View *w, Unc_Tuple args,
                                        void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    Unc_Size sn;
    const char *sb;
    int e, ec;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    e = unc_getstring(w, &args.values[1], &sn, &sb);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "type", "encoding must be a string");
    }
    ec = unc0_resolveencindex(w, sn, (const byte *)sb);
    if (ec < 0)
        return unc_throwexc(w, "value", "unrecognized or unsupported encoding");
    file->encodingtext = ec ? -ec : UNCIL_IO_FILE_ENC_UTF8;
    if (file->encoding != UNCIL_IO_FILE_ENC_BINARY)
        file->encoding = file->encodingtext;
    unc_unlock(w, &args.values[0]);
    return 0;
}

static int unc0_io_fgetc__(void *v) {
    return unc0_io_fgetc((struct ulib_io_file *)v);
}

struct unc_woutwrap {
    Unc_Allocator *alloc;
    byte *b;
    Unc_Size n, c;
    int fail;
};

static int unc0_io_rutf8_w_(void *v, Unc_Size n, const byte *b) {
    struct unc_woutwrap *w = v;
    int e;
#if UNCIL_CONVERT_CRLF
    if (w->n && n == 1 && b[0] == '\n' && w->b[w->n - 1] == '\r') {
        w->b[w->n - 1] = '\n';
        return -1;
    }
#endif
    if (!w->alloc) {
        if (w->n + n > w->c) {
            ASSERT(0);
            w->fail = UNCIL_ERR_MEM;
            return 1;
        }
        unc0_memcpy(w->b + w->n, b, n);
        w->n += n;
        return 0;
    }
    e = unc0_strpush(w->alloc, &w->b, &w->n, &w->c, 6, n, b);
    if (e) {
        w->fail = e;
        return 1;
    }
    return 0;
}

static int unc0_io_rutf8_wn_(void *v, Unc_Size n, const byte *b) {
    struct unc_woutwrap *w = v;
    int e;
#if UNCIL_CONVERT_CRLF
    if (w->n && n == 1 && b[0] == '\n' && w->b[w->n - 1] == '\r') {
        w->b[w->n - 1] = '\n';
        return -1;
    }
#endif
    e = unc0_strpush(w->alloc, &w->b, &w->n, &w->c, 6, n, b);
    if (e) {
        w->fail = e;
        return 1;
    }
    return n == 1 && b[0] == '\n';
}

#if UNCIL_CONVERT_CRLF
static const char newline[2] = "\r\n";
int unc0_io_fwrite_convnl(struct Unc_View *w, struct ulib_io_file *file,
                          const byte *b, Unc_Size *pn) {
    Unc_Size n = *pn;
    const byte *s = b, *x, *d = s + n;
    while ((n = memchr(s, '\n', d - s))) {
        if (unc0_io_fwrite_p(w, file, s, n - s))
            return 1;
        if (unc0_io_fwrite_p(w, file, (const byte *)newline, 2))
            return 1;
        s = n + 1;
        *++pn;
    }
    return unc0_io_fwrite_p(w, file, s, d - s);
}
#endif

struct uncdec_in_buffer {
    Unc_Size n;
    const byte *s;
};

struct uncdec_out_buffer {
    struct ulib_io_file *file;
    Unc_Size n;
    Unc_View *view;
};

static int uncdec_in_wrapper(void *data) {
    struct uncdec_in_buffer *buf = data;
    if (!buf->n) return -1;
    --buf->n;
    return *buf->s++;
}

static int uncdec_out_wrapper(void *data, Unc_Size n, const byte *b) {
    int e;
    struct uncdec_out_buffer *buf = data;
    if (buf->n + n >= UNC_INT_MAX - 1) return -1;
#if UNCIL_CONVERT_CRLF
    e = unc0_io_fwrite_convnl(buf->view, buf->file, b, &n);
#else
    e = unc0_io_fwrite_p(buf->view, buf->file, b, n);
#endif
    if (!e) buf->n += n;
    return 0;
}

int unc0_io_fgetc_text(Unc_View *w, struct ulib_io_file *file, char *buffer) {
    switch (file->encoding) {
    case UNCIL_IO_FILE_ENC_BINARY:
        return unc0_io_fread_p(w, file, (byte *)buffer, 0, 1) ? -1 : 1;
    case UNCIL_IO_FILE_ENC_UTF8:
    {
        struct unc_woutwrap wo;
        wo.alloc = NULL;
        wo.b = (byte *)buffer;
        wo.n = 0;
        wo.c = UNC_IO_GETC_BUFFER;
        wo.fail = 0;
        if (unc0_cconv_utf8ts(&unc0_io_fgetc__, file,
                              &unc0_io_rutf8_w_, &wo, 1)) {
            return (wo.fail || unc0_io_ferror(file) || unc0_io_feof(file))
                    ? -1 : -2;
        }
        if (!wo.n && unc0_io_feof(file)) return -1;
        return wo.n;
    }
    default:
        if (file->encoding < 0) {
            Unc_CConv_Dec dec = unc0_getbyencindex(w, -file->encoding)->dec;
            struct unc_woutwrap wo;
            wo.alloc = NULL;
            wo.b = (byte *)buffer;
            wo.n = 0;
            wo.c = UNC_IO_GETC_BUFFER;
            wo.fail = 0;
            if ((*dec)(&unc0_io_fgetc__, file, &unc0_io_rutf8_w_, &wo, 1)) {
                return (wo.fail || unc0_io_ferror(file) || unc0_io_feof(file))
                        ? -1 : -2;
            }
            if (!wo.n && unc0_io_feof(file)) return -1;
            return wo.n;
        }
        NEVER();
    }
}

int unc0_io_fwrite_text(Unc_View *w, struct ulib_io_file *file,
                        const byte *b, Unc_Size n) {
    switch (file->encoding) {
    case UNCIL_IO_FILE_ENC_BINARY:
        return unc0_io_fwrite_p(w, file, b, n);
    case UNCIL_IO_FILE_ENC_UTF8:
#if UNCIL_CONVERT_CRLF
        return unc0_io_fwrite_convnl(w, file, (const byte *)b, &n);
#else
        return unc0_io_fwrite_p(w, file, (const byte *)b, n);
#endif
    default:
        if (file->encoding < 0) {
            Unc_CConv_Enc enc = unc0_getbyencindex(w, -file->encoding)->enc;
            struct uncdec_in_buffer ibuf;
            struct uncdec_out_buffer obuf;
            ibuf.n = n;
            ibuf.s = b;
            obuf.file = file;
            obuf.n = 0;
            obuf.view = w;
            if ((*enc)(&uncdec_in_wrapper, &ibuf, &uncdec_out_wrapper, &obuf)) {
                return unc0_io_ferror(file) ? 1 : -1;
            }
        }
        NEVER();
    }
}

Unc_RetVal unc0_lib_io_file_read(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    Unc_Int i;
    int e;
    size_t sz;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    e = unc_getint(w, &args.values[1], &i);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (i < 0) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "value", "read size cannot be negative");
    }
    if (i > SIZE_MAX) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "system", "read size too large");
    }
    sz = (size_t)i;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
    if (unc0_io_feof(file)) {
        unc_setnull(w, &p);
        unc_unlock(w, &args.values[0]);
        return unc_pushmove(w, &p, NULL);
    }
    switch (file->encoding) {
    case UNCIL_IO_FILE_ENC_BINARY:
    {
        byte *j = unc_malloc(w, sz);
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        e = unc0_io_fread_p(w, file, j, 0, sz);
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (e)
            return e == EOF ? unc0_io_makeerr(w, "io.file.read()", errno) : e;
        e = unc_newblobmove(w, &p, j);
        if (e) {
            unc_mfree(w, j);
            return e;
        }
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    case UNCIL_IO_FILE_ENC_UTF8:
    {
        struct unc_woutwrap wo;
        wo.alloc = &w->world->alloc;
        wo.b = NULL;
        wo.n = wo.c = 0;
        wo.fail = 0;
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (unc0_cconv_utf8ts(&unc0_io_fgetc__, file,
                              &unc0_io_rutf8_w_, &wo, sz)) {
            if (unc0_io_ferror(file)) {
                unc_unlock(w, &args.values[0]);
                return unc0_io_makeerr(w, "io.file.read()", errno);
            } else if (unc0_io_feof(file)) {
                if (!wo.n) {
                    unc_unlock(w, &args.values[0]);
                    unc_setnull(w, &p);
                    return unc_pushmove(w, &p, NULL);
                }
            } else {
                unc_unlock(w, &args.values[0]);
                return wo.fail ? wo.fail
                    : unc_throwexc(w, "encoding",
                        "invalid encoding on read");
            }
        }
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (!wo.b && unc0_io_feof(file)) {
            unc_setnull(w, &p);
            return unc_pushmove(w, &p, NULL);
        }
        e = unc_newstringmove(w, &p, wo.n, (char *)wo.b);
        if (e) {
            unc0_mmfree(wo.alloc, wo.b);
            return e;
        }
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    default:
        if (file->encoding < 0) {
            Unc_CConv_Dec dec = unc0_getbyencindex(w, -file->encoding)->dec;
            struct unc_woutwrap wo;
            wo.alloc = &w->world->alloc;
            wo.b = NULL;
            wo.n = wo.c = 0;
            wo.fail = 0;
#ifdef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if ((*dec)(&unc0_io_fgetc__, file, &unc0_io_rutf8_w_, &wo, sz)) {
                if (unc0_io_ferror(file)) {
                    unc_unlock(w, &args.values[0]);
                    return unc0_io_makeerr(w, "io.file.read()", errno);
                } else if (unc0_io_feof(file)) {
                    if (!wo.n) {
                        unc_unlock(w, &args.values[0]);
                        unc_setnull(w, &p);
                        return unc_pushmove(w, &p, NULL);
                    }
                } else {
                    unc_unlock(w, &args.values[0]);
                    return wo.fail ? wo.fail
                        : unc_throwexc(w, "encoding",
                            "invalid encoding on read");
                }
            }
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if (!wo.b && unc0_io_feof(file)) {
                unc_setnull(w, &p);
                return unc_pushmove(w, &p, NULL);
            }
            e = unc_newstringmove(w, &p, wo.n, (char *)wo.b);
            if (e) {
                unc0_mmfree(wo.alloc, wo.b);
                return e;
            }
            e = unc_pushmove(w, &p, NULL);
            break;
        }
        NEVER();
    }
    return e;
}

Unc_RetVal unc0_lib_io_file_readline(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
    if (unc0_io_feof(file)) {
        unc_unlock(w, &args.values[0]);
        unc_setnull(w, &p);
        return unc_pushmove(w, &p, NULL);
    }
    switch (file->encoding) {
    case UNCIL_IO_FILE_ENC_BINARY:
    {
        byte *j = NULL, jt;
        size_t jn = 0, jc = 0;
        int c;
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        for (;;) {
            c = unc0_io_fgetc(file);
            if (c < 0) {
                if (unc0_io_ferror(file)) {
                    unc_unlock(w, &args.values[0]);
                    return unc0_io_makeerr(w, "io.file.readline()", errno);
                }
                /* EOF */
                if (!j) {
                    unc_unlock(w, &args.values[0]);
                    unc_setnull(w, &p);
                    return unc_pushmove(w, &p, NULL);
                }
                break;
            }
            jt = c;
            if ((e = unc0_strpushb(&w->world->alloc, &j, &jn, &jc,
                                   6, 1, &jt))) {
                unc_unlock(w, &args.values[0]);
                return e;
            }
            if (c == '\n')
                break;
        }
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        j = unc_mrealloc(w, j, jn);
        e = unc_newblobmove(w, &p, j);
        if (e) {
            unc_mfree(w, j);
            return e;
        }
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    case UNCIL_IO_FILE_ENC_UTF8:
    {
        struct unc_woutwrap wo;
        wo.alloc = &w->world->alloc;
        wo.b = NULL;
        wo.n = wo.c = 0;
        wo.fail = 0;
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (unc0_cconv_utf8ts(&unc0_io_fgetc__, file, &unc0_io_rutf8_wn_, &wo,
                                                        UNC_SIZE_MAX)) {
            if (unc0_io_ferror(file)) {
                unc_unlock(w, &args.values[0]);
                return unc0_io_makeerr(w, "io.file.readline()", errno);
            } else if (unc0_io_feof(file)) {
                if (!wo.n) {
                    unc_unlock(w, &args.values[0]);
                    unc_setnull(w, &p);
                    return unc_pushmove(w, &p, NULL);
                }
            } else {
                unc_unlock(w, &args.values[0]);
                return wo.fail ? wo.fail
                    : unc_throwexc(w, "encoding", "invalid encoding on read");
            }
        }
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (!wo.b && unc0_io_feof(file)) {
            unc_setnull(w, &p);
            return unc_pushmove(w, &p, NULL);
        }
        e = unc_newstringmove(w, &p, wo.n, (char *)wo.b);
        if (e) {
            unc0_mmfree(wo.alloc, wo.b);
            return e;
        }
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    default:
        if (file->encoding < 0) {
            Unc_CConv_Dec dec = unc0_getbyencindex(w, -file->encoding)->dec;
            struct unc_woutwrap wo;
            wo.alloc = &w->world->alloc;
            wo.b = NULL;
            wo.n = wo.c = 0;
            wo.fail = 0;
#ifdef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if ((*dec)(&unc0_io_fgetc__, file, &unc0_io_rutf8_wn_, &wo,
                                                            UNC_SIZE_MAX)) {
                if (unc0_io_ferror(file)) {
                    unc_unlock(w, &args.values[0]);
                    return unc0_io_makeerr(w, "io.file.readline()", errno);
                } else if (unc0_io_feof(file)) {
                    if (!wo.n) {
                        unc_unlock(w, &args.values[0]);
                        unc_setnull(w, &p);
                        return unc_pushmove(w, &p, NULL);
                    }
                } else {
                    unc_unlock(w, &args.values[0]);
                    return wo.fail ? wo.fail
                        : unc_throwexc(w, "encoding",
                            "invalid encoding on read");
                }
            }
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if (!wo.b && unc0_io_feof(file)) {
                unc_setnull(w, &p);
                return unc_pushmove(w, &p, NULL);
            }
            e = unc_newstringmove(w, &p, wo.n, (char *)wo.b);
            if (e) {
                unc0_mmfree(wo.alloc, wo.b);
                return e;
            }
            e = unc_pushmove(w, &p, NULL);
            break;
        }
        NEVER();
    }
    return e;
}

Unc_RetVal unc0_lib_io_file_readall(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
    if (unc0_io_feof(file)) {
        unc_unlock(w, &args.values[0]);
        unc_setnull(w, &p);
        return unc_pushmove(w, &p, NULL);
    }
    switch (file->encoding) {
    case UNCIL_IO_FILE_ENC_BINARY:
    {
        byte *j = NULL, jt;
        size_t jn = 0, jc = 0;
        int c;
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        for (;;) {
            c = unc0_io_fgetc(file);
            if (c < 0) {
                if (unc0_io_ferror(file)) {
                    unc_unlock(w, &args.values[0]);
                    return unc0_io_makeerr(w, "io.file.readall()", errno);
                }
                /* EOF */
                if (!j) {
                    unc_unlock(w, &args.values[0]);
                    unc_setnull(w, &p);
                    return unc_pushmove(w, &p, NULL);
                }
                break;
            }
            jt = c;
            if ((e = unc0_strpushb(&w->world->alloc, &j, &jn, &jc,
                                    6, 1, &jt))) {
                unc_unlock(w, &args.values[0]);
                return e;
            }
        }
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        j = unc_mrealloc(w, j, jn);
        e = unc_newblobmove(w, &p, j);
        if (e) {
            unc_mfree(w, j);
            return e;
        }
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    case UNCIL_IO_FILE_ENC_UTF8:
    {
        struct unc_woutwrap wo;
        wo.alloc = &w->world->alloc;
        wo.b = NULL;
        wo.n = wo.c = 0;
        wo.fail = 0;
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (unc0_cconv_utf8ts(&unc0_io_fgetc__, file, &unc0_io_rutf8_w_, &wo,
                                                        UNC_SIZE_MAX)) {
            if (unc0_io_ferror(file)) {
                unc_unlock(w, &args.values[0]);
                return unc0_io_makeerr(w, "io.file.readall()", errno);
            } else if (unc0_io_feof(file)) {
                if (!wo.n) {
                    unc_unlock(w, &args.values[0]);
                    unc_setnull(w, &p);
                    return unc_pushmove(w, &p, NULL);
                }
            } else {
                unc_unlock(w, &args.values[0]);
                return wo.fail ? wo.fail
                    : unc_throwexc(w, "encoding", "invalid encoding on read");
            }
        }
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (!wo.b && unc0_io_feof(file)) {
            unc_setnull(w, &p);
            return unc_pushmove(w, &p, NULL);
        }
        e = unc_newstringmove(w, &p, wo.n, (char *)wo.b);
        if (e) {
            unc0_mmfree(wo.alloc, wo.b);
            return e;
        }
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    default:
        if (file->encoding < 0) {
            Unc_CConv_Dec dec = unc0_getbyencindex(w, -file->encoding)->dec;
            struct unc_woutwrap wo;
            wo.alloc = &w->world->alloc;
            wo.b = NULL;
            wo.n = wo.c = 0;
            wo.fail = 0;
#ifdef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if ((*dec)(&unc0_io_fgetc__, file, &unc0_io_rutf8_w_, &wo,
                                                            UNC_SIZE_MAX)) {
                if (unc0_io_ferror(file)) {
                    unc_unlock(w, &args.values[0]);
                    return unc0_io_makeerr(w, "io.file.readall()", errno);
                } else if (unc0_io_feof(file)) {
                    if (!wo.n) {
                        unc_unlock(w, &args.values[0]);
                        unc_setnull(w, &p);
                        return unc_pushmove(w, &p, NULL);
                    }
                } else {
                    unc_unlock(w, &args.values[0]);
                    return wo.fail ? wo.fail
                        : unc_throwexc(w, "encoding",
                            "invalid encoding on read");
                }
            }
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if (!wo.b && unc0_io_feof(file)) {
                unc_setnull(w, &p);
                return unc_pushmove(w, &p, NULL);
            }
            e = unc_newstringmove(w, &p, wo.n, (char *)wo.b);
            if (e) {
                unc0_mmfree(wo.alloc, wo.b);
                return e;
            }
            e = unc_pushmove(w, &p, NULL);
            break;
        }
        NEVER();
    }
    return e;
}

Unc_RetVal unc0_lib_io_file_write(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
    switch (file->encoding) {
    case UNCIL_IO_FILE_ENC_BINARY:
    {
        Unc_Size sz;
        byte *b;
        e = unc_lockblob(w, &args.values[1], &sz, &b);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "type", "must write blob to "
                                           "binary file");
        }
        if (sz > UNC_INT_MAX) {
            unc_unlock(w, &args.values[1]);
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "system", "write size too large");
        }
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (unc0_io_fwrite_p(w, file, b, sz)) {
            unc_unlock(w, &args.values[1]);
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            return unc0_io_makeerr(w, "io.file.write()", errno);
        }
        unc_unlock(w, &args.values[1]);
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        unc_setint(w, &p, sz);
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    case UNCIL_IO_FILE_ENC_UTF8:
    {
        Unc_Size sz;
        const char *b;
        e = unc_getstring(w, &args.values[1], &sz, &b);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "type", "must write string to "
                                           "text file");
        }
        if (sz > UNC_INT_MAX) {
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "system", "write size too large");
        }
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
#if UNCIL_CONVERT_CRLF
        if (unc0_io_fwrite_convnl(w, file, (const byte *)b, &sz)) {
#else
        if (unc0_io_fwrite_p(w, file, (const byte *)b, sz)) {
#endif
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            return unc0_io_makeerr(w, "io.file.write()", errno);
        }
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        unc_setint(w, &p, sz);
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    default:
        if (file->encoding < 0) {
            Unc_CConv_Enc enc = unc0_getbyencindex(w, -file->encoding)->enc;
            struct uncdec_in_buffer ibuf;
            struct uncdec_out_buffer obuf;
            Unc_Size sz;
            const char *b;
            e = unc_getstring(w, &args.values[1], &sz, &b);
            if (e) {
                unc_unlock(w, &args.values[0]);
                return unc_throwexc(w, "type", "must write string to "
                                               "text file");
            }
            ibuf.n = sz;
            ibuf.s = (const byte *)b;
            obuf.file = file;
            obuf.n = 0;
            obuf.view = w;
#ifdef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if ((*enc)(&uncdec_in_wrapper, &ibuf, &uncdec_out_wrapper, &obuf)) {
                unc_unlock(w, &args.values[0]);
                if (unc0_io_ferror(file)) {
                    return unc0_io_makeerr(w, "io.file.write()", errno);
                } else {
                    return unc_throwexc(w, "encoding",
                            "cannot encode string with current encoding");
                }
            }
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            unc_setint(w, &p, obuf.n);
            e = unc_pushmove(w, &p, NULL);
            break;
        }
        NEVER();
    }
    return e;
}

Unc_RetVal unc0_lib_io_file_writeline(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
    switch (file->encoding) {
    case UNCIL_IO_FILE_ENC_BINARY:
    {
        Unc_Size sz;
        byte *b;
        e = unc_lockblob(w, &args.values[1], &sz, &b);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "type", "must write blob to "
                                           "binary file");
        }
        if (sz > UNC_INT_MAX) {
            unc_unlock(w, &args.values[1]);
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "system", "write size too large");
        }
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        if (unc0_io_fwrite_p(w, file, b, sz)) {
            unc_unlock(w, &args.values[1]);
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            return unc0_io_makeerr(w, "io.file.writeline()", errno);
        }
        unc_unlock(w, &args.values[1]);
        if (unc0_io_fwrite_p(w, file, (const byte *)"\n", 1)) {
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            return unc0_io_makeerr(w, "io.file.writeline()", errno);
        }
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        sz += 1;
        unc_setint(w, &p, sz);
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    case UNCIL_IO_FILE_ENC_UTF8:
    {
        Unc_Size sz;
        const char *b;
        e = unc_getstring(w, &args.values[1], &sz, &b);
        if (e) {
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "type", "must write string to "
                                           "text file");
        }
        if (sz > UNC_INT_MAX) {
            unc_unlock(w, &args.values[0]);
            return unc_throwexc(w, "system", "write size too large");
        }
#ifdef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
#if UNCIL_CONVERT_CRLF
        if (unc0_io_fwrite_convnl(w, file, (const byte *)b, &sz)) {
            unc_unlock(w, &args.values[0]);
            return unc0_io_makeerr(w, "io.file.writeline()", errno);
        }
        if (unc0_io_fwrite_convnl(w, file, (const byte *)newline,
                                    sizeof(newline) - 1)) {
            unc_unlock(w, &args.values[0]);
            return unc0_io_makeerr(w, "io.file.writeline()", errno);
        }
        sz += sizeof(newline) - 1;
#else
        if (unc0_io_fwrite_p(w, file, (const byte *)b, sz)) {
            unc_unlock(w, &args.values[0]);
            return unc0_io_makeerr(w, "io.file.writeline()", errno);
        }
        if (unc0_io_fwrite_p(w, file, (const byte *)"\n", 1)) {
            unc_unlock(w, &args.values[0]);
            return unc0_io_makeerr(w, "io.file.writeline()", errno);
        }
        sz += 1;
#endif
#ifndef STDIO_FILE_THREADSAFE
        unc_unlock(w, &args.values[0]);
#endif
        unc_setint(w, &p, sz);
        e = unc_pushmove(w, &p, NULL);
        break;
    }
    default:
        if (file->encoding < 0) {
            Unc_CConv_Enc enc = unc0_getbyencindex(w, -file->encoding)->enc;
            struct uncdec_in_buffer ibuf;
            struct uncdec_out_buffer obuf;
            Unc_Size sz;
            const char *b;
            e = unc_getstring(w, &args.values[1], &sz, &b);
            if (e) {
                unc_unlock(w, &args.values[0]);
                return unc_throwexc(w, "type", "must write string to "
                                               "text file");
            }
            ibuf.n = sz;
            ibuf.s = (const byte *)b;
            obuf.file = file;
            obuf.n = 0;
            obuf.view = w;
#ifdef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            if ((*enc)(&uncdec_in_wrapper, &ibuf, &uncdec_out_wrapper, &obuf)) {
                unc_unlock(w, &args.values[0]);
                if (unc0_io_ferror(file)) {
                    return unc0_io_makeerr(w, "io.file.write()", errno);
                } else {
                    return unc_throwexc(w, "encoding",
                            "cannot encode string with current encoding");
                }
            } else {
#if UNCIL_CONVERT_CRLF
                if (unc0_io_fwrite_conv(w, file, (const byte *)newline,
                                            sizeof(newline) - 1)) {
                    unc_unlock(w, &args.values[0]);
                    return unc0_io_makeerr(w, "io.file.writeline()", errno);
                }
                obuf.n += sizeof(newline) - 1;
#else
                if (unc0_io_fwrite_p(w, file, (const byte *)"\n", 1)) {
                    unc_unlock(w, &args.values[0]);
                    return unc0_io_makeerr(w, "io.file.writeline()", errno);
                }
                obuf.n += 1;
#endif
            }
#ifndef STDIO_FILE_THREADSAFE
            unc_unlock(w, &args.values[0]);
#endif
            unc_setint(w, &p, obuf.n);
            e = unc_pushmove(w, &p, NULL);
            break;
        }
        NEVER();
    }
    return e;
}

Unc_RetVal unc0_lib_io_file_readbyte(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
    if (unc0_io_feof(file)) {
        unc_unlock(w, &args.values[0]);
        unc_setint(w, &p, -1);
        return unc_pushmove(w, &p, NULL);
    }
    if (file->encoding != UNCIL_IO_FILE_ENC_BINARY) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io",
            "readbyte() can only be used with files opened in binary mode");
    }
#ifdef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    e = unc0_io_fgetc(file);
    if (e == EOF) {
        if (unc0_io_ferror(file))
            return unc0_io_makeerr(w, "io.file.readbyte()", errno);
        e = -1;
    }
#ifndef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    unc_setint(w, &p, e);
    e = unc_pushmove(w, &p, NULL);
    return e;
}

Unc_RetVal unc0_lib_io_file_writebyte(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    byte b;
    Unc_Int i;
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e)
        return e;
    if (!unc0_io_isopen(file)) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io", "file is closed");
    }
    if (file->encoding != UNCIL_IO_FILE_ENC_BINARY) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "io",
            "writebyte() can only be used with files opened in binary mode");
    }
    e = unc_getint(w, &args.values[1], &i);
    if (e) {
        unc_unlock(w, &args.values[0]);
        return e;
    }
    if (i < 0 || i > UCHAR_MAX) {
        unc_unlock(w, &args.values[0]);
        return unc_throwexc(w, "value", "value is not a valid byte");
    }
    b = (byte)i;
#ifdef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    e = unc0_io_fwrite_p(w, file, &b, 1);
#ifndef STDIO_FILE_THREADSAFE
    unc_unlock(w, &args.values[0]);
#endif
    if (e)
        return unc0_io_makeerr(w, "io.file.writebyte()", errno);
    unc_setint(w, &p, 1);
    e = unc_pushmove(w, &p, NULL);
    return e;
}

Unc_RetVal unc0_lib_io_file_lines_next(Unc_View *w,
                                       Unc_Tuple args, void *udata) {
    int e;
    Unc_Value *fd, *readl;
    Unc_Pile pile;
    Unc_Tuple tuple;

    (void)udata;
    ASSERT(unc_boundcount(w) == 2);
    fd = unc_boundvalue(w, 0);
    readl = unc_boundvalue(w, 1);

    e = unc_push(w, 1, fd, NULL);
    if (e) return e;
    e = unc_call(w, readl, 1, &pile);
    if (e) return e;
    unc_returnvalues(w, &pile, &tuple);
    if (tuple.count != 1 || !unc_gettype(w, &tuple.values[0]))
        unc_discard(w, &pile);
    return 0;
}

Unc_RetVal unc0_lib_io_file_lines(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_Value v = UNC_BLANK, p = UNC_BLANK;
    struct ulib_io_file *file;
    int e;
    Unc_Size refcopies[] = { 1 };
    unc_getprototype(w, &args.values[0], &p);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &p, unc_boundvalue(w, 0))) {
        unc_clear(w, &p);
        return unc_throwexc(w, "type", "argument is not a file");
    }
    unc_clear(w, &p);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&file);
    if (e) return e;
    e = unc_newcfunction(w, &v, &unc0_lib_io_file_lines_next,
                         UNC_CFUNC_DEFAULT, 0, 0, 0, NULL, 1, &args.values[0],
                         1, refcopies, "io.file.lines.next", NULL);
    unc_unlock(w, &args.values[0]);
    if (e) return e;
    return unc_pushmove(w, &v, NULL);
}

Unc_RetVal unc0_lib_io_file_open(Unc_View *w, Unc_Tuple args, void *udata) {
    int e, flags, fflags = 0, bufspec = 0, bufmode;
    size_t mn, bufsize;
    const char *sb, *mb;
    char mode[8], *mp;
    struct ulib_io_file file, *pfile;
    Unc_Value result = UNC_BLANK;
    if (unc_gettype(w, &args.values[3])) {
        Unc_Int ui;
        bufspec = 1;
        e = unc_getstring(w, &args.values[3], &mn, &mb);
        if (!e) {
            if (mn == 4 && !memcmp(sb, "line", 4)) {
                bufmode = _IOLBF;
                bufsize = BUFSIZ;
            } else {
                return unc_throwexc(w, "value", "unrecognized buffer mode");
            }
        } else {
            e = unc_getint(w, &args.values[3], &ui);
            if (e)
                return unc_throwexc(w, "type",
                    "buffer argument must be a string or integer");
            if (ui < 0)
                return unc_throwexc(w, "value",
                    "buffer size cannot be negative");
            if (ui > SIZE_MAX)
                return unc_throwexc(w, "system", "buffer size too large");
            bufmode = ui ? _IOFBF : _IONBF;
            bufsize = (size_t)ui;
        }
    }
    e = unc_getstringc(w, &args.values[0], &sb);
    if (e) return e;
    e = unc_getstring(w, &args.values[1], &mn, &mb);
    if (e) return e;

    if (!mn)
        return unc_throwexc(w, "value", "invalid mode");
    mp = mode;
    e = *mp++ = *mb++;
    --mn;
    switch (e) {
    case 'r':
        fflags |= UNCIL_IO_FILE_FLAG_READABLE;
        break;
    case 'w':
    case 'a':
        fflags |= UNCIL_IO_FILE_FLAG_WRITABLE;
        break;
    default:
        return unc_throwexc(w, "value", "invalid mode");
    }

    flags = 0;
    while (mn) {
        if (*mb == 'b') {
            if (flags & 1)
                return unc_throwexc(w, "value", "invalid mode");
            flags |= 1;
            --mn;
        } else if (*mb == '+') {
            if (flags & 2)
                return unc_throwexc(w, "value", "invalid mode");
            flags |= 2;
            *mp++ = *mb++;
            fflags |= UNCIL_IO_FILE_FLAG_READABLE | UNCIL_IO_FILE_FLAG_WRITABLE;
            --mn;
        } else if (*mb == 'x') {
            if (flags & 4 || *mode == 'r')
                return unc_throwexc(w, "value", "invalid mode");
            flags |= 4;
#if UNCIL_C11
            *mp++ = *mb++;
#endif
            --mn;
        } else
            return unc_throwexc(w, "value", "invalid mode");
    }
    *mp++ = 'b';
    *mp++ = 0;

#if !UNCIL_IS_POSIX
    fflags |= UNCIL_IO_FILE_FLAG_TESTSEEKABLE;
#endif

#if !UNCIL_C11
    if (flags & 4) { /* simulate 'x' */
        e = unc0_io_fopen(&file, sb, "rb");
        if (!e) {
            unc0_io_fclose_p(w, &w->world->alloc, &file);
            return unc_throwexc(w, "io",
                "x specified, but file already exists");
        }
    }
#endif
    file.flags = 0;
    if (unc_gettype(w, &args.values[2])) {
        int ec;
        e = unc_getstring(w, &args.values[2], &mn, &mb);
        if (e)
            return unc_throwexc(w, "type", "encoding must be null or a string");
        ec = unc0_resolveencindex(w, mn, (const byte *)mb);
        if (ec < 0)
            return unc_throwexc(w, "value", "unknown or unsupported encoding");
        file.encodingtext = ec ? -ec : UNCIL_IO_FILE_ENC_UTF8;
    } else
        file.encodingtext = UNCIL_IO_FILE_ENC_UTF8;
    e = unc0_io_fopen(&file, sb, mode);
    if (e) return unc0_io_makeerr(w, "io.open()", errno);
    file.encoding = !(flags & 1) ? file.encodingtext
                                 : UNCIL_IO_FILE_ENC_BINARY;
    file.flags |= fflags;

    if (bufspec) {
        errno = 0;
        if ((e = unc0_io_setvbuf(&file, NULL, bufmode, bufsize))) {
            if (errno)
                e = unc0_io_makeerr(w, "io.open(), setvbuf()", errno);
            else
                e = unc_throwexc(w, "io",
                    "could not set buffer for opened file");
            unc0_io_fclose_p(w, &w->world->alloc, &file);
            return e;
        }
    }

    e = unc_newopaque(w, &result, unc_boundvalue(w, 0),
                        sizeof(struct ulib_io_file), (void **)&pfile,
                        &unc0_io_file_destr, 0, NULL, 0, NULL);
    if (e) {
        unc0_io_fclose_p(w, &w->world->alloc, &file);
        return e;
    }

    *pfile = file;
    unc_unlock(w, &result);
    e = unc_pushmove(w, &result, NULL);
    return e;
}

Unc_RetVal unc0_lib_io_file_tempfile(Unc_View *w, Unc_Tuple args, void *udata) {
    int e, flags, fflags = 0, bufspec = 0, bufmode, hastmpfilename;
    size_t mn, bufsize;
    const char *mb;
    char mode[8], *mp, tmpfilename[(TMPFILENAME_MAX + 1) < 12 ? 12
                                  : TMPFILENAME_MAX + 1];
    struct ulib_io_file file, *pfile;
    Unc_Value result = UNC_BLANK;
    if (unc_gettype(w, &args.values[3])) {
        Unc_Int ui;
        bufspec = 1;
        e = unc_getstring(w, &args.values[3], &mn, &mb);
        if (!e) {
            if (mn == 4 && !memcmp(mb, "line", 4)) {
                bufmode = _IOLBF;
                bufsize = BUFSIZ;
            } else {
                return unc_throwexc(w, "value", "unrecognized buffer mode");
            }
        } else {
            e = unc_getint(w, &args.values[3], &ui);
            if (e)
                return unc_throwexc(w, "type",
                    "buffer argument must be a string or integer");
            if (ui < 0)
                return unc_throwexc(w, "value",
                    "buffer size cannot be negative");
            if (ui > SIZE_MAX)
                return unc_throwexc(w, "system", "buffer size too large");
            bufmode = ui ? _IOFBF : _IONBF;
            bufsize = (size_t)ui;
        }
    }

    if (!unc_gettype(w, &args.values[1])) {
        fflags = UNCIL_IO_FILE_FLAG_READABLE | UNCIL_IO_FILE_FLAG_WRITABLE;
        flags = 3;
        strcpy(mode, "w+b");
    } else {
        e = unc_getstring(w, &args.values[1], &mn, &mb);
        if (e) return e;

        if (!mn)
            return unc_throwexc(w, "value", "invalid mode");
        mp = mode;
        e = *mp++ = *mb++;
        --mn;
        switch (e) {
        case 'r':
            fflags |= UNCIL_IO_FILE_FLAG_READABLE;
            break;
        case 'w':
        case 'a':
            fflags |= UNCIL_IO_FILE_FLAG_WRITABLE;
            break;
        default:
            return unc_throwexc(w, "value", "invalid mode");
        }

        flags = 0;
        while (mn) {
            if (*mb == 'b') {
                if (flags & 1)
                    return unc_throwexc(w, "value", "invalid mode");
                flags |= 1;
                --mn;
            } else if (*mb == '+') {
                if (flags & 2)
                    return unc_throwexc(w, "value", "invalid mode");
                flags |= 2;
                *mp++ = *mb++;
                fflags |= UNCIL_IO_FILE_FLAG_READABLE | UNCIL_IO_FILE_FLAG_WRITABLE;
                --mn;
            } else if (*mb == 'x') {
                if (flags & 4 || *mode == 'r')
                    return unc_throwexc(w, "value", "invalid mode");
                flags |= 4;
#if UNCIL_C11
                *mp++ = *mb++;
#endif
                --mn;
            } else
                return unc_throwexc(w, "value", "invalid mode");
        }
        *mp++ = 'b';
        *mp++ = 0;

#if !UNCIL_IS_POSIX
        fflags |= UNCIL_IO_FILE_FLAG_TESTSEEKABLE;
#endif
    }

    e = unc_getbool(w, &args.values[0], 0);
    if (UNCIL_IS_ERR(e)) return e;
    file.flags = 0;
    if (unc_gettype(w, &args.values[2])) {
        int ec;
        e = unc_getstring(w, &args.values[2], &mn, &mb);
        if (e)
            return unc_throwexc(w, "type", "encoding must be null or a string");
        ec = unc0_resolveencindex(w, mn, (const byte *)mb);
        if (ec < 0)
            return unc_throwexc(w, "value",
                "unrecognized or unsupported encoding");
        file.encodingtext = ec ? -ec : UNCIL_IO_FILE_ENC_UTF8;
    } else
        file.encodingtext = UNCIL_IO_FILE_ENC_UTF8;
    if ((hastmpfilename = e))
        e = unc0_io_tmpfilenamed(&w->world->alloc, &file, tmpfilename, mode);
    else
        e = unc0_io_tmpfile(&w->world->alloc, &file, mode);
    if (e) {
        if (e == TMPFILE_MEM)
            return UNCIL_ERR_MEM;
        if (e == TMPFILE_FAIL)
            return unc_throwexc(w, "io", "could not create temporary file");
        return unc0_io_makeerr(w, "io.tempfile()", errno);
    }
    file.encoding = !(flags & 1) ? file.encodingtext
                                 : UNCIL_IO_FILE_ENC_BINARY;
    file.flags |= fflags;

    if (bufspec) {
        errno = 0;
        if ((e = unc0_io_setvbuf(&file, NULL, bufmode, bufsize))) {
            if (errno)
                e = unc0_io_makeerr(w, "io.tempfile(), setvbuf()", errno);
            else
                e = unc_throwexc(w, "io",
                    "could not set buffer for opened file");
            unc0_io_fclose_p(w, &w->world->alloc, &file);
            return e;
        }
    }

    e = unc_newopaque(w, &result, unc_boundvalue(w, 0),
                        sizeof(struct ulib_io_file), (void **)&pfile,
                        &unc0_io_file_destr, 0, NULL, 0, NULL);
    if (e) {
        unc0_io_fclose_p(w, &w->world->alloc, &file);
        return e;
    }

    *pfile = file;
    unc_unlock(w, &result);
    e = unc_push(w, 1, &result, NULL);
    if (!e) {
        if (hastmpfilename)
            e = unc_newstringc(w, &result, tmpfilename);
        else
            unc_setnull(w, &result);
        e = unc_push(w, 1, &result, NULL);
    }
    unc_clear(w, &result);
    return e;
}

Unc_RetVal unc0_lib_io_file_pipe(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    struct ulib_io_file pipes[2], *pfile;
    Unc_Value result = UNC_BLANK;

    errno = 0;
    e = unc0_io_pipe(pipes);
    if (e) {
        if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            e = unc0_io_makeerr(w, "io.pipe()", errno);
        else
            return e;
    }

    pipes[0].encoding = UNCIL_IO_FILE_ENC_BINARY;
    pipes[0].encodingtext = UNCIL_IO_FILE_ENC_UTF8;
    pipes[0].flags = UNCIL_IO_FILE_FLAG_READABLE;
    
    pipes[1].encoding = UNCIL_IO_FILE_ENC_BINARY;
    pipes[1].encodingtext = UNCIL_IO_FILE_ENC_UTF8;
    pipes[1].flags = UNCIL_IO_FILE_FLAG_WRITABLE;
    
    e = unc_newopaque(w, &result, unc_boundvalue(w, 0),
                        sizeof(struct ulib_io_file), (void **)&pfile,
                        &unc0_io_file_destr, 0, NULL, 0, NULL);
    if (e) {
        unc0_io_fclose_p(w, &w->world->alloc, &pipes[1]);
        unc0_io_fclose_p(w, &w->world->alloc, &pipes[0]);
        return e;
    }
    *pfile = pipes[0];
    unc_unlock(w, &result);
    e = unc_push(w, 1, &result, NULL);
    if (e) {
        unc0_io_fclose_p(w, &w->world->alloc, &pipes[1]);
        unc_clear(w, &result);
        return e;
    }

    e = unc_newopaque(w, &result, unc_boundvalue(w, 0),
                        sizeof(struct ulib_io_file), (void **)&pfile,
                        &unc0_io_file_destr, 0, NULL, 0, NULL);
    if (e) {
        unc0_io_fclose_p(w, &w->world->alloc, &pipes[1]);
        return e;
    }
    *pfile = pipes[1];
    unc_unlock(w, &result);
    e = unc_push(w, 1, &result, NULL);
    unc_clear(w, &result);
    return e;
}

static int unc0_lib_io_filewrap(Unc_View *w, FILE *f,
                                Unc_Value *proto, Unc_Value *result,
                                int encoding, int flags) {
    struct ulib_io_file *pfile;
    int e = unc_newopaque(w, result, proto,
                        sizeof(struct ulib_io_file), (void **)&pfile,
                        &unc0_io_file_destr, 0, NULL, 0, NULL);
    if (e) return e;

    pfile->f = f;
    pfile->encoding = encoding;
    pfile->flags = flags;
    unc_unlock(w, result);
    return 0;
}

static int unc0_io_init_i(Unc_View *w) {
    int e;
    Unc_Value io_file = UNC_BLANK;

    e = unc_newobject(w, &io_file, NULL);
    if (e) return e;

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_close,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "close", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "close", &fn);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "__close", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_flush,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "flush", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "flush", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_seek,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 1, NULL,
                             1, &io_file, 0, NULL, "seek", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "seek", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_tell,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "tell", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "tell", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_isopen,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "isopen", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "isopen", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_iseof,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "iseof", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "iseof", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_canread,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "canread", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "canread", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_canwrite,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "canwrite", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "canwrite", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_canseek,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "canseek", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "canseek", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_getbinary,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "getbinary", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "getbinary", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_getencoding,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "getencoding", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "getencoding", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_setbinary,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &io_file, 0, NULL, "setbinary", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "setbinary", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_setencoding,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &io_file, 0, NULL, "setencoding", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "setencoding", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_read,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &io_file, 0, NULL, "read", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "read", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_write,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &io_file, 0, NULL, "write", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "write", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_readbyte,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "readbyte", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "readbyte", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_writebyte,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &io_file, 0, NULL, "writebyte", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "writebyte", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_readline,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "readline", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "readline", &fn);
        if (e) return e;
        {
            Unc_Value fn2 = UNC_BLANK;
            Unc_Value binds[2] = UNC_BLANKS;
            unc_copy(w, &binds[0], &io_file);
            unc_copy(w, &binds[1], &fn);
            e = unc_newcfunction(w, &fn2, &unc0_lib_io_file_lines,
                                UNC_CFUNC_CONCURRENT,
                                1, 0, 0, NULL,
                                2, binds, 0, NULL, "lines", NULL);
            if (e) return e;
            e = unc_setattrc(w, &io_file, "lines", &fn2);
            if (e) return e;
            unc_clearmany(w, 2, binds);
            unc_clear(w, &fn2);
        }
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_writeline,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &io_file, 0, NULL, "writeline", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "writeline", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_readall,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &io_file, 0, NULL, "readall", NULL);
        if (e) return e;
        e = unc_setattrc(w, &io_file, "readall", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value ns = UNC_BLANK;
        e = unc_newstringc(w, &ns, "io.file");
        if (e) return e;
        e = unc_setattrc(w, &io_file, "__name", &ns);
        if (e) return e;
        unc_clear(w, &ns);
    }

    VMOVE(w, &w->world->io_file, &io_file);
    return 0;
}

UNC_LOCKSTATICF(ioinitlock)
UNC_LOCKSTATICFINIT0(ioinitlock)

int unc0_io_init(Unc_View *w) {
    int e = 0;
    UNC_LOCKSTATICFINIT1(ioinitlock);
    UNC_LOCKF(ioinitlock);
    if (!VGETTYPE(&w->world->io_file))
        e = unc0_io_init_i(w);
    UNC_UNLOCKF(ioinitlock);
    return e;
}

Unc_RetVal uncilmain_io(Unc_View *w) {
    Unc_RetVal e;
    Unc_Value io_file;

    if ((e = unc0_io_init(w))) return e;
    io_file = VGETRAW(&w->world->io_file);

    e = unc_setpublicc(w, "file", &io_file);
    if (e) return e;

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_open,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 2, NULL,
                             1, &io_file, 0, NULL, "open", NULL);
        if (e) return e;    
        e = unc_setpublicc(w, "open", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_tempfile,
                             UNC_CFUNC_DEFAULT,
                             0, 0, 3, NULL,
                             1, &io_file, 0, NULL, "tempfile", NULL);
        if (e) return e;    
        e = unc_setpublicc(w, "tempfile", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value fn = UNC_BLANK;
        e = unc_newcfunction(w, &fn, &unc0_lib_io_file_pipe,
                             UNC_CFUNC_DEFAULT,
                             0, 0, 0, NULL,
                             1, &io_file, 0, NULL, "pipe", NULL);
        if (e) return e;    
        e = unc_setpublicc(w, "pipe", &fn);
        if (e) return e;
        unc_clear(w, &fn);
    }

    {
        Unc_Value s = UNC_BLANK;
        e = unc0_lib_io_filewrap(w, stdin, &io_file, &s,
                                 UNCIL_IO_FILE_ENC_UTF8,
                                 UNCIL_IO_FILE_FLAG_READABLE);
        if (e) return e;    
        e = unc_setpublicc(w, "stdin", &s);
        if (e) return e;
        unc_clear(w, &s);
#if UNCIL_IS_WINDOWS
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    }

    {
        Unc_Value s = UNC_BLANK;
        e = unc0_lib_io_filewrap(w, stdout, &io_file, &s,
                                 UNCIL_IO_FILE_ENC_UTF8,
                                 UNCIL_IO_FILE_FLAG_WRITABLE);
        if (e) return e;    
        e = unc_setpublicc(w, "stdout", &s);
        if (e) return e;
        unc_clear(w, &s);
#if UNCIL_IS_WINDOWS
        _setmode(_fileno(stdout), _O_BINARY);
#endif
    }

    {
        Unc_Value s = UNC_BLANK;
        e = unc0_lib_io_filewrap(w, stderr, &io_file, &s,
                                 UNCIL_IO_FILE_ENC_UTF8,
                                 UNCIL_IO_FILE_FLAG_WRITABLE);
        if (e) return e;    
        e = unc_setpublicc(w, "stderr", &s);
        if (e) return e;
        unc_clear(w, &s);
#if UNCIL_IS_WINDOWS
        _setmode(_fileno(stderr), _O_BINARY);
#endif
    }

    {
        Unc_Value v = UNC_BLANK;
#if UNCIL_CONVERT_CRLF
        char nbuf[3];
        nbuf[0] = '\r';
        nbuf[1] = '\n';
        nbuf[2] = 0;
        e = unc_newstringc(w, &v, nbuf);
#else
        char buf[2];
        buf[0] = '\n';
        buf[1] = 0;
        e = unc_newstringc(w, &v, buf);
#endif
        if (e) return e;
        e = unc_setpublicc(w, "newline", &v);
        if (e) return e;
    }

    return 0;
}
