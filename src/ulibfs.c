/*******************************************************************************
 
Uncil -- builtin fs library impl

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

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define UNCIL_DEFINES

#include "udef.h"
#include "uncil.h"
#include "uosdef.h"
#include "uvsio.h"

#if UNCIL_IS_POSIX
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#if __GNUC__
#if UNCIL_C99
#define RESTRICT restrict
#else
#define RESTRICT
#endif
char *realpath(const char *RESTRICT path, char *RESTRICT resolved_path);
#endif
#elif UNCIL_IS_WINDOWS
#include <Windows.h>
#endif

static int unc__fs_makeerr(Unc_View *w, const char *prefix, int err) {
    return unc__std_makeerr(w, "system", prefix, err);
}

static int unc__ansiexists(const char *fn) {
    FILE *fd = fopen(fn, "rb");
    if (fd) {
        fclose(fd);
        return 1;
    } else
        return 0;
}

static int unc__ansifcopy(FILE *f1, FILE *f0) {
    char buf[BUFSIZ];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f0)))
        if (!fwrite(buf, n, 1, f1))
            return -1;
    return 0;
}

static int unc__ansicopy(const char *f0, const char *f1, int overwrite) {
    FILE *a = fopen(f0, "rb"), *b;
    if (!a) return -1;
#if UNCIL_C11
    if (!overwrite)
        b = fopen(f1, "wxb");
    else
#endif
    b = fopen(f1, "wb");
    if (!b) {
        fclose(a);
        return -1;
    } else {
        if (unc__ansifcopy(b, a) || ferror(a) || ferror(b)) goto unc__ansicopy_fail;
    }
    fclose(b);
    fclose(a);
    return 0;
unc__ansicopy_fail:
    fclose(b);
    {
        int old_errno = errno;
        remove(f1);
        errno = old_errno;
    }
    fclose(a);
    return -1;
}

#if UNCIL_IS_POSIX
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

int unc__getcwd(Unc_Allocator *alloc, char **c) {
    Unc_Size pathSize = PATH_MAX;
    char *buf = NULL, *obuf = NULL, *ptr = NULL;
    do {
        buf = unc__mmrealloc(alloc, Unc_AllocExternal, buf, pathSize);
        if (!buf) {
            unc__mmfree(alloc, obuf);
            return UNCIL_ERR_MEM;
        }
        obuf = buf;
        ptr = getcwd(buf, pathSize);
        if (!ptr && errno != ERANGE) {
            unc__mmfree(alloc, buf);
            return UNCIL_ERR_IO_UNDERLYING;
        }
        pathSize <<= 1;
    } while (!ptr);
    *c = buf;
    return 0;
}

int unc__getcwdv(Unc_View *w, Unc_Value *v) {
    char *buf;
    int e = unc__getcwd(&w->world->alloc, &buf);
    if (e) {
        if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.getcwd()", errno);
        return e;
    }
    return unc_newstringcmove(w, v, buf);
}

static char *unc__getext_posix(Unc_Allocator *alloc,
                               const char *path, size_t *len) {
    const char *ext = strrchr(path, '/');
    if (!ext)
        ext = path;
    else
        ext += 1;
    ext = strrchr(ext, '.');
    if (!ext) {
        *len = 0;
        return unc__mmalloc(alloc, Unc_AllocExternal, 0);
    } else {
        size_t l = strlen(ext);
        char *p = unc__mmalloc(alloc, Unc_AllocExternal, l);
        if (!p) return NULL;
        *len = l;
        unc__memcpy(p, ext, l);
        return p;
    }
}

static char *unc__normpath_posix(Unc_Allocator *alloc,
                                 const char *path, size_t *len) {
    byte *out_p = NULL;
    Unc_Size out_n = 0, out_c = 0;
    Unc_Size root;
    const char *prev;
    size_t n = 0;
    while (*path == '/')
        ++n, ++path;
    if (n) {
        /* more than two leading slashes shall be treated as a single slash */
        if (n > 2) n = 1;
        if (unc__strpush(alloc, &out_p, &out_n, &out_c, 6, n,
                         (const byte *)(path - n)))
            goto unc__normpath_posix_fail;
    }
    root = out_n;
    prev = NULL;
    while (prev || *path) {
        if (*path && *path != '/') {
            if (!prev) prev = path;
            ++path;
            continue;
        }
        if (*path == '/' && !prev) {
            /* out_n = root; */
            ++path;
            continue;
        }
        if (*prev == '.') {
            if (prev + 1 == path) {
                goto unc__normpath_posix_noappend;
            } else if (prev[1] == '.' && prev + 2 == path) {
                while (out_n > root) {
                    --out_n;
                    if (out_p[out_n] == '/')
                        break;
                }
                goto unc__normpath_posix_noappend;
            }
            goto unc__normpath_posix_append;
        } else
unc__normpath_posix_append:
        {
            if (out_n != root)
                if (unc__strpush1(alloc, &out_p, &out_n, &out_c, 6, '/'))
                    goto unc__normpath_posix_fail;
            if (unc__strpush(alloc, &out_p, &out_n, &out_c, 6, path - prev,
                            (const byte *)prev))
                goto unc__normpath_posix_fail;
        }
unc__normpath_posix_noappend:
        if (*path) {
            ASSERT(*path == '/');
            ++path;
            prev = NULL;
            continue;
        } else
            break;
    }

    *len = out_n;
    return (char *)out_p;
unc__normpath_posix_fail:
    unc__mmfree(alloc, out_p);
    return NULL;
}

int unc__abspath_posix(Unc_Allocator *alloc, const char *path,
                       size_t *len, char **str) {
    int e;
    char *cwd, *cwde;
    Unc_Size l, lp;
    char *p;
    if (*path == '/') {
        p = unc__normpath_posix(alloc, path, len);
        *str = p;
        return p ? 0 : UNCIL_ERR_MEM;
    }
    e = unc__getcwd(alloc, &cwd);
    if (e) return e;
    l = unc__mmgetsize(alloc, cwd);
    lp = strlen(cwd) + strlen(path) + 2;
    if (l < lp) {
        char *pcwd = unc__mmrealloc(alloc, Unc_AllocExternal, cwd, lp);
        if (!pcwd) {
            unc__mmfree(alloc, cwd);
            return UNCIL_ERR_MEM;
        }
        cwd = pcwd;
    }
    cwde = strchr(cwd, '\0');
    if (*cwd && cwde[-1] != '/')
        *cwde++ = '/';
    strcpy(cwde, path);
    p = unc__normpath_posix(alloc, cwd, len);
    unc__mmfree(alloc, cwd);
    *str = p;
    return p ? 0 : UNCIL_ERR_MEM;
}

char *unc__realpath_posix_path(Unc_Allocator *alloc, const char *path) {
    errno = ENOSYS;
    return NULL;
}

int unc__realpath_posix(Unc_Allocator *alloc, const char *path,
                       size_t *len, char **str) {
    int e;
    size_t sl;
    char *s, *p;
    e = unc__abspath_posix(alloc, path, &sl, &s);
    if (e) return e;
#if _POSIX_C_SOURCE >= 200809L
    p = realpath(s, NULL);
#else
    p = unc__realpath_posix_path(alloc, s);
#endif
    if (!p) {
        switch (errno) {
        case ENOENT:
            *len = sl;
            *str = s;
            return 0;
        case ENAMETOOLONG:
        case ENOMEM:
            return UNCIL_ERR_MEM;
        case ENOSYS:
            return UNCIL_ERR_LOGIC_NOTSUPPORTED;
        default:
            return UNCIL_ERR_IO_UNDERLYING;
        }
    }
    unc__mmfree(alloc, s);
    sl = strlen(p);
    s = unc__mmalloc(alloc, Unc_AllocExternal, sl + 1);
    if (!s) {
        free(p);
        return UNCIL_ERR_MEM;
    }
    strcpy(s, p);
    free(p);
    *len = sl + 1;
    *str = s;
    return 0;
}

int unc__relpath_posix(Unc_Allocator *alloc, const char *path,
                       const char *base, size_t *len, char **str) {
    int e;
    byte *out_p = NULL;
    Unc_Size out_n = 0, out_c = 0;
    size_t prefix;
    ASSERT(*path == '/' && *base == '/');
    prefix = strspn(path, base);
    if (!base[prefix] && !path[prefix]) {
        /* complete */
        e = unc__strpush1(alloc, &out_p, &out_n, &out_c, 6, '.');
    } else if (!base[prefix] && path[prefix] == '/') {
        path += prefix + 1;
        e = unc__strpush(alloc, &out_p, &out_n, &out_c, 6,
                         strlen(path), (const byte *)path);
    } else if (!path[prefix] && (base[prefix] == '/' || prefix <= 1)) {
        size_t nodes = 0;
        if (base[prefix] == '/')
            base += prefix;
        while (*base)
            nodes += *base++ == '/';
        if (nodes) {
            if ((e = unc__strpush(alloc, &out_p, &out_n, &out_c, 6, 2,
                                 (const byte *)"..")))
                goto unc__relpath_posix_fail;
            while (--nodes) {
                if ((e = unc__strpush(alloc, &out_p, &out_n, &out_c, 6, 3,
                                    (const byte *)"/..")))
                    goto unc__relpath_posix_fail;
            }
        }
    } else {
        size_t nodes = 0;
        ASSERT(prefix >= 1);
        while (path[prefix] != '/')
            --prefix;
        path += prefix;
        base += prefix;
        while (*base)
            nodes += *base++ == '/';
        if (nodes) {
            if ((e = unc__strpush(alloc, &out_p, &out_n, &out_c, 6, 2,
                                 (const byte *)"..")))
                goto unc__relpath_posix_fail;
            while (--nodes) {
                if ((e = unc__strpush(alloc, &out_p, &out_n, &out_c, 6, 3,
                                    (const byte *)"/..")))
                    goto unc__relpath_posix_fail;
            }
        } else
            ++path;
        e = unc__strpush(alloc, &out_p, &out_n, &out_c, 6,
                         strlen(path), (const byte *)path);
    }
    *len = out_n;
    *str = (char *)out_p;
    return 0;
unc__relpath_posix_fail:
    unc__mmfree(alloc, out_p);
    return e;
}

Unc_Size unc__pathprefix_posix(Unc_Size n, const char *a, const char *b) {
    Unc_Size i = 0;
    while (i < n && a[i] == b[i])
        ++i;
    if (!((!a[i] || a[i] == '/') && (!b[i] || b[i] == '/')))
        while (i > 0 && a[i] != '/')
            --i;
    return i;
}

struct unc__pathsplit_posix_buf {
    const char *s;
    int root;
    int final;
};

int unc__pathsplit_posix_init(struct unc__pathsplit_posix_buf *b,
                              const char *fn) {
    b->s = fn;
    b->root = 1;
    b->final = 0;
    return 0;
}

int unc__pathsplit_posix_next(struct unc__pathsplit_posix_buf *b,
                              size_t *l, const char **p) {
    const char *f = b->s, *fx;
    if (b->final)
        return 0;
    if (b->root) {
        b->root = 0;
        if (*f == '/') {
            *l = 1;
            *p = b->s++;
            return 1;
        }
    }
    fx = f;
    while (*fx && *fx != '/')
        ++fx;
    *l = fx - f;
    if (*fx)
        b->s = fx + 1;
    else
        b->s = fx, b->final = 1;
    *p = f;
    return 1;
}

int unc__stat_posix_obj(Unc_View *w, Unc_Value *v,
                        const char *fn, struct stat *st) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    const char *s = "other";

    e = unc_newtable(w, v);
    if (e) return e;

    if (S_ISLNK(st->st_mode)) s = "link";
#ifdef S_TYPEISMQ
    else if (S_TYPEISMQ(st)) s = "mqueue";
#endif
#ifdef S_TYPEISSEM
    else if (S_TYPEISSEM(st)) s = "sem";
#endif
#ifdef S_TYPEISSHM
    else if (S_TYPEISSHM(st)) s = "sharedmem";
#endif
#ifdef S_TYPEISTMO
    else if (S_TYPEISTMO(st)) s = "typedmem";
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(st->st_mode)) s = "socket";
#endif
    else if (S_ISFIFO(st->st_mode)) s = "fifo";
    else if (S_ISBLK(st->st_mode)) s = "block";
    else if (S_ISCHR(st->st_mode)) s = "char";
    else if (S_ISDIR(st->st_mode)) s = "dir";
    else if (S_ISREG(st->st_mode)) s = "file";
    
    if ((e = unc_newstringc(w, &tmp, s))) goto unc__stat_posix_obj_err;
    if ((e = unc_setattrc(w, v, "type", &tmp))) goto unc__stat_posix_obj_err;

    if (sizeof(off_t) > sizeof(Unc_Int) &&
            (st->st_size < (off_t)UNC_INT_MIN
                || st->st_size > (off_t)UNC_INT_MAX)) {
        e = unc_throwexc(w, "system", "size too large for this "
                                      "Uncil version to handle");
        goto unc__stat_posix_obj_err;
    }
    unc_setint(w, &tmp, st->st_size);
    if ((e = unc_setattrc(w, v, "size", &tmp))) goto unc__stat_posix_obj_err;

    unc_setint(w, &tmp, st->st_atime);
    if ((e = unc_setattrc(w, v, "atime", &tmp))) goto unc__stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_mtime);
    if ((e = unc_setattrc(w, v, "mtime", &tmp))) goto unc__stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_ctime);
    if ((e = unc_setattrc(w, v, "ctime", &tmp))) goto unc__stat_posix_obj_err;

    unc_setint(w, &tmp, st->st_ino);
    if ((e = unc_setattrc(w, v, "inode", &tmp))) goto unc__stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_nlink);
    if ((e = unc_setattrc(w, v, "links", &tmp))) goto unc__stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_dev);
    if ((e = unc_setattrc(w, v, "device", &tmp))) goto unc__stat_posix_obj_err;

    unc_setint(w, &tmp, st->st_uid);
    if ((e = unc_setattrc(w, v, "uid", &tmp))) goto unc__stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_gid);
    if ((e = unc_setattrc(w, v, "gid", &tmp))) goto unc__stat_posix_obj_err;

    {
        int mask = 0;
        if (st->st_mode & S_IXOTH) mask |= S_IXOTH;
        if (st->st_mode & S_IWOTH) mask |= S_IWOTH;
        if (st->st_mode & S_IROTH) mask |= S_IROTH;
        if (st->st_mode & S_IXGRP) mask |= S_IXGRP;
        if (st->st_mode & S_IWGRP) mask |= S_IWGRP;
        if (st->st_mode & S_IRGRP) mask |= S_IRGRP;
        if (st->st_mode & S_IXUSR) mask |= S_IXUSR;
        if (st->st_mode & S_IWUSR) mask |= S_IWUSR;
        if (st->st_mode & S_IRUSR) mask |= S_IRUSR;
#ifdef S_ISVTX
        if (st->st_mode & S_ISVTX) mask |= S_ISVTX;
#endif
        if (st->st_mode & S_ISGID) mask |= S_ISGID;
        if (st->st_mode & S_ISUID) mask |= S_ISUID;
        unc_setint(w, &tmp, mask);
        if ((e = unc_setattrc(w, v, "mode", &tmp)))
            goto unc__stat_posix_obj_err;
    }

unc__stat_posix_obj_err:
    unc_clear(w, &tmp);
    return e;
}

struct unc__scan_posix_buf {
    DIR *dir;
    Unc_Size baselen;
    char *base;
};

static Unc_RetVal unc__scan_posix_init(Unc_Allocator *alloc,
            struct unc__scan_posix_buf *buf, const char *fn) {
    DIR *dir;
    size_t namelen = strlen(fn);
    int addslash = namelen && fn[namelen - 1] != '/';
    buf->baselen = namelen + addslash;
    buf->base = unc__mmalloc(alloc, Unc_AllocExternal, buf->baselen);
    if (!buf->base) return UNCIL_ERR_MEM;
    strcpy(buf->base, fn);
    if (addslash) buf->base[namelen] = '/';
    dir = opendir(fn);
    if (!dir) {
        unc__mmfree(alloc, buf->base);
        return UNCIL_ERR_IO_UNDERLYING;
    }
    buf->dir = dir;
    return 0;
}

static Unc_RetVal unc__scan_posix_next(Unc_Allocator *alloc,
            struct unc__scan_posix_buf *buf, Unc_Size *nn, char **ns) {
    struct dirent *dir;
    size_t namelen;
    Unc_Size n;
    char *ptr;

    errno = 0;
    dir = readdir(buf->dir);
    if (!dir) {
        if (!errno) {
            *nn = 0, *ns = NULL;
            return 0;
        }
        return UNCIL_ERR_IO_UNDERLYING;
    }
    namelen = strlen(dir->d_name);
    n = buf->baselen + namelen;
    ptr = unc__mmalloc(alloc, Unc_AllocExternal, n);
    if (!ptr) return UNCIL_ERR_MEM;
    unc__memcpy(ptr, buf->base, buf->baselen);
    unc__memcpy(ptr + buf->baselen, dir->d_name, namelen);
    *nn = n;
    *ns = ptr;
    return 0;
}

static Unc_RetVal unc__scan_posix_destr(Unc_Allocator *alloc,
                                        struct unc__scan_posix_buf *d) {
    closedir(d->dir);
    unc__mmfree(alloc, d->base);
    return 0;
}

static Unc_RetVal unc__scan_posix_destrw(Unc_View *w, size_t n, void *data) {
    return unc__scan_posix_destr(&w->world->alloc, data);
}

#define DEFAULT_MODE (S_IRWXU | S_IRWXG | S_IRWXO)

#define UNCIL_POSIX_ERR_EXISTS 1
#define UNCIL_POSIX_ERR_DIRONFILE 2
#define UNCIL_POSIX_ERR_FILEONDIR 3
#define UNCIL_POSIX_ERR_DIRONDIR 4

static int unc__fdcopy_eof(int fd1, int fd0) {
    char buf[BUFSIZ];
    ssize_t i, n, e;
    do {
        i = read(fd0, buf, sizeof(buf));
        if (i == -1) {
#if EWOULDBLOCK != EAGAIN
            if (errno == EWOULDBLOCK) errno = EAGAIN;
#endif
            switch (errno) {
            case EAGAIN:
            case EINTR:
                continue;
            default:
                return 1;
            }
        } else if (i) {
            i = 0, n = i;
            do {
                e = write(fd1, buf + i, n - i);
                if (e == -1) {
#if EWOULDBLOCK != EAGAIN
                    if (errno == EWOULDBLOCK) errno = EAGAIN;
#endif
                    switch (errno) {
                    case EAGAIN:
                    case EINTR:
                        continue;
                    default:
                        return 1;
                    }
                }
                i += e;
            } while (i < n);
        }
    } while (i);
    return 0;
}

#define EXBITS (S_IXUSR | S_IXGRP | S_IXOTH)

static Unc_RetVal unc__copyfile_posix(
                        const char *fn, const char *d, int *copied) {
    int overwrite = *copied;
    struct stat st0, st1;
    int fd0, fd1;
    mode_t mode;
    *copied = 0;
    if (lstat(fn, &st0)) return UNCIL_ERR_IO_UNDERLYING;
    if (S_ISDIR(st0.st_mode)) {
        mode_t mode = DEFAULT_MODE;
        if (lstat(fn, &st1)) {
            switch (errno) {
            case ENOENT:
                *copied = 1;
                if (!mkdir(d, mode))
                    return 0;
            default:
                return UNCIL_ERR_IO_UNDERLYING;
            }
        }
        if (!S_ISDIR(st1.st_mode)) return UNCIL_POSIX_ERR_DIRONFILE;
        if (rmdir(d)) return UNCIL_ERR_IO_UNDERLYING;
        if (mkdir(d, mode)) return UNCIL_ERR_IO_UNDERLYING;
        *copied = 1;
        return 0;
    }
    fd0 = open(fn, O_RDONLY);
    if (fd0 == -1) return UNCIL_ERR_IO_UNDERLYING;
    mode = DEFAULT_MODE & ~EXBITS;
    if (S_ISREG(st0.st_mode)) mode |= st0.st_mode & EXBITS;
    fd1 = open(d, overwrite
                    ? (O_WRONLY | O_CREAT)
                    : (O_WRONLY | O_CREAT | O_EXCL),
                    mode);
    if (fd1 == -1) {
        close(fd0);
        return UNCIL_ERR_IO_UNDERLYING;
    }
    if (fstat(fd1, &st1)) goto unc__copyfile_posix_fail;
    if (st0.st_rdev == st1.st_rdev && st0.st_ino == st1.st_ino) {
        /* same file! */
        close(fd0);
        close(fd1);
        return 0;
    }
    if (S_ISDIR(st1.st_mode)) {
        close(fd0);
        close(fd1);
        return UNCIL_POSIX_ERR_FILEONDIR;
    }
    if (ftruncate(fd1, 0)) goto unc__copyfile_posix_fail;
    if (!unc__fdcopy_eof(fd1, fd0)) {
        *copied = 1;
        close(fd0);
        close(fd1);
        return 0;
    }
unc__copyfile_posix_fail:
    close(fd0);
    unlink(d);
    close(fd1);
    return UNCIL_ERR_IO_UNDERLYING;
}

static Unc_RetVal unc__copymeta_posix(Unc_Allocator *alloc,
                        const char *fn, const char *d) {
    struct stat st0;
    if (lstat(fn, &st0)) return UNCIL_ERR_IO_UNDERLYING;
    /*
    int fd;
    fd = open(d, O_RDWR);
    if (fd == -1) return UNCIL_ERR_IO_UNDERLYING;*/
    
    /* do we what we can, ignore errors from this point */

    /* times */
    {
#if _POSIX_VERSION >= 200809L
        struct timeval times[2];
#if UNCIL_IS_LINUX
        times[0].tv_sec = st0.st_atim.tv_sec;
        times[0].tv_usec = st0.st_atim.tv_nsec / 1000;
        times[1].tv_sec = st0.st_mtim.tv_sec;
        times[1].tv_usec = st0.st_mtim.tv_nsec / 1000;
#else
        times[0].tv_sec = st0.st_atime;
        times[0].tv_usec = 0;
        times[1].tv_sec = st0.st_mtime;
        times[1].tv_usec = 0;
#endif
        utimes(d, times);
#else
        struct utimbuf utmbuf;
        utmbuf.actime = st0.st_atime;
        utmbuf.modtime = st0.st_mtime;
        utime(d, &utmbuf);
#endif
    }
    /* owner */
    chown(d, st0.st_uid, st0.st_gid);
    /* permission bits */
    chmod(d, st0.st_mode);

    return 0;
}

static int unc__exists_posix(const char *fn) {
    struct stat st;
    return !lstat(fn, &st);
}

static Unc_RetVal unc__mkdir_posix_rec(Unc_Allocator *alloc,
                        const char *fn, mode_t mode) {
    int old_errno = errno;
    size_t l = strlen(fn);
    int sink = 1;
    size_t slashes = 0;
    char *p = unc__mmalloc(alloc, Unc_AllocString, l + 1);
    if (!p) return UNCIL_ERR_MEM;
    strcpy(p, fn);
    while (sink) {
        char *slash = strrchr(p, '/');
        if (!slash) {
            unc__mmfree(alloc, p);
            errno = old_errno;
            return UNCIL_ERR_IO_UNDERLYING;
        }
        *slash = 0;
        ++slashes;
        sink = mkdir(p, mode);
        if (sink) {
            switch (errno) {
            case EEXIST:
                sink = 0;
                break;
            case ENOENT:
                break;
            default:
                unc__mmfree(alloc, p);
                return UNCIL_ERR_IO_UNDERLYING;
            }
        }
    }
    while (slashes--) {
        *strchr(p, 0) = '/';
        if (mkdir(p, mode)) {
            switch (errno) {
            case EEXIST:
                continue;
            default:
                unc__mmfree(alloc, p);
                return UNCIL_ERR_IO_UNDERLYING;
            }
        }
    }
    unc__mmfree(alloc, p);
    return 0;
}

static Unc_RetVal unc__mkdir_posix(Unc_Allocator *alloc,
                        const char *fn, mode_t mode, int parents) {
    if (mkdir(fn, mode)) {
        switch (errno) {
        case EEXIST:
            if (parents) return 0;
        case ENOENT:
            if (parents) return unc__mkdir_posix_rec(alloc, fn, mode);
        default:
            return UNCIL_ERR_IO_UNDERLYING;
        }
    }
    return 0;
}

static Unc_RetVal unc__chmod_posix(Unc_Allocator *alloc,
                        const char *fn, mode_t mode) {
    return chmod(fn, mode) ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc__remove_posix(Unc_Allocator *alloc, const char *fn) {
    return remove(fn) ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc__move_posix(Unc_Allocator *alloc,
                        const char *fn, const char *d, int overwrite) {
    if (!overwrite && unc__exists_posix(d))
        return UNCIL_POSIX_ERR_EXISTS; /* TOCTOU?? */
    return rename(fn, d) ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc__link_posix(Unc_Allocator *alloc,
                        const char *fn, const char *n, int symbolic) {
    return (symbolic ? symlink(fn, n) : link(fn, n))
            ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc__copy_posix(Unc_Allocator *alloc,
                        const char *fn, const char *d,
                        int metadata, int overwrite) {
    int copied = overwrite;
    int e = unc__copyfile_posix(fn, d, &copied);
    if (!e && copied && metadata)
        return unc__copymeta_posix(alloc, fn, d);
    return e ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc__copy_posix2(Unc_Allocator *alloc,
                        const char *fn, const char *d,
                        int metadata, int overwrite) {
    int e = unc__copy_posix(alloc, fn, d, metadata, overwrite);
    if (!overwrite && e == UNCIL_ERR_IO_UNDERLYING && errno == EEXIST)
        e = 0;
    return e;
}

struct unc__rcopy_posix_buf {
    Unc_Allocator *alloc;
    char *src;
    Unc_Size src_n, src_c;
    char *dst;
    Unc_Size dst_n, dst_c;
    const char *dsto;
    Unc_Size dsto_n;
    int metadata;
    int overwrite;
    int follow;
    Unc_Size recurse;
};

static Unc_RetVal unc__rcopy_posix_mkdir(const char *fn, mode_t mode,
                                         int *created) {
    if (mkdir(fn, mode)) {
        switch (errno) {
        case EEXIST:
            *created = 0;
            return 0;
        default:
            return UNCIL_ERR_IO_UNDERLYING;
        }
    }
    *created = 1;
    return 0;
}

static Unc_RetVal unc__rcopy_posix_do(struct unc__rcopy_posix_buf *buf,
                                      struct stat *pst) {
    int e;
    DIR *d;
    struct dirent *de;
    Unc_Size srcn, dstn;

    if (!buf->recurse) return UNCIL_ERR_TOODEEP;
    --buf->recurse;
    
    d = opendir(buf->src);
    if (!d) return UNCIL_ERR_IO_UNDERLYING;
    buf->src[buf->src_n - 1] = '/';
    srcn = buf->src_n;
    dstn = buf->dst_n;
    
    e = 0;
    while ((errno = 0, de = readdir(d))) {
        Unc_Size sl;
        char *oldsrc = NULL;
        Unc_Size oldsrc_c;
        /* skip . and .. */
        if (de->d_name[0] == '.') {
            if (!de->d_name[1]) continue;
            if (!strcmp(de->d_name + 1, ".")) continue;
        }
        sl = strlen(de->d_name) + 1;
        buf->src_n = srcn;
        e = unc__strputn(buf->alloc,
                    (byte **)&buf->src, &buf->src_n, &buf->src_c,
                    6, sl, (const byte *)de->d_name);
        if (e) break;
        if (lstat(buf->src, pst)) break;
        buf->dst_n = dstn;
        e = unc__strputn(buf->alloc,
                    (byte **)&buf->dst, &buf->dst_n, &buf->dst_c,
                    6, sl, (const byte *)de->d_name);
        if (buf->follow) {
            if (S_ISLNK(pst->st_mode)) {
                struct stat tst;
                if (stat(buf->src, &tst)) break;
            }
            while (S_ISLNK(pst->st_mode)) {
                size_t in = 64;
                char *ip = unc__malloc(buf->alloc, Unc_AllocExternal, in), *ip2;
                ssize_t ir;
                if (!ip)
                    e = UNCIL_ERR_MEM;
                else
                    for (;;) {
                        ir = readlink(buf->src, ip, in);
                        if (ir < in) {
                            ip[ir] = 0;
                            break;
                        }
                        ip2 = unc__mrealloc(buf->alloc, Unc_AllocExternal, ip,
                                            in, in << 1);
                        in <<= 1;
                        if (!ip2) {
                            e = UNCIL_ERR_MEM;
                            break;
                        }
                        ip = ip2;
                    }
                if (e) goto unc__rcopy_posix_do_link_break;
                if (lstat(ip, pst)) goto unc__rcopy_posix_do_link_break;
                if (S_ISDIR(pst->st_mode)) {
                    int loop = 0;
                    if (!unc__memcmp(ip, buf->dsto, buf->dsto_n) &&
                            (!ip[buf->dsto_n] || ip[buf->dsto_n] == '/'))
                        loop = 1;
                    if (ir <= buf->src_n && !unc__memcmp(ip, buf->src, ir) &&
                            (!buf->src[ir] || buf->src[ir] == '/'))
                        loop = 1;
                    if (oldsrc && ir <= srcn
                            && !unc__memcmp(ip, oldsrc, ir) &&
                            (!oldsrc[ir] || oldsrc[ir] == '/'))
                        loop = 1;
                    if (loop) {
                        errno = ELOOP;
                        goto unc__rcopy_posix_do_link_break;
                    }
                }
                if (!oldsrc) {
                    oldsrc = buf->src;
                    oldsrc_c = buf->src_c;
                }
                buf->src = ip;
                buf->src_n = ir;
                buf->src_c = in;
            }
unc__rcopy_posix_do_link_break:
            ;
        }
        if (S_ISDIR(pst->st_mode)) {
            Unc_Size stn = buf->src_n - 1, dtn = buf->dst_n - 1;
            int created;
            e = unc__rcopy_posix_mkdir(buf->dst, DEFAULT_MODE, &created);
            if (e) break;
            buf->dst[dtn] = '/';
            e = unc__rcopy_posix_do(buf, pst);
            if (e) break;
            if (!buf->metadata || created || buf->overwrite) continue;
            buf->src[stn] = buf->dst[dtn] = 0;
            e = unc__copymeta_posix(buf->alloc, buf->src, buf->dst);
        } else {
            e = unc__copy_posix2(buf->alloc, buf->src, buf->dst,
                                            buf->metadata, buf->overwrite);
        }
        if (e) break;
        if (oldsrc) {
            unc__mfree(buf->alloc, buf->src, buf->src_c);
            buf->src = oldsrc;
            buf->src_c = oldsrc_c;
        }
    }
    
    if (errno) {
        int old_errno = errno;
        closedir(d);
        errno = old_errno;
        if (!e) e = UNCIL_ERR_IO_UNDERLYING;
    } else
        closedir(d);
    ++buf->recurse;
    return e;
}

static Unc_RetVal unc__rcopy_posix(Unc_View *w,
                        const char *fn, const char *d,
                        int metadata, int overwrite, int follow) {
    int e;
    struct unc__rcopy_posix_buf buf;
    struct stat st;
    int dstdir;
    const char *bn;
    if (stat(fn, &st)) return UNCIL_ERR_IO_UNDERLYING;
    buf.alloc = &w->world->alloc;
    buf.src = buf.dst = NULL;
    buf.src_n = buf.src_c = buf.dst_n = buf.dst_c = 0;
    buf.dsto = d;
    buf.dsto_n = strlen(d);
    buf.metadata = metadata;
    buf.overwrite = overwrite;
    buf.follow = follow;
    buf.recurse = unc_recurselimit(w);
    if (unc__strputn(buf.alloc, (byte **)&buf.dst, &buf.dst_n, &buf.dst_c,
                     6, buf.dsto_n + 1, (const byte *)d))
        return UNCIL_ERR_MEM;
    --buf.dst_n;
    dstdir = buf.dst_n && buf.dst[buf.dst_n - 1] == '/';
    if (!S_ISDIR(st.st_mode)) {
        if (dstdir) {
            errno = ENOTDIR;
            e = UNCIL_ERR_IO_UNDERLYING;
        } else {
            e = unc__strput(buf.alloc,
                    (byte **)&buf.dst, &buf.dst_n, &buf.dst_c,
                    6, '/');
            bn = strrchr(fn, '/');
            fn = bn ? bn + 1 : fn;
            ASSERT(*fn);
            if (!e) e = unc__strputn(buf.alloc,
                       (byte **)&buf.dst, &buf.dst_n, &buf.dst_c,
                      6, strlen(bn) + 1, (const byte *)bn);
            if (!e)
                e = unc__copy_posix2(buf.alloc, fn, buf.dst,
                                     metadata, overwrite);
        }
    } else {
        if (unc__strputn(buf.alloc, (byte **)&buf.src, &buf.src_n, &buf.src_c,
                        6, strlen(fn) + 1, (const byte *)fn)) {
            unc__mfree(buf.alloc, buf.dst, buf.dst_c);
            return UNCIL_ERR_MEM;
        }
        if (buf.src_n > 1 && buf.src[buf.src_n - 2] == '/')
            buf.src[--buf.src_n - 1] = 0;
        bn = strrchr(buf.src, '/');
        fn = bn ? bn + 1 : fn;
        if (!dstdir)
            buf.dst[buf.dst_n++] = '/';
        e = unc__strputn(buf.alloc,
                        (byte **)&buf.dst, &buf.dst_n, &buf.dst_c,
                        6, strlen(fn) + 1, (const byte *)fn);
        if (!e) {
            int created;
            Unc_Size stn = buf.src_n - 1, dtn = buf.dst_n - 1;
            buf.dst[dtn] = 0;
            e = unc__rcopy_posix_mkdir(buf.dst, DEFAULT_MODE, &created);
            if (!e) buf.dst[dtn] = '/', e = unc__rcopy_posix_do(&buf, &st);
            if (!e && buf.metadata && (created || buf.overwrite)) {
                buf.src[stn] = buf.dst[dtn] = 0;
                e = unc__copymeta_posix(buf.alloc, buf.src, buf.dst);
            }
        }
        unc__mfree(buf.alloc, buf.src, buf.src_c);
    }
    unc__mfree(buf.alloc, buf.dst, buf.dst_c);
    return e;
}

static Unc_RetVal unc__rdestroy_posix_do(struct unc__rcopy_posix_buf *buf,
                                         struct stat *pst) {
    int e;
    DIR *d;
    struct dirent *de;
    Unc_Size srcn;

    if (!buf->recurse) return UNCIL_ERR_TOODEEP;
    --buf->recurse;
    
    d = opendir(buf->src);
    if (!d) return UNCIL_ERR_IO_UNDERLYING;
    buf->src[buf->src_n - 1] = '/';
    srcn = buf->src_n;
    
    e = 0;
    while ((errno = 0, de = readdir(d))) {
        Unc_Size sl;
        /* skip . and .. */
        if (de->d_name[0] == '.') {
            if (!de->d_name[1]) continue;
            if (!strcmp(de->d_name + 1, ".")) continue;
        }
        sl = strlen(de->d_name) + 1;
        buf->src_n = srcn;
        e = unc__strputn(buf->alloc,
                    (byte **)&buf->src, &buf->src_n, &buf->src_c,
                    6, sl, (const byte *)de->d_name);
        if (e) break;
        if (lstat(buf->src, pst)) break;
        if (S_ISDIR(pst->st_mode))
            e = unc__rdestroy_posix_do(buf, pst);
        else if (unlink(buf->src))
            e = UNCIL_ERR_IO_UNDERLYING;
        if (e) break;
    }
    
    if (errno) {
        int old_errno = errno;
        closedir(d);
        errno = old_errno;
        if (!e) e = UNCIL_ERR_IO_UNDERLYING;
    } else {
        closedir(d);
        buf->src[srcn - 1] = 0;
        if (rmdir(buf->src))
            e = UNCIL_ERR_IO_UNDERLYING;
    }
    ++buf->recurse;
    return e;
}

static Unc_RetVal unc__rdestroy_posix(Unc_View *w, const char *fn) {
    int e;
    struct unc__rcopy_posix_buf buf;
    struct stat st;
    if (stat(fn, &st)) return UNCIL_ERR_IO_UNDERLYING;
    buf.alloc = &w->world->alloc;
    buf.src = buf.dst = NULL;
    buf.src_n = buf.src_c = buf.dst_n = buf.dst_c = 0;
    buf.recurse = unc_recurselimit(w);
    if (!S_ISDIR(st.st_mode)) {
        e = unc__remove_posix(&w->world->alloc, fn);
    } else {
        if (unc__strputn(buf.alloc, (byte **)&buf.src, &buf.src_n, &buf.src_c,
                        6, strlen(fn) + 1, (const byte *)fn)) {
            unc__mfree(buf.alloc, buf.dst, buf.dst_c);
            return UNCIL_ERR_MEM;
        }
        if (buf.src_n > 1 && buf.src[buf.src_n - 2] == '/')
            buf.src[--buf.src_n - 1] = 0;
        e = unc__rdestroy_posix_do(&buf, &st);
        unc__mfree(buf.alloc, buf.src, buf.src_c);
    }
    unc__mfree(buf.alloc, buf.dst, buf.dst_c);
    return e;
}

#endif

Unc_RetVal unc__lib_fs_exists(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        struct stat st;
        e = stat(fn, &st);
        if (!e)
            unc_setbool(w, &v, 1);
        else {
            switch (errno) {
            case 0:
                break;
            case EACCES:
            case ENOENT:
            case ENOTDIR:
                unc_setbool(w, &v, 0);
                break;
            default:
                return unc__fs_makeerr(w, "fs.exists()", errno);
            }
        }
#else
        unc_setbool(w, &v, unc__ansiexists(fn));
#endif
    }
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc__lib_fs_isdir(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        struct stat st;
        e = stat(fn, &st);
        if (!e)
            unc_setbool(w, &v, S_ISDIR(st.st_mode));
        else {
            switch (errno) {
            case 0:
                break;
            case EACCES:
            case ENOENT:
            case ENOTDIR:
                unc_setbool(w, &v, 0);
                break;
            default:
                return unc__fs_makeerr(w, "fs.isdir()", errno);
            }
        }
#else
        unc_setbool(w, &v, 0);
#endif
    }
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc__lib_fs_isfile(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        struct stat st;
        e = stat(fn, &st);
        if (!e)
            unc_setbool(w, &v, S_ISREG(st.st_mode));
        else {
            switch (errno) {
            case 0:
                break;
            case EACCES:
            case ENOENT:
            case ENOTDIR:
                unc_setbool(w, &v, 0);
                break;
            default:
                return unc__fs_makeerr(w, "fs.isfile()", errno);
            }
        }
#else
        unc_setbool(w, &v, unc__ansiexists(fn));
#endif
    }
    return unc_push(w, 1, &v, NULL);
}

Unc_RetVal unc__lib_fs_getcwd(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    {
#if UNCIL_IS_POSIX
        e = unc__getcwdv(w, &v);
#else
        e = 0;
#endif
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
    }
}

Unc_RetVal unc__lib_fs_setcwd(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        if (chdir(fn))
            return unc__fs_makeerr(w, "fs.setcwd()", errno);
        e = unc__getcwdv(w, &v);
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_getext(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        size_t l;
        char *p = unc__getext_posix(&w->world->alloc, fn, &l);
        if (!p)
            e = UNCIL_ERR_MEM;
        else {
            if ((e = unc_newstringmove(w, &v, l, p)))
                unc__mmfree(&w->world->alloc, p);
        }
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_normpath(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        size_t l;
        char *p = unc__normpath_posix(&w->world->alloc, fn, &l);
        if (!p)
            e = UNCIL_ERR_MEM;
        else {
            if ((e = unc_newstringmove(w, &v, l, p)))
                unc__mmfree(&w->world->alloc, p);
        }
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_abspath(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        size_t l;
        char *p;
        e = unc__abspath_posix(&w->world->alloc, fn, &l, &p);
        if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            e = unc__fs_makeerr(w, "fs.abspath()", errno);
        else if (!e) {
            if ((e = unc_newstringmove(w, &v, l, p)))
                unc__mmfree(&w->world->alloc, p);
        }
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_realpath(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        size_t l;
        char *p;
        e = unc__realpath_posix(&w->world->alloc, fn, &l, &p);
        if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            e = unc__fs_makeerr(w, "fs.realpath()", errno);
        else if (e == UNCIL_ERR_LOGIC_NOTSUPPORTED)
            e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
        else if (!e) {
            if ((e = unc_newstringmove(w, &v, l, p)))
                unc__mmfree(&w->world->alloc, p);
        }
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_relpath(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        size_t al, bl, l;
        char *ap, *bp, *p;
        const char *root;
        int root_cwd;
        if (unc_gettype(w, &args.values[1])) {
            e = unc_getstringc(w, &args.values[1], &root);
            if (e) return e;
            root_cwd = 0;
        } else {
            e = unc__getcwd(&w->world->alloc, &bp);
            if (e) {
                if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
                    return unc__fs_makeerr(w, "fs.relpath() -> fs.getcwd()",
                        errno);
                return e;
            }
            bl = strlen(bp);
            root_cwd = 1;
        }
        e = unc__abspath_posix(&w->world->alloc, fn, &al, &ap);
        if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            e = unc__fs_makeerr(w, "fs.relpath()", errno);
        ap[al] = 0;
        if (!e && !root_cwd) {    
            e = unc__abspath_posix(&w->world->alloc, root, &bl, &bp);
            if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
                e = unc__fs_makeerr(w, "fs.relpath()", errno);
            bp[bl] = 0;
        }
        if (!e) {
            e = unc__relpath_posix(&w->world->alloc, ap, bp, &l, &p);
            if (!e) {
                if ((e = unc_newstringmove(w, &v, l, p)))
                    unc__mmfree(&w->world->alloc, p);
            }
        }
        unc__mmfree(&w->world->alloc, bp);
        unc__mmfree(&w->world->alloc, ap);
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_pathjoin(Unc_View *w, Unc_Tuple args, void *udata) {
#if UNCIL_IS_POSIX
    int e;
    byte *out_p = NULL;
    Unc_Size out_n = 0, out_c = 0, i;
    Unc_Value v = UNC_BLANK;
    for (i = 0; i < args.count; ++i) {
        const char *fn;
        e = unc_getstringc(w, &args.values[i], &fn);
        if (e) {
            unc_mfree(w, out_p);
            return e;
        }
        if (*fn == '/') {
            out_n = 0;
        } else if (out_n && out_p[out_n - 1] != '/') {
            e = unc__strpush1(&w->world->alloc, &out_p, &out_n, &out_c, 6, '/');
            if (e) {
                unc_mfree(w, out_p);
                return e;
            }
        }
        e = unc__strpush(&w->world->alloc, &out_p, &out_n, &out_c, 6,
                                            strlen(fn), (const byte *)fn);
        if (e) {
            unc_mfree(w, out_p);
            return e;
        }
    }
    if ((e = unc_newstringmove(w, &v, out_n, (char *)out_p)))
        unc__mmfree(&w->world->alloc, out_p);
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc__lib_fs_pathprefix(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
#if UNCIL_IS_POSIX
    {
        Unc_Size i, prefix;
        int abs;
        abs = *fn == '/';
        prefix = strlen(fn);
        for (i = 1; i < args.count; ++i) {
            const char *nfn;
            e = unc_getstringc(w, &args.values[i], &nfn);
            if (e) return e;
            if (abs != (*nfn == '/'))
                return unc_pushmove(w, &v, NULL);
            prefix = unc__pathprefix_posix(prefix, fn, nfn);
        }
        e = unc_newstring(w, &v, prefix, fn);
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
    }
#else
    (void)v;
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc__lib_fs_pathsplit(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Size ai = 0, an;
    Unc_Value *ap;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    an = 8;
    e = unc_newarray(w, &v, an, &ap);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        size_t l;
        const char *p;
        struct unc__pathsplit_posix_buf b;
        e = unc__pathsplit_posix_init(&b, fn);
        if (e) {
            unc_unlock(w, &v);
            unc_clear(w, &v);
            return e;
        }
        while (unc__pathsplit_posix_next(&b, &l, &p)) {
            if (ai >= an) {
                Unc_Size aq = an + 8;
                e = unc_resizearray(w, &v, aq, &ap);
                if (e) break;
                an = aq;
            }
            e = unc_newstring(w, &ap[ai++], l, p);
            if (e) break;
        }
        unc_unlock(w, &v);
        if (!e) e = unc_resizearray(w, &v, ai, &ap);
        if (!e) e = unc_pushmove(w, &v, NULL);
        else unc_clear(w, &v);
        return e;
#else
        (void)v; (void)ai; (void)an; (void)ap;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_basename(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        const char *p = strrchr(fn, '/');
        p = p ? p + 1 : fn;
        e = unc_newstringc(w, &v, p);
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_dirname(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        const char *p = strrchr(fn, '/');
        if (p == fn) ++p;
        e = p ? unc_newstring(w, &v, p - fn, fn) : 0;
        if (!e) e = unc_pushmove(w, &v, NULL);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_stat(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        struct stat st;
        e = stat(fn, &st);
        if (!e) {
            e = unc__stat_posix_obj(w, &v, fn, &st);
            if (!e) e = unc_pushmove(w, &v, NULL);
        } else
            return unc__fs_makeerr(w, "fs.stat()", errno);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_lstat(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        struct stat st;
        e = lstat(fn, &st);
        if (!e)  {
            e = unc__stat_posix_obj(w, &v, fn, &st);
            if (!e) e = unc_pushmove(w, &v, NULL);
        } else
            return unc__fs_makeerr(w, "fs.lstat()", errno);
        return e;
#else
        (void)v;
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

static Unc_RetVal unc__lib_fs_scan_next(Unc_View *w, Unc_Tuple args,
                                        void *udata) {
    struct unc__scan_posix_buf *buf;
    Unc_Size bn;
    Unc_Size sn;
    Unc_Value v = UNC_BLANK;
    char *ss;
    int e;
    ASSERT(unc_boundcount(w) == 1);
    e = unc_lockopaque(w, unc_boundvalue(w, 0), &bn, (void **)&buf);
    if (e) return e;
#if UNCIL_IS_POSIX
    e = unc__scan_posix_next(&w->world->alloc, buf, &sn, &ss);
    if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
        e = unc__fs_makeerr(w, "fs.scan()", errno);
#else
    sn = 0, ss = NULL;
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    if (!e && ss) {
        e = unc_newstringmove(w, &v, sn, ss);
        if (!e) e = unc_pushmove(w, &v, NULL);
        else unc_mfree(w, ss);
    }
    unc_unlock(w, unc_boundvalue(w, 0));
    return e;
}

Unc_RetVal unc__lib_fs_scan(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Value v = UNC_BLANK, q = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        struct unc__scan_posix_buf *buf;
        e = unc_newopaque(w, &q, NULL, sizeof(struct unc__scan_posix_buf),
                    (void **)&buf, &unc__scan_posix_destrw, 0, NULL, 0, NULL);
        if (e) return e;
        e = unc__scan_posix_init(&w->world->alloc, buf, fn);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.scan()", errno);
#else
        e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
        if (!e) {
            e = unc_newcfunction(w, &v, &unc__lib_fs_scan_next,
                            UNC_CFUNC_DEFAULT, 0, 0, 0, NULL, 1, &q, 0, NULL,
                            "fs.scan(),next", NULL);
            if (!e) e = unc_pushmove(w, &v, NULL);
            unc_clear(w, &q);
        }
        return e;
    }
}

Unc_RetVal unc__lib_fs_mkdir(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    int got_mode = 0, parents;
    Unc_Int mode = 0;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    if (unc_gettype(w, &args.values[1])) {
        e = unc_getint(w, &args.values[1], &mode);
        if (e) return e;
        got_mode = 1;
    }
    parents = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(parents)) return parents;
#if UNCIL_IS_POSIX
    {
        e = unc__mkdir_posix(&w->world->alloc, fn,
                got_mode ? (mode_t)mode : DEFAULT_MODE,
                parents);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.mkdir()", errno);
        return e;
    }
#else
    (void)got_mode; (void)mode;
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc__lib_fs_chmod(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    Unc_Int mode = 0;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &mode);
    if (e) return e;
#if UNCIL_IS_POSIX
    {
        e = unc__chmod_posix(&w->world->alloc, fn, (mode_t)mode);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.chmod()", errno);
        return e;
    }
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

Unc_RetVal unc__lib_fs_remove(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        e = unc__remove_posix(&w->world->alloc, fn);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.remove()", errno);
        return e;
#else
        if (remove(fn))
            return unc__fs_makeerr(w, "fs.remove()", errno);
        return 0;
#endif
    }
}

Unc_RetVal unc__lib_fs_rdestroy(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    {
#if UNCIL_IS_POSIX
        e = unc__rdestroy_posix(w, fn);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.rdestroy()", errno);
        return e;
#else
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_copy(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn, *fn2;
    int metadata, overwrite;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    metadata = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(metadata)) return metadata;
    overwrite = unc_getbool(w, &args.values[3], 0);
    if (UNCIL_IS_ERR(overwrite)) return overwrite;
    {
#if UNCIL_IS_POSIX
        size_t fn1l;
        char *fn1;
        e = unc__realpath_posix(&w->world->alloc, fn, &fn1l, &fn1);
        if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.copy()", errno);
        e = unc__copy_posix(&w->world->alloc, fn1, fn2, metadata, overwrite);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            e = unc__fs_makeerr(w, "fs.copy()", errno);
        else if (e == UNCIL_POSIX_ERR_EXISTS)
            e = unc_throwexc(w, "value", "fs.copy(): destination exists");
        else if (e == UNCIL_POSIX_ERR_DIRONFILE)
            e = unc_throwexc(w, "value",
                "fs.copy(): cannot copy file on directory");
        else if (e == UNCIL_POSIX_ERR_FILEONDIR)
            e = unc_throwexc(w, "value",
                "fs.copy(): cannot copy directory on file");
        else if (e == UNCIL_POSIX_ERR_DIRONDIR)
            e = unc_throwexc(w, "value",
                "fs.copy(): cannot copy directory on non-empty directory");
        unc__mmfree(&w->world->alloc, fn1);
        return e;
#else
        if (!overwrite && unc__ansiexists(fn2))
            return unc_throwexc(w, "value", "fs.copy(): destination exists");
        if (unc__ansicopy(fn, fn2, overwrite))
            return unc__fs_makeerr(w, "fs.copy()", errno);
        return 0;
#endif
    }
}

Unc_RetVal unc__lib_fs_lcopy(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn, *fn2;
    int metadata, overwrite;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    metadata = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(metadata)) return metadata;
    overwrite = unc_getbool(w, &args.values[3], 0);
    if (UNCIL_IS_ERR(overwrite)) return overwrite;
    {
#if UNCIL_IS_POSIX
        e = unc__copy_posix(&w->world->alloc, fn, fn2, metadata, overwrite);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.lcopy()", errno);
        else if (e == UNCIL_POSIX_ERR_EXISTS)
            return unc_throwexc(w, "value", "fs.lcopy(): destination exists");
        else if (e == UNCIL_POSIX_ERR_DIRONFILE)
            e = unc_throwexc(w, "value",
                "fs.lcopy(): cannot copy file on directory");
        else if (e == UNCIL_POSIX_ERR_FILEONDIR)
            e = unc_throwexc(w, "value",
                "fs.lcopy(): cannot copy directory on file");
        else if (e == UNCIL_POSIX_ERR_DIRONDIR)
            e = unc_throwexc(w, "value",
                "fs.lcopy(): cannot copy directory on non-empty directory");
        return e;
#else
        if (!overwrite && unc__ansiexists(fn2))
            return unc_throwexc(w, "value", "fs.lcopy(): destination exists");
        if (unc__ansicopy(fn, fn2, overwrite))
            return unc__fs_makeerr(w, "fs.lcopy()", errno);
        return 0;
#endif
    }
}

Unc_RetVal unc__lib_fs_rcopy(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn, *fn2;
    int metadata, overwrite, follow;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    metadata = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(metadata)) return metadata;
    overwrite = unc_getbool(w, &args.values[3], 0);
    if (UNCIL_IS_ERR(overwrite)) return overwrite;
    follow = unc_getbool(w, &args.values[4], 0);
    if (UNCIL_IS_ERR(overwrite)) return follow;
    {
#if UNCIL_IS_POSIX
        e = unc__rcopy_posix(w, fn, fn2, metadata, overwrite, follow);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.rcopy()", errno);
        return e;
#else
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

Unc_RetVal unc__lib_fs_move(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn, *fn2;
    int overwrite;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    overwrite = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(overwrite)) return overwrite;
    {
#if UNCIL_IS_POSIX
        e = unc__move_posix(&w->world->alloc, fn, fn2, overwrite);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.move()", errno);
        else if (e == UNCIL_POSIX_ERR_EXISTS)
            return unc_throwexc(w, "value", "fs.move(): destination exists");
        return e;
#else
        if (!overwrite && unc__ansiexists(fn2))
            return unc_throwexc(w, "value", "fs.move(): destination exists");
        if (rename(fn, fn2))
            return unc__fs_makeerr(w, "fs.move()", errno);
        return 0;
#endif
    }
}

Unc_RetVal unc__lib_fs_link(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    const char *fn, *fn2;
    int symbolic;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    symbolic = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(symbolic)) return symbolic;
    {
#if UNCIL_IS_POSIX
        e = unc__link_posix(&w->world->alloc, fn, fn2, symbolic);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            return unc__fs_makeerr(w, "fs.link()", errno);
        return e;
#else
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
}

#if UNCIL_IS_POSIX
INLINE Unc_RetVal setint_(struct Unc_View *w, const char *s, mode_t m) {
    Unc_Value v = UNC_BLANK;
    if (sizeof(mode_t) > sizeof(Unc_Int) &&
            (m < (mode_t)UNC_INT_MIN || m > (mode_t)UNC_INT_MAX))
        return 0;
    unc_setint(w, &v, m);
    return unc_setpublicc(w, s, &v);
}
#endif

Unc_RetVal uncilmain_fs(struct Unc_View *w) {
    Unc_RetVal e;
    char buf[2];
    buf[1] = 0;
    {
        Unc_Value v = UNC_BLANK;
        buf[0] = UNCIL_DIRSEP;
        e = unc_newstringc(w, &v, buf);
        if (e) return e;
        e = unc_setpublicc(w, "sep", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
        buf[0] = UNCIL_PATHSEP;
        e = unc_newstringc(w, &v, buf);
        if (e) return e;
        e = unc_setpublicc(w, "pathsep", &v);
        if (e) return e;
    }
#if UNCIL_IS_POSIX
    if (!e) e = setint_(w, "S_IRUSR", S_IRUSR);
    if (!e) e = setint_(w, "S_IWUSR", S_IWUSR);
    if (!e) e = setint_(w, "S_IXUSR", S_IXUSR);
    if (!e) e = setint_(w, "S_IRGRP", S_IRGRP);
    if (!e) e = setint_(w, "S_IWGRP", S_IWGRP);
    if (!e) e = setint_(w, "S_IXGRP", S_IXGRP);
    if (!e) e = setint_(w, "S_IROTH", S_IROTH);
    if (!e) e = setint_(w, "S_IWOTH", S_IWOTH);
    if (!e) e = setint_(w, "S_IXOTH", S_IXOTH);
    if (!e) e = setint_(w, "S_ISUID", S_ISUID);
    if (!e) e = setint_(w, "S_ISGID", S_ISGID);
#ifdef S_ISVTX
    if (!e) e = setint_(w, "S_ISVTX", S_ISVTX);
#endif
#endif
    if (e) return e;
    e = unc_exportcfunction(w, "exists", &unc__lib_fs_exists,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "isdir", &unc__lib_fs_isdir,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "isfile", &unc__lib_fs_isfile,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "getcwd", &unc__lib_fs_getcwd,
                            UNC_CFUNC_DEFAULT,
                            0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "setcwd", &unc__lib_fs_setcwd,
                            UNC_CFUNC_DEFAULT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "getext", &unc__lib_fs_getext,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "normpath", &unc__lib_fs_normpath,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "abspath", &unc__lib_fs_abspath,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "realpath", &unc__lib_fs_realpath,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "relpath", &unc__lib_fs_relpath,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "pathjoin", &unc__lib_fs_pathjoin,
                            UNC_CFUNC_CONCURRENT,
                            1, 1, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "pathprefix", &unc__lib_fs_pathprefix,
                            UNC_CFUNC_CONCURRENT,
                            1, 1, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "pathsplit", &unc__lib_fs_pathsplit,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "basename", &unc__lib_fs_basename,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "dirname", &unc__lib_fs_dirname,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "stat", &unc__lib_fs_stat,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "lstat", &unc__lib_fs_lstat,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "scan", &unc__lib_fs_scan,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "mkdir", &unc__lib_fs_mkdir,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "chmod", &unc__lib_fs_chmod,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "remove", &unc__lib_fs_remove,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "move", &unc__lib_fs_move,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "copy", &unc__lib_fs_copy,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "lcopy", &unc__lib_fs_lcopy,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "rcopy", &unc__lib_fs_rcopy,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "rdestroy", &unc__lib_fs_rdestroy,
                            UNC_CFUNC_CONCURRENT,
                            1, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    e = unc_exportcfunction(w, "link", &unc__lib_fs_link,
                            UNC_CFUNC_CONCURRENT,
                            2, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;
    return 0;
}
