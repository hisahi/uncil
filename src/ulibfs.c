/*******************************************************************************
 
Uncil -- builtin fs library impl

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

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <limits.h>

#include <errno.h>
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
#endif /* UNCIL_C99 */
char *realpath(const char *RESTRICT path, char *RESTRICT resolved_path);
#endif /* __GNUC__ */
#elif UNCIL_IS_WINDOWS
#undef BOOL
#include <Windows.h>
#endif /* UNCIL_IS_... */

INLINE Unc_RetVal unc0_fs_makeerr(Unc_View *w, const char *prefix, int err) {
    return unc0_std_makeerr(w, "system", prefix, err);
}

INLINE Unc_RetVal unc0_fs_makeerr_maybe(Unc_View *w, Unc_RetVal e,
                                        const char *prefix, int err) {
    if (UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
        e = unc0_fs_makeerr(w, prefix, err);
    return e;
}

static int unc0_ansiexists(const char *fn) {
    FILE *fd = fopen(fn, "rb");
    if (fd) {
        fclose(fd);
        return 1;
    } else
        return 0;
}

INLINE int unc0_ansifcopy(FILE *f1, FILE *f0) {
    char buf[BUFSIZ];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f0)))
        if (!fwrite(buf, n, 1, f1))
            return -1;
    return 0;
}

static int unc0_ansicopy(const char *f0, const char *f1, int overwrite) {
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
        if (unc0_ansifcopy(b, a) || ferror(a) || ferror(b))
            goto unc0_ansicopy_fail;
    }
    fclose(b);
    fclose(a);
    return 0;
unc0_ansicopy_fail:
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

static Unc_RetVal unc0_getcwd(struct unc0_strbuf *buffer) {
    Unc_Size pathSize = PATH_MAX;
    char *buf, *ptr;
    do {
        buf = (char *)unc0_strbuf_reserve_clear(buffer, pathSize);
        if (!buf) return UNCIL_ERR_MEM;
        ptr = getcwd(buf, pathSize);
        if (!ptr && errno != ERANGE) return UNCIL_ERR_IO_UNDERLYING;
        pathSize <<= 1;
    } while (!ptr);
    buffer->length = strlen(ptr);
    return 0;
}

static Unc_RetVal unc0_getcwdv(Unc_View *w, Unc_Value *v) {
    struct unc0_strbuf buffer;
    Unc_RetVal e;
    unc0_strbuf_init(&buffer, &w->world->alloc, Unc_AllocLibrary);
    e = unc0_getcwd(&buffer);
    if (e)
        e = unc0_fs_makeerr_maybe(w, e, "fs.getcwd()", errno);
    else
        e = unc0_buftostring(w, v, &buffer);
    unc0_strbuf_free(&buffer);
    return e;
}

static Unc_RetVal unc0_getext_posix(struct unc0_strbuf *buffer,
                                    const char *path) {
    const char *ext = strrchr(path, '/');
    ext = ext ? ext + 1 : path;
    ext = strrchr(ext, '.');
    buffer->length = 0;
    return !ext ? 0 : unc0_strbuf_putn(buffer, strlen(ext), (const byte *)ext);
}

static Unc_RetVal unc0_normpath_posix(struct unc0_strbuf *buffer,
                                      const char *path) {
    Unc_Size root;
    const char *prev;
    size_t n = 0;
    while (*path == '/')
        ++n, ++path;
    if (n) {
        /* more than two leading slashes shall be treated as a single slash */
        if (n > 2) n = 1;
        if (unc0_strbuf_putn(buffer, n, (const byte *)(path - n)))
            return UNCIL_ERR_MEM;
    }
    root = buffer->length;
    prev = NULL;
    while (prev || *path) {
        if (*path && *path != '/') {
            if (!prev) prev = path;
            ++path;
            continue;
        }
        if (*path == '/' && !prev) {
            /* buffer->length = root; */
            ++path;
            continue;
        }
        if (*prev == '.') {
            if (prev + 1 == path) {
                goto unc0_normpath_posix_noappend;
            } else if (prev[1] == '.' && prev + 2 == path) {
                Unc_Size out_n = buffer->length;
                const char *out_p = (const char *)buffer->buffer;
                while (out_n > root) {
                    --out_n;
                    if (out_p[out_n] == '/')
                        break;
                }
                buffer->length = out_n;
                goto unc0_normpath_posix_noappend;
            }
            goto unc0_normpath_posix_append;
        } else
unc0_normpath_posix_append:
        {
            if (buffer->length != root)
                if (unc0_strbuf_put1(buffer, '/'))
                    return UNCIL_ERR_MEM;
            if (unc0_strbuf_putn(buffer, path - prev, (const byte *)prev))
                return UNCIL_ERR_MEM;
        }
unc0_normpath_posix_noappend:
        if (*path) {
            ASSERT(*path == '/');
            ++path;
            prev = NULL;
            continue;
        } else
            break;
    }

    return 0;
}

static Unc_RetVal unc0_abspath_posix(struct unc0_strbuf *buffer,
                                     const char *path) {
    Unc_RetVal e;
    struct unc0_strbuf buffer2;
    if (*path == '/')
        return unc0_normpath_posix(buffer, path);
    e = unc0_getcwd(buffer);
    if (e) return e;
    if (buffer->length > 0 && buffer->buffer[buffer->length - 1] == '/') {
        e = unc0_strbuf_put1(buffer, '/');
        if (e) return e;
    }
    e = unc0_strbuf_putn(buffer, strlen(path) + 1, (const byte *)path);
    if (e) return e;
    unc0_strbuf_init_forswap(&buffer2, buffer);
    e = unc0_normpath_posix(&buffer2, (const char *)buffer->buffer);
    if (e) {
        unc0_strbuf_free(&buffer2);
        return e;
    }
    unc0_strbuf_swap(buffer, &buffer2);
    return 0;
}

#define realpath_free free
static Unc_RetVal unc0_realpath_posix(struct unc0_strbuf *buffer,
                                      const char *path, int strict) {
    Unc_RetVal e;
    char *p;
    struct unc0_strbuf buffer2;
    int alloc = 1;
    e = unc0_abspath_posix(buffer, path);
    if (e) return e;
    unc0_strbuf_init_forswap(&buffer2, buffer);
    p = realpath((const char *)buffer->buffer, NULL);
    if (!p) {
        char *out = (char *)unc0_strbuf_reserve_next(&buffer2, PATH_MAX);
        if (!out) return UNCIL_ERR_MEM;
        alloc = 0;
        p = realpath((const char *)buffer->buffer, out);
        e = errno;
        if (p) buffer2.length = strlen(p);
    }
    if (!p) {
        unc0_strbuf_free(&buffer2);
        switch (e) {
        case ENOENT:
            return strict ? UNCIL_ERR_IO_UNDERLYING : 0;
        case ENAMETOOLONG:
        case ENOMEM:
            return UNCIL_ERR_MEM;
        case ENOSYS:
            return UNCIL_ERR_LOGIC_NOTSUPPORTED;
        default:
            return UNCIL_ERR_IO_UNDERLYING;
        }
    }
    e = 0;
    if (alloc) {
        e = unc0_strbuf_putn(&buffer2, strlen(p) + 1, (const byte*)p);
        realpath_free(p);
    }
    unc0_strbuf_swap(buffer, &buffer2);
    return e;
}

static Unc_RetVal unc0_relpath_posix(struct unc0_strbuf *buffer,
                                     const char *path, const char *base) {
    Unc_RetVal e;
    size_t prefix;
    ASSERT(*path == '/' && *base == '/');
    prefix = strspn(path, base);
    if (!base[prefix] && !path[prefix]) {
        /* complete */
        e = unc0_strbuf_put1(buffer, '.');
    } else if (!base[prefix] && path[prefix] == '/') {
        path += prefix + 1;
        e = unc0_strbuf_putn(buffer, strlen(path), (const byte *)path);
    } else if (!path[prefix] && (base[prefix] == '/' || prefix <= 1)) {
        size_t nodes = 0;
        if (base[prefix] == '/')
            base += prefix;
        while (*base)
            nodes += *base++ == '/';
        if (nodes) {
            if ((e = unc0_strbuf_putn(buffer, 2, (const byte *)"..")))
                return e;
            while (--nodes) {
                if ((e = unc0_strbuf_putn(buffer, 3, (const byte *)"/..")))
                    return e;
            }
        }
    } else {
        size_t nodes = 0;
        ASSERT(prefix >= 1);
        while (path[prefix] != '/')
            --prefix;
        path += prefix, base += prefix;
        while (*base)
            nodes += *base++ == '/';
        if (nodes) {
            if ((e = unc0_strbuf_putn(buffer, 2, (const byte *)"..")))
                return e;
            while (--nodes) {
                if ((e = unc0_strbuf_putn(buffer, 3, (const byte *)"/..")))
                    return e;
            }
        } else
            ++path;
        e = unc0_strbuf_putn(buffer, strlen(path), (const byte *)path);
    }
    return 0;
}

static Unc_Size unc0_pathprefix_posix(Unc_Size n,
                                      const char *a, const char *b) {
    Unc_Size i = 0;
    while (i < n && a[i] == b[i])
        ++i;
    if (!((!a[i] || a[i] == '/') && (!b[i] || b[i] == '/')))
        while (i > 0 && a[i] != '/')
            --i;
    return i;
}

struct unc0_pathsplit_buf {
    const char *s;
    int root;
    int final;
};

static int unc0_pathsplit_init(struct unc0_pathsplit_buf *b, const char *fn) {
    b->s = fn;
    b->root = 1;
    b->final = 0;
    return 0;
}

static int unc0_pathsplit_next(struct unc0_pathsplit_buf *b,
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

static Unc_RetVal unc0_stat_posix_obj(Unc_View *w, Unc_Value *v,
                                      const char *fn, struct stat *st) {
    Unc_RetVal e;
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
    
    if ((e = unc_newstringc(w, &tmp, s))) goto unc0_stat_posix_obj_err;
    if ((e = unc_setattrc(w, v, "type", &tmp))) goto unc0_stat_posix_obj_err;

    if (sizeof(off_t) > sizeof(Unc_Int) &&
            (st->st_size < (off_t)UNC_INT_MIN
                || st->st_size > (off_t)UNC_INT_MAX)) {
        e = unc_throwexc(w, "system", "size too large for this "
                                      "Uncil version to handle");
        goto unc0_stat_posix_obj_err;
    }
    unc_setint(w, &tmp, st->st_size);
    if ((e = unc_setattrc(w, v, "size", &tmp))) goto unc0_stat_posix_obj_err;

    unc_setint(w, &tmp, st->st_atime);
    if ((e = unc_setattrc(w, v, "atime", &tmp))) goto unc0_stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_mtime);
    if ((e = unc_setattrc(w, v, "mtime", &tmp))) goto unc0_stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_ctime);
    if ((e = unc_setattrc(w, v, "ctime", &tmp))) goto unc0_stat_posix_obj_err;

    unc_setint(w, &tmp, st->st_ino);
    if ((e = unc_setattrc(w, v, "inode", &tmp))) goto unc0_stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_nlink);
    if ((e = unc_setattrc(w, v, "links", &tmp))) goto unc0_stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_dev);
    if ((e = unc_setattrc(w, v, "device", &tmp))) goto unc0_stat_posix_obj_err;

    unc_setint(w, &tmp, st->st_uid);
    if ((e = unc_setattrc(w, v, "uid", &tmp))) goto unc0_stat_posix_obj_err;
    unc_setint(w, &tmp, st->st_gid);
    if ((e = unc_setattrc(w, v, "gid", &tmp))) goto unc0_stat_posix_obj_err;

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
            goto unc0_stat_posix_obj_err;
    }

unc0_stat_posix_obj_err:
    VCLEAR(w, &tmp);
    return e;
}

static Unc_RetVal unc0_stat_posix_auto(Unc_View *w, Unc_Value *v,
                                       const char *fn, int linkstat) {
    struct stat st;
    Unc_RetVal e = linkstat ? lstat(fn, &st) : stat(fn, &st);
    if (e) return unc0_fs_makeerr(w, "stat", errno);
    return unc0_stat_posix_obj(w, v, fn, &st);
}

struct unc0_scan_posix_buf {
    DIR *dir;
    Unc_Size baselen;
    char *base;
};

static Unc_RetVal unc0_scan_posix_init(Unc_Allocator *alloc,
            struct unc0_scan_posix_buf *buf, const char *fn) {
    DIR *dir;
    size_t namelen = strlen(fn);
    int addslash = namelen && fn[namelen - 1] != '/';
    buf->baselen = namelen + addslash;
    buf->base = unc0_mmalloc(alloc, Unc_AllocLibrary, buf->baselen);
    if (!buf->base) return UNCIL_ERR_MEM;
    strcpy(buf->base, fn);
    if (addslash) buf->base[namelen] = '/';
    dir = opendir(fn);
    if (!dir) {
        unc0_mmfree(alloc, buf->base);
        return UNCIL_ERR_IO_UNDERLYING;
    }
    buf->dir = dir;
    return 0;
}

static Unc_RetVal unc0_scan_posix_next(struct unc0_strbuf *sbuf,
            struct unc0_scan_posix_buf *buf) {
    Unc_RetVal e;
    struct dirent *dir;
    errno = 0;
    dir = readdir(buf->dir);
    if (!dir) return errno ? UNCIL_ERR_IO_UNDERLYING : 1;
    sbuf->length = 0;
    e = unc0_strbuf_putn(sbuf, buf->baselen, (const byte *)buf->base);
    if (e) return e;
    return unc0_strbuf_putn(sbuf, strlen(dir->d_name),
                                  (const byte *)dir->d_name);
}

static Unc_RetVal unc0_scan_posix_destr(Unc_Allocator *alloc,
                                        struct unc0_scan_posix_buf *d) {
    closedir(d->dir);
    unc0_mmfree(alloc, d->base);
    return 0;
}

static Unc_RetVal unc0_scan_posix_destrw(Unc_View *w, size_t n, void *data) {
    return unc0_scan_posix_destr(&w->world->alloc, data);
}

#define DEFAULT_MODE (S_IRWXU | S_IRWXG | S_IRWXO)

#define UNCIL_POSIX_ERR_EXISTS 0x0180
#define UNCIL_POSIX_ERR_DIRONFILE 0x0181
#define UNCIL_POSIX_ERR_FILEONDIR 0x0182
#define UNCIL_POSIX_ERR_DIRONDIR 0x0183

static int unc0_fdcopy_eof(int fd1, int fd0) {
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

INLINE int unc0_posix_samefile(const struct stat *st0,
                               const struct stat *st1) {
    return st0->st_rdev == st1->st_rdev && st0->st_ino == st1->st_ino;
}

#define EXBITS (S_IXUSR | S_IXGRP | S_IXOTH)

static Unc_RetVal unc0_copyfile_posix(const char *fn,
                                      const char *d, int overwrite) {
    struct stat st0, st1;
    int fd0, fd1;
    mode_t mode;
    Unc_RetVal e;
    int backup_errno;
    if (lstat(fn, &st0)) return UNCIL_ERR_IO_UNDERLYING;
    if (S_ISDIR(st0.st_mode)) {
        mode_t mode = DEFAULT_MODE;
        if (lstat(fn, &st1)) {
            switch (errno) {
            case ENOENT:
                if (!mkdir(d, mode))
                    return 1;
            default:
                return UNCIL_ERR_IO_UNDERLYING;
            }
        }
        if (!overwrite) return UNCIL_POSIX_ERR_EXISTS;
        if (!S_ISDIR(st1.st_mode)) return UNCIL_POSIX_ERR_DIRONFILE;
        if (rmdir(d)) return UNCIL_ERR_IO_UNDERLYING;
        if (mkdir(d, mode)) return UNCIL_ERR_IO_UNDERLYING;
        return 1;
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
        backup_errno = errno;
        goto unc0_copyfile_posix_fail0;
    }
    if (fstat(fd1, &st1)) goto unc0_copyfile_posix_fail;
    if (unc0_posix_samefile(&st0, &st1)) {
        e = 0; /* same file! do not do anything */
    } else if (S_ISDIR(st1.st_mode)) {
        e = UNCIL_POSIX_ERR_FILEONDIR;
    } else if (ftruncate(fd1, 0)) {
        goto unc0_copyfile_posix_fail; /* truncate failed */
    } else if (unc0_fdcopy_eof(fd1, fd0)) {
        goto unc0_copyfile_posix_fail; /* copy failed */
    } else {
        e = 1; /* copy OK */
    }
    close(fd0);
    close(fd1);
    return e;
unc0_copyfile_posix_fail:
    backup_errno = errno;
    unlink(d);
    close(fd1);
unc0_copyfile_posix_fail0:
    close(fd0);
    errno = backup_errno;
    return UNCIL_ERR_IO_UNDERLYING;
}

static Unc_RetVal unc0_copymeta_posix(const char *fn, const char *d) {
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

static int unc0_exists_posix(const char *fn) {
    struct stat st;
    return !lstat(fn, &st);
}

static Unc_RetVal unc0_mkdir_posix_rec(Unc_Allocator *alloc,
                        const char *fn, mode_t mode) {
    int old_errno = errno;
    size_t l = strlen(fn);
    int sink = 1;
    size_t slashes = 0;
    char *p = unc0_mmalloc(alloc, Unc_AllocString, l + 1);
    if (!p) return UNCIL_ERR_MEM;
    strcpy(p, fn);
    while (sink) {
        char *slash = strrchr(p, '/');
        if (!slash) {
            unc0_mmfree(alloc, p);
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
                unc0_mmfree(alloc, p);
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
                unc0_mmfree(alloc, p);
                return UNCIL_ERR_IO_UNDERLYING;
            }
        }
    }
    unc0_mmfree(alloc, p);
    return 0;
}

static Unc_RetVal unc0_mkdir_posix(Unc_Allocator *alloc,
                        const char *fn, mode_t mode, int parents) {
    if (mkdir(fn, mode)) {
        switch (errno) {
        case EEXIST:
            if (parents) return 0;
        case ENOENT:
            if (parents) return unc0_mkdir_posix_rec(alloc, fn, mode);
        default:
            return UNCIL_ERR_IO_UNDERLYING;
        }
    }
    return 0;
}

static Unc_RetVal unc0_chmod_posix(Unc_Allocator *alloc,
                        const char *fn, mode_t mode) {
    return chmod(fn, mode) ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc0_remove_posix(Unc_Allocator *alloc, const char *fn) {
    return remove(fn) ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc0_move_posix(Unc_Allocator *alloc,
                        const char *fn, const char *d, int overwrite) {
    if (!overwrite && unc0_exists_posix(d))
        return UNCIL_POSIX_ERR_EXISTS; /* TODO TOCTOU?? */
    return rename(fn, d) ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc0_link_posix(Unc_Allocator *alloc,
                        const char *fn, const char *n, int symbolic) {
    return (symbolic ? symlink(fn, n) : link(fn, n))
            ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc0_copy_posix(const char *fn, const char *d,
                                  int metadata, int overwrite) {
    Unc_RetVal e = unc0_copyfile_posix(fn, d, overwrite);
    if (!UNCIL_IS_ERR(e) && e && metadata)
        return unc0_copymeta_posix(fn, d);
    return UNCIL_IS_ERR(e) ? UNCIL_ERR_IO_UNDERLYING : 0;
}

static Unc_RetVal unc0_copy2_posix(const char *fn, const char *d,
                                   int metadata, int overwrite) {
    Unc_RetVal e = unc0_copy_posix(fn, d, metadata, overwrite);
    if (!overwrite && (e == UNCIL_POSIX_ERR_EXISTS ||
                      (e == UNCIL_ERR_IO_UNDERLYING && errno == EEXIST)))
        e = 0;
    return e;
}

struct unc0_rcopy_posix_buf {
    struct unc0_strbuf src;
    struct unc0_strbuf dst;
    const char *dsto;
    Unc_Size dsto_n;
    int metadata;
    int overwrite;
    int follow;
    Unc_Size recurse;
};

static Unc_RetVal unc0_rcopy_posix_mkdir(const byte *fn, mode_t mode,
                                         int *created) {
    if (mkdir((const char *)fn, mode)) {
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

static Unc_RetVal unc0_rcopy_posix_do(struct unc0_rcopy_posix_buf *buf,
                                      struct stat *pst) {
    Unc_RetVal e;
    DIR *d;
    struct dirent *de;
    Unc_Size srcn, dstn;

    if (!buf->recurse) return UNCIL_ERR_TOODEEP;
    --buf->recurse;
    
    d = opendir((const char *)buf->src.buffer);
    if (!d) return UNCIL_ERR_IO_UNDERLYING;
    srcn = buf->src.length, dstn = buf->dst.length;
    buf->src.buffer[srcn - 1] = '/';
    
    e = 0;
    while ((errno = 0, de = readdir(d))) {
        Unc_Size sl;
        byte *oldsrc = NULL;
        Unc_Size oldsrc_c;
        
        /* skip . and .. */
        if (de->d_name[0] == '.') {
            if (!de->d_name[1]) continue;
            if (!strcmp(de->d_name + 1, ".")) continue;
        }
        
        sl = strlen(de->d_name) + 1;
        buf->src.length = srcn;
        e = unc0_strbuf_putn(&buf->src, sl, (const byte *)de->d_name);
        if (e) break;
        if (lstat((const char *)buf->src.buffer, pst)) break;

        buf->dst.length = dstn;
        e = unc0_strbuf_putn(&buf->dst, sl, (const byte *)de->d_name);
        if (e) break;

        if (buf->follow) {
            if (S_ISLNK(pst->st_mode)) {
                struct stat tst;
                if (stat((const char *)buf->src.buffer, &tst)) break;
            }
            while (S_ISLNK(pst->st_mode)) {
                size_t in = 64;
                byte *ip = unc0_malloc(buf->src.alloc, Unc_AllocLibrary, in),
                     *ip2;
                ssize_t ir;
                if (!ip)
                    e = UNCIL_ERR_MEM;
                else
                    for (;;) {
                        ir = readlink((const char *)buf->src.buffer,
                                      (char *)ip, in);
                        if (ir < in) {
                            ip[ir] = 0;
                            break;
                        }
                        ip2 = unc0_mrealloc(buf->src.alloc, Unc_AllocLibrary,
                                            ip, in, in << 1);
                        in <<= 1;
                        if (!ip2) {
                            e = UNCIL_ERR_MEM;
                            break;
                        }
                        ip = ip2;
                    }
                if (e || lstat((const char *)ip, pst))
                    goto unc0_rcopy_posix_do_link_break;
                if (S_ISDIR(pst->st_mode)) {
                    int loop = 0;
                    if (!unc0_memcmp(ip, buf->dsto, buf->dsto_n) &&
                            (!ip[buf->dsto_n] || ip[buf->dsto_n] == '/'))
                        loop = 1;
                    if (ir <= buf->src.length &&
                            !unc0_memcmp(ip, buf->src.buffer, ir) &&
                            (!buf->src.buffer[ir]
                                || buf->src.buffer[ir] == '/'))
                        loop = 1;
                    if (oldsrc && ir <= srcn
                            && !unc0_memcmp(ip, oldsrc, ir) &&
                            (!oldsrc[ir] || oldsrc[ir] == '/'))
                        loop = 1;
                    if (loop) {
                        errno = ELOOP;
                        goto unc0_rcopy_posix_do_link_break;
                    }
                }
                if (!oldsrc)
                    oldsrc = buf->src.buffer, oldsrc_c = buf->src.capacity;
                buf->src.buffer = ip,
                    buf->src.length = ir,
                    buf->src.capacity = in;
            }
unc0_rcopy_posix_do_link_break:
            ;
        }
        if (S_ISDIR(pst->st_mode)) {
            Unc_Size stn = buf->src.length - 1, dtn = buf->dst.length - 1;
            int created;
            e = unc0_rcopy_posix_mkdir(buf->dst.buffer,
                                       DEFAULT_MODE, &created);
            if (e) break;
            buf->dst.buffer[dtn] = '/';
            e = unc0_rcopy_posix_do(buf, pst);
            if (e) break;
            if (!buf->metadata || created || buf->overwrite) continue;
            buf->src.buffer[stn] = buf->dst.buffer[dtn] = 0;
            e = unc0_copymeta_posix((const char *)buf->src.buffer,
                                    (const char *)buf->dst.buffer);
        } else {
            e = unc0_copy2_posix((const char *)buf->src.buffer,
                                 (const char *)buf->dst.buffer,
                                 buf->metadata, buf->overwrite);
        }
        if (e) break;
        if (oldsrc) {
            unc0_mfree(buf->src.alloc, buf->src.buffer, buf->src.capacity);
            buf->src.buffer = oldsrc;
            buf->src.capacity = oldsrc_c;
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

static Unc_RetVal unc0_rcopy_posix(Unc_View *w,
                        const char *fn, const char *d,
                        int metadata, int overwrite, int follow) {
    Unc_RetVal e;
    struct unc0_rcopy_posix_buf buf;
    struct stat st;
    int dstdir;
    const char *bn;
    if (stat(fn, &st)) return UNCIL_ERR_IO_UNDERLYING;
    unc0_strbuf_init(&buf.src, &w->world->alloc, Unc_AllocLibrary);
    unc0_strbuf_init(&buf.dst, &w->world->alloc, Unc_AllocLibrary);
    buf.dsto = d;
    buf.dsto_n = strlen(d);
    buf.metadata = metadata;
    buf.overwrite = overwrite;
    buf.follow = follow;
    buf.recurse = unc_recurselimit(w);
    if (unc0_strbuf_putn(&buf.dst, buf.dsto_n + 1, (const byte *)d)) {
        unc0_strbuf_free(&buf.dst);
        return UNCIL_ERR_MEM;
    }
    --buf.dst.length;
    dstdir = buf.dst.length && buf.dst.buffer[buf.dst.length - 1] == '/';
    if (!S_ISDIR(st.st_mode)) {
        if (dstdir) {
            errno = ENOTDIR;
            e = UNCIL_ERR_IO_UNDERLYING;
        } else {
            e = unc0_strbuf_put1(&buf.dst, '/');
            bn = strrchr(fn, '/');
            fn = bn ? bn + 1 : fn;
            ASSERT(*fn);
            if (!e) e = unc0_strbuf_putn(&buf.dst,
                                         strlen(bn) + 1, (const byte *)bn);
            if (!e)
                e = unc0_copy2_posix(fn, (const char *)buf.dst.buffer,
                                     metadata, overwrite);
        }
    } else if (unc0_strbuf_putn(&buf.src, strlen(fn) + 1, (const byte *)fn)) {
        e = UNCIL_ERR_MEM;
    } else {
        if (buf.src.length > 1 && buf.src.buffer[buf.src.length - 2] == '/')
            buf.src.buffer[--buf.src.length - 1] = 0;
        bn = strrchr((const char *)buf.src.buffer, '/');
        fn = bn ? bn + 1 : fn;
        if (!dstdir)
            buf.dst.buffer[buf.dst.length++] = '/';
        e = unc0_strbuf_putn(&buf.dst, strlen(fn) + 1, (const byte *)fn);
        if (!e) {
            int created;
            Unc_Size stn = buf.src.length - 1, dtn = buf.dst.length - 1;
            buf.dst.buffer[dtn] = 0;
            e = unc0_rcopy_posix_mkdir(buf.dst.buffer, DEFAULT_MODE, &created);
            if (!e)
                buf.dst.buffer[dtn] = '/',
                    e = unc0_rcopy_posix_do(&buf, &st);
            if (!e && buf.metadata && (created || buf.overwrite)) {
                buf.src.buffer[stn] = buf.dst.buffer[dtn] = 0;
                e = unc0_copymeta_posix((const char *)buf.src.buffer,
                                        (const char *)buf.dst.buffer);
            }
        }
        unc0_strbuf_free(&buf.src);
    }
    unc0_strbuf_free(&buf.dst);
    return e;
}

static Unc_RetVal unc0_rdestroy_posix_do(struct unc0_rcopy_posix_buf *buf,
                                         struct stat *pst) {
    Unc_RetVal e;
    DIR *d;
    struct dirent *de;
    Unc_Size srcn;

    if (!buf->recurse) return UNCIL_ERR_TOODEEP;
    --buf->recurse;
    
    d = opendir((const char *)buf->src.buffer);
    if (!d) return UNCIL_ERR_IO_UNDERLYING;
    buf->src.buffer[buf->src.length - 1] = '/';
    srcn = buf->src.length;
    
    e = 0;
    while ((errno = 0, de = readdir(d))) {
        Unc_Size sl;
        /* skip . and .. */
        if (de->d_name[0] == '.') {
            if (!de->d_name[1]) continue;
            if (!strcmp(de->d_name + 1, ".")) continue;
        }
        sl = strlen(de->d_name) + 1;
        buf->src.length = srcn;
        e = unc0_strbuf_putn(&buf->src, sl, (const byte *)de->d_name);
        if (e) break;
        if (lstat((const char *)buf->src.buffer, pst)) break;
        if (S_ISDIR(pst->st_mode))
            e = unc0_rdestroy_posix_do(buf, pst);
        else if (unlink((const char *)buf->src.buffer))
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
        buf->src.buffer[srcn - 1] = 0;
        if (rmdir((const char *)buf->src.buffer))
            e = UNCIL_ERR_IO_UNDERLYING;
    }
    ++buf->recurse;
    return e;
}

static Unc_RetVal unc0_rdestroy_posix(Unc_View *w, const char *fn) {
    Unc_RetVal e;
    struct unc0_rcopy_posix_buf buf;
    struct stat st;
    if (stat(fn, &st)) return UNCIL_ERR_IO_UNDERLYING;
    unc0_strbuf_init(&buf.src, &w->world->alloc, Unc_AllocLibrary);
    unc0_strbuf_init(&buf.dst, &w->world->alloc, Unc_AllocLibrary);
    buf.recurse = unc_recurselimit(w);
    if (!S_ISDIR(st.st_mode)) {
        e = unc0_remove_posix(&w->world->alloc, fn);
    } else if (unc0_strbuf_putn(&buf.src, strlen(fn) + 1, (const byte *)fn)) {
        e = UNCIL_ERR_MEM;
    } else {
        if (buf.src.length > 2 && buf.src.buffer[buf.src.length - 2] == '/')
            buf.src.buffer[--buf.src.length - 1] = 0;
        e = unc0_rdestroy_posix_do(&buf, &st);
        unc0_strbuf_free(&buf.src);
    }
    unc0_strbuf_free(&buf.dst);
    return e;
}

static Unc_RetVal uncl_fs_copy_do_posix(Unc_View *w,
                               const char *fn, const char *fn2,
                               int metadata, int overwrite) {
    Unc_RetVal e = unc0_copy_posix(fn, fn2, metadata, overwrite);
    if (e) {
        switch (e) {
        case UNCIL_POSIX_ERR_EXISTS:
            e = unc_throwexc(w, "value", "copy: destination exists");
            break;
        case UNCIL_POSIX_ERR_DIRONFILE:
            e = unc_throwexc(w, "value",
                "copy: cannot copy file on directory");
            break;
        case UNCIL_POSIX_ERR_FILEONDIR:
            e = unc_throwexc(w, "value",
                "copy: cannot copy directory on file");
            break;
        case UNCIL_POSIX_ERR_DIRONDIR:
            e = unc_throwexc(w, "value",
                "copy: cannot copy directory on non-empty directory");
            break;
        default:
            e = unc0_fs_makeerr_maybe(w, e, "copy", errno);
        }
    }
    return e;
}

static Unc_RetVal uncl_fs_scan_posix(Unc_View *w, Unc_Value *q,
                                     const char *fn) {
    Unc_RetVal e;
    struct unc0_scan_posix_buf *buf;
    e = unc_newopaque(w, q, NULL, sizeof(struct unc0_scan_posix_buf),
                (void **)&buf, &unc0_scan_posix_destrw, 0, NULL, 0, NULL);
    if (e) return e;
    e = unc0_scan_posix_init(&w->world->alloc, buf, fn);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.scan()", errno);
    return e;
}

static Unc_RetVal uncl_fs_basename_posix(Unc_View *w, Unc_Value *v,
                                         const char *fn) {
    const char *p = strrchr(fn, '/');
    p = p ? p + 1 : fn;
    return unc_newstringc(w, v, p);
}

static Unc_RetVal uncl_fs_dirname_posix(Unc_View *w, Unc_Value *v,
                                        const char *fn) {
    const char *p = strrchr(fn, '/');
    if (p == fn) ++p;
    return p ? unc_newstring(w, v, p - fn, fn) : 0;
}

static Unc_RetVal uncl_fs_posix_pathprefix(Unc_View *w, Unc_Value *v,
                                           const char *fn,
                                           Unc_Tuple args) {
    Unc_RetVal e;
    int abs;
    Unc_Size i, prefix;
    abs = *fn == '/';
    prefix = strlen(fn);
    for (i = 1; i < args.count; ++i) {
        const char *nfn;
        e = unc_getstringc(w, &args.values[i], &nfn);
        if (e) return e;
        if (abs != (*nfn == '/'))
            return unc_returnlocal(w, 0, v);
        prefix = unc0_pathprefix_posix(prefix, fn, nfn);
    }
    e = unc_newstring(w, v, prefix, fn);
    return e;
}

static Unc_RetVal uncl_fs_relpath_posix(Unc_View *w, Unc_Value *v,
                                        Unc_Value *v_base,
                                        const char *fn) {
    Unc_RetVal e;
    struct unc0_strbuf buf, buf1, buf2;
    
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);
    unc0_strbuf_init(&buf1, &w->world->alloc, Unc_AllocString);
    unc0_strbuf_init(&buf2, &w->world->alloc, Unc_AllocString);
    if (unc_gettype(w, v_base)) {
        const char *root;
        e = unc_getstringc(w, v_base, &root);
        if (e) goto fail;
        e = unc0_abspath_posix(&buf2, root);
        if (e) {
            e = unc0_fs_makeerr_maybe(w, e, "fs.relpath()", errno);
            goto fail;
        }
        if ((e = unc0_strbuf_put1(&buf2, 0)))
            goto fail;
    } else {
        e = unc0_getcwd(&buf2);
        if (e) {
            e = unc0_fs_makeerr_maybe(w, e, "fs.relpath() -> fs.getcwd()",
                                                errno);
            goto fail;
        }
    }

    e = unc0_abspath_posix(&buf1, fn);
    if (e) {
        e = unc0_fs_makeerr_maybe(w, e, "fs.relpath()", errno);
        goto fail;
    }
    if ((e = unc0_strbuf_put1(&buf1, 0)))
        goto fail;

    if ((e = unc0_relpath_posix(&buf, (const char *)buf1.buffer,
                                      (const char *)buf2.buffer)))
        goto fail;
    e = unc0_buftostring(w, v, &buf);
fail:
    unc0_strbuf_free(&buf1);
    unc0_strbuf_free(&buf2);
    unc0_strbuf_free(&buf);
    return e;
}

static Unc_RetVal uncl_fs_pathjoin_posix(Unc_View *w,
                                         struct unc0_strbuf *out,
                                         Unc_Tuple args) {
    Unc_RetVal e;
    Unc_Size i;
    for (i = 0; i < args.count; ++i) {
        const char *fn;
        e = unc_getstringc(w, &args.values[i], &fn);
        if (e) return e;
        if (*fn == '/') {
            out->length = 0;
        } else if (out->length && out->buffer[out->length - 1] != '/') {
            e = unc0_strbuf_put1(out, '/');
            if (e) return e;
        }
        e = unc0_strbuf_putn(out, strlen(fn), (const byte *)fn);
        if (e) return e;
    }
    return 0;
}

static int uncl_fs_exists_posix(Unc_View *w, const char *fn) {
    struct stat st;
    if (!stat(fn, &st))
        return 1;
    else {
        switch (errno) {
        case 0:
            /*break;*/
        case EACCES:
        case ENOENT:
        case ENOTDIR:
            return 0;
        default:
            return unc0_fs_makeerr(w, "fs.exists()", errno);
        }
    }
}

static int uncl_fs_isdir_posix(Unc_View *w, const char *fn) {
    struct stat st;
    if (!stat(fn, &st))
        return !!S_ISDIR(st.st_mode);
    else {
        switch (errno) {
        case 0:
            /*break;*/
        case EACCES:
        case ENOENT:
        case ENOTDIR:
            return 0;
        default:
            return unc0_fs_makeerr(w, "fs.isdir()", errno);
        }
    }
}

static int uncl_fs_isfile_posix(Unc_View *w, const char *fn) {
    struct stat st;
    if (!stat(fn, &st))
        return !!S_ISREG(st.st_mode);
    else {
        switch (errno) {
        case 0:
            /*break;*/
        case EACCES:
        case ENOENT:
        case ENOTDIR:
            return 0;
        default:
            return unc0_fs_makeerr(w, "fs.isfile()", errno);
        }
    }
}

#else /* not any platform*/

struct unc0_pathsplit_buf {
    char c_;
};

static Unc_RetVal unc0_pathsplit_init(struct unc0_pathsplit_buf *b,
                                      const char *fn) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

static Unc_RetVal unc0_pathsplit_next(struct unc0_pathsplit_buf *b,
                                      size_t *l, const char **p) {
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
}

#endif /* UNCIL_IS_POSIX */

Unc_RetVal uncl_fs_exists(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = uncl_fs_exists_posix(w, fn);
#else
    e = unc0_ansiexists(fn);
#endif

    if (!UNCIL_IS_ERR(e)) {
        unc_setbool(w, &v, !!e);
        e = unc_returnlocal(w, 0, &v);
    }
    return e;
}

Unc_RetVal uncl_fs_isdir(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    
#if UNCIL_IS_POSIX
    e = uncl_fs_isdir_posix(w, fn);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    if (!UNCIL_IS_ERR(e)) {
        unc_setbool(w, &v, !!e);
        e = unc_returnlocal(w, 0, &v);
    }
    return e;
}

Unc_RetVal uncl_fs_isfile(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = uncl_fs_isfile_posix(w, fn);
#else
    e = unc0_ansiexists(fn);
#endif

    if (!UNCIL_IS_ERR(e)) {
        unc_setbool(w, &v, !!e);
        e = unc_returnlocal(w, 0, &v);
    }
    return e;
}

Unc_RetVal uncl_fs_getcwd(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    Unc_Value v = UNC_BLANK;

#if UNCIL_IS_POSIX
    e = unc0_getcwdv(w, &v);
#else
    e = 0;
#endif
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_setcwd(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    if (chdir(fn)) e = unc0_fs_makeerr(w, "fs.setcwd()", errno);
    else e = unc0_getcwdv(w, &v);
#else
    (void)v;
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_getext(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    struct unc0_strbuf buf;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);

#if UNCIL_IS_POSIX
    e = unc0_getext_posix(&buf, fn);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    if (!e) e = unc0_buftostring(w, &v, &buf);
    unc0_strbuf_free(&buf);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_normpath(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    struct unc0_strbuf buf;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);

#if UNCIL_IS_POSIX
    e = unc0_normpath_posix(&buf, fn);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    if (!e) e = unc0_buftostring(w, &v, &buf);
    unc0_strbuf_free(&buf);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_abspath(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    struct unc0_strbuf buf;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);
#if UNCIL_IS_POSIX
    e = unc0_abspath_posix(&buf, fn);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.abspath()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    if (!e) e = unc0_buftostring(w, &v, &buf);
    unc0_strbuf_free(&buf);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_realpath(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    struct unc0_strbuf buf;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);

#if UNCIL_IS_POSIX
    e = unc0_realpath_posix(&buf, fn, 0);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.realpath()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    if (!e) e = unc0_buftostring(w, &v, &buf);
    unc0_strbuf_free(&buf);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_relpath(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = uncl_fs_relpath_posix(w, &v, &args.values[1], fn);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_pathjoin(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    struct unc0_strbuf out;
    Unc_Value v = UNC_BLANK;
    unc0_strbuf_init(&out, &w->world->alloc, Unc_AllocInternal);

#if UNCIL_IS_POSIX
    e = uncl_fs_pathjoin_posix(w, &out, args);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    if (!e) e = unc0_buftostring(w, &v, &out);
    unc0_strbuf_free(&out);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_pathprefix(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = uncl_fs_posix_pathprefix(w, &v, fn, args);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_pathsplit(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Size ai = 0, an;
    Unc_Value *ap;
    Unc_Value v = UNC_BLANK;
    size_t l;
    const char *p;
    struct unc0_pathsplit_buf b;

    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    an = 8;
    e = unc_newarray(w, &v, an, &ap);
    if (e) return e;

    e = unc0_pathsplit_init(&b, fn);
    if (!e) {
        while (unc0_pathsplit_next(&b, &l, &p)) {
            if (ai >= an) {
                Unc_Size aq = an + 8;
                e = unc_resizearray(w, &v, aq, &ap);
                if (e) break;
                an = aq;
            }
            e = unc_newstring(w, &ap[ai++], l, p);
            if (e) break;
        }
    }
    if (!e) e = unc_resizearray(w, &v, ai, &ap);
    unc_unlock(w, &v);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_basename(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = uncl_fs_basename_posix(w, &v, fn);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_dirname(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = uncl_fs_dirname_posix(w, &v, fn);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_stat(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = unc0_stat_posix_auto(w, &v, fn, 0);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_lstat(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = unc0_stat_posix_auto(w, &v, fn, 1);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    return unc_returnlocal(w, e, &v);
}

static Unc_RetVal uncl_fs_scan_next(Unc_View *w, Unc_Tuple args,
                                        void *udata) {
    Unc_RetVal e;
    struct unc0_scan_posix_buf *buf;
    Unc_Size bn;
    Unc_Value v = UNC_BLANK;
    struct unc0_strbuf sbuf;
    ASSERT(unc_boundcount(w) == 1);
    e = unc_lockopaque(w, unc_boundvalue(w, 0), &bn, (void **)&buf);
    if (e) return e;
    unc0_strbuf_init(&sbuf, &w->world->alloc, Unc_AllocString);

#if UNCIL_IS_POSIX
    e = unc0_scan_posix_next(&sbuf, buf);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.scan()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    if (!e) e = unc0_buftostring(w, &v, &sbuf);
    unc_unlock(w, unc_boundvalue(w, 0));
    unc0_strbuf_free(&sbuf);
    return unc_returnlocal(w, e, &v);
}

Unc_RetVal uncl_fs_scan(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Value v = UNC_BLANK, q = UNC_BLANK;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = uncl_fs_scan_posix(w, &q, fn);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif

    if (!e) {
        e = unc_newcfunction(w, &v, &uncl_fs_scan_next,
                        0, 0, 0, UNC_CFUNC_DEFAULT, NULL, 1, &q, 0, NULL,
                        "fs.scan(),next", NULL);
        VCLEAR(w, &q);
        e = unc_returnlocal(w, e, &v);
    }
    return e;
}

Unc_RetVal uncl_fs_mkdir(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
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
    e = unc0_mkdir_posix(&w->world->alloc, fn,
            got_mode ? (mode_t)mode : DEFAULT_MODE, parents);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.mkdir()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
    (void)got_mode; (void)mode;
#endif
    return e;
}

Unc_RetVal uncl_fs_chmod(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    Unc_Int mode = 0;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getint(w, &args.values[1], &mode);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = unc0_chmod_posix(&w->world->alloc, fn, (mode_t)mode);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.chmod()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    return e;
}

Unc_RetVal uncl_fs_remove(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = unc0_remove_posix(&w->world->alloc, fn);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.remove()", errno);
#else
    e = 0;
    if (remove(fn))
        return unc0_fs_makeerr(w, "fs.remove()", errno);
#endif
    return e;
}

Unc_RetVal uncl_fs_rdestroy(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;

#if UNCIL_IS_POSIX
    e = unc0_rdestroy_posix(w, fn);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.rdestroy()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    return e;
}

Unc_RetVal uncl_fs_copy(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn, *fn2;
    int metadata, overwrite;
    struct unc0_strbuf buf;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    metadata = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(metadata)) return metadata;
    overwrite = unc_getbool(w, &args.values[3], 0);
    if (UNCIL_IS_ERR(overwrite)) return overwrite;

#if UNCIL_IS_POSIX
    unc0_strbuf_init(&buf, &w->world->alloc, Unc_AllocString);
    e = unc0_realpath_posix(&buf, fn, 1);
    if (e) return unc0_fs_makeerr_maybe(w, e, "fs.copy()", errno);
    e = uncl_fs_copy_do_posix(w, (const char *)buf.buffer, fn2,
                            metadata, overwrite);
    unc0_strbuf_free(&buf);
#else
    (void)buf;
    e = 0;
    if (!overwrite && unc0_ansiexists(fn2))
        return unc_throwexc(w, "value", "fs.copy(): destination exists");
    if (unc0_ansicopy(fn, fn2, overwrite))
        return unc0_fs_makeerr(w, "fs.copy()", errno);
#endif
    return e;
}

Unc_RetVal uncl_fs_lcopy(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
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

#if UNCIL_IS_POSIX
    e = uncl_fs_copy_do_posix(w, fn, fn2, metadata, overwrite);
#else
    e = 0;
    if (!overwrite && unc0_ansiexists(fn2))
        return unc_throwexc(w, "value", "fs.lcopy(): destination exists");
    if (unc0_ansicopy(fn, fn2, overwrite))
        return unc0_fs_makeerr(w, "fs.lcopy()", errno);
#endif
    return e;
}

Unc_RetVal uncl_fs_rcopy(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
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

#if UNCIL_IS_POSIX
    e = unc0_rcopy_posix(w, fn, fn2, metadata, overwrite, follow);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.rcopy()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    return e;
}

Unc_RetVal uncl_fs_move(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn, *fn2;
    int overwrite;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    overwrite = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(overwrite)) return overwrite;
#if UNCIL_IS_POSIX
    e = unc0_move_posix(&w->world->alloc, fn, fn2, overwrite);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.move()", errno);
    else if (e == UNCIL_POSIX_ERR_EXISTS)
        return unc_throwexc(w, "value", "fs.move(): destination exists");
#else
    e = 0;
    if (!overwrite && unc0_ansiexists(fn2))
        return unc_throwexc(w, "value", "fs.move(): destination exists");
    if (rename(fn, fn2))
        return unc0_fs_makeerr(w, "fs.move()", errno);
#endif
    return e;
}

Unc_RetVal uncl_fs_link(Unc_View *w, Unc_Tuple args, void *udata) {
    Unc_RetVal e;
    const char *fn, *fn2;
    int symbolic;
    e = unc_getstringc(w, &args.values[0], &fn);
    if (e) return e;
    e = unc_getstringc(w, &args.values[1], &fn2);
    if (e) return e;
    symbolic = unc_getbool(w, &args.values[2], 0);
    if (UNCIL_IS_ERR(symbolic)) return symbolic;

#if UNCIL_IS_POSIX
    e = unc0_link_posix(&w->world->alloc, fn, fn2, symbolic);
    if (e) e = unc0_fs_makeerr_maybe(w, e, "fs.link()", errno);
#else
    e = UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    return e;
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

#define FN(x) &uncl_fs_##x, #x
static const Unc_ModuleCFunc lib[] = {
    { FN(exists),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(isdir),       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(isfile),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(getcwd),      0, 0, 0, UNC_CFUNC_DEFAULT    },
    { FN(setcwd),      1, 0, 0, UNC_CFUNC_DEFAULT    },
    { FN(getext),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(normpath),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(abspath),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(realpath),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(relpath),     1, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(pathjoin),    1, 0, 1, UNC_CFUNC_CONCURRENT },
    { FN(pathprefix),  1, 0, 1, UNC_CFUNC_CONCURRENT },
    { FN(pathsplit),   1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(basename),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(dirname),     1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(stat),        1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(lstat),       1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(scan),        1, 0, 0, UNC_CFUNC_CONCURRENT }, 
    { FN(mkdir),       1, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(chmod),       2, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(remove),      1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(move),        2, 1, 0, UNC_CFUNC_CONCURRENT },
    { FN(copy),        2, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(lcopy),       2, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(rcopy),       2, 2, 0, UNC_CFUNC_CONCURRENT },
    { FN(rdestroy),    1, 0, 0, UNC_CFUNC_CONCURRENT },
    { FN(link),        2, 1, 0, UNC_CFUNC_CONCURRENT },
};

Unc_RetVal uncilmain_fs(struct Unc_View *w) {
    Unc_RetVal e;
    char buf[2];
    buf[1] = 0;
    {
        Unc_Value v = UNC_BLANK;
        buf[0] = UNCIL_DIRSEP;
        e = unc_newstringc(w, &v, buf);
        if (!e) e = unc_setpublicc(w, "sep", &v);
        if (e) return e;
    }
    {
        Unc_Value v = UNC_BLANK;
        buf[0] = UNCIL_PATHSEP;
        e = unc_newstringc(w, &v, buf);
        if (!e) e = unc_setpublicc(w, "pathsep", &v);
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
    return unc_exportcfunctions(w, PASSARRAY(lib), 0, NULL, NULL);
}
