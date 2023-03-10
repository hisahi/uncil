/*******************************************************************************
 
Uncil -- builtin process library impl

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

#include <errno.h>
#include <string.h>

#define UNCIL_DEFINES

#include "uarithm.h"
#include "udef.h"
#include "ulibio.h"
#include "uncil.h"
#include "uosdef.h"
#include "uvsio.h"

#if UNCIL_IS_POSIX
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#endif

#include <stdio.h>

static int unc0_proc_makeerr(Unc_View *w, const char *prefix, int err) {
    return unc0_std_makeerr(w, "system", prefix, err);
}

enum pipe_type {
    PIPE_INHERIT,
    PIPE_CLOSED,
    PIPE_PIPE,
    PIPE_PIPENB,
    PIPE_NULL,
    PIPE_FILE,
    PIPE_ALIAS
};

typedef int Unc_Pipe;

union pipe_data {
    Unc_Pipe fd;
    void *ptr;
};

struct unc0_proc_job {
    pid_t pid;
    int finished;
    int exitcode;
};

#if UNCIL_IS_POSIX
typedef int unc_fd_t;

static int unc0_posix_pipe(unc_fd_t pipefd[2]) {
    return pipe(pipefd);
}

static void unc0_posix_disown(pid_t pid) {
    /* move pid to another parent if supported by the platform to avoid
       leaving behind zombies when the child (pid) dies.
       if only POSIX had defined something for this... */
    /* no other option, otherwise we might end up with a zombie */
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
}
#endif

static void unc0_proc_pcheck(struct unc0_proc_job *job) {
    if (!job->finished) {
#if UNCIL_IS_POSIX
        int status;
        pid_t pid = waitpid(job->pid, &status, WNOHANG);
        if (pid != job->pid) return;
        job->finished = 1;
        if (WIFEXITED(status)) {
            job->exitcode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            job->exitcode = WTERMSIG(status) + 128;
        } else {
            job->exitcode = -1;
        }
#endif
    }
}

static int unc0_proc_popen(Unc_View *w, const char *cmd,
                      Unc_Size argc, Unc_Value *argv,
                      const char *cwd, Unc_Value *envt,
                      enum pipe_type stdin, union pipe_data *stdin_data,
                      enum pipe_type stdout, union pipe_data *stdout_data,
                      enum pipe_type stderr, union pipe_data *stderr_data,
                      struct unc0_proc_job *job) {
#if UNCIL_IS_POSIX
    int e;
    unc_fd_t r_stat[2];
    unc_fd_t r_in[2] = { -1, -1 },
             r_out[2] = { -1, -1 },
             r_err[2] = { -1, -1 };
    pid_t pid;
    int accountable[3] = { 0 };
    const char **arg;
    char **env = NULL;
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_Size env_n = 0, env_c = 0;

    arg = TMALLOCZ(const char *, alloc, Unc_AllocLibrary, argc + 2);
    if (!arg) return UNCIL_ERR_MEM;

    {
        Unc_Size i;
        arg[0] = cmd;
        for (i = 0; i < argc; ++i) {
            e = unc_getstringc(w, &argv[i], &arg[i + 1]);
            if (e) {
                TMFREE(const char *, alloc, arg, argc + 1);
                return e;
            }
        }
    }

    if (unc_gettype(w, envt)) {
        Unc_Value iter = UNC_BLANK;
        char *keyval;
        e = unc_getiterator(w, envt, &iter);
        for (;;) {
            Unc_Pile pile;
            Unc_Tuple tuple;
            e = unc_call(w, &iter, 0, &pile);
            if (e) break;
            unc_returnvalues(w, &pile, &tuple);
            if (!tuple.count) {
                keyval = NULL;
                unc_discard(w, &pile);
                break;
            } else {
                Unc_Size keyn, valn;
                char *keyc, *valc;
                if (tuple.count != 2) {
                    e = UNCIL_ERR_INTERNAL;
                    unc_discard(w, &pile);
                    break;
                }
                if (unc_gettype(w, &tuple.values[0]) != Unc_TString) {
                    e = unc_throwexc(w, "type",
                        "environment table keys must be strings");
                    unc_discard(w, &pile);
                    break;
                }
                e = unc_valuetostringn(w, &tuple.values[0], &keyn, &keyc);
                if (e) break;
                if (strchr(keyc, '=')) {
                    e = unc_throwexc(w, "value",
                        "environment table keys may not contain '='");
                    unc_mfree(w, keyc);
                    unc_discard(w, &pile);
                    break;
                }
                e = unc_valuetostringn(w, &tuple.values[1], &valn, &valc);
                if (e) {
                    unc_mfree(w, keyc);
                    unc_discard(w, &pile);
                    break;
                }
                unc_discard(w, &pile);
                keyval = unc_mrealloc(w, keyc, keyn + valn + 2);
                if (!keyval) {
                    unc_mfree(w, valc);
                    unc_mfree(w, keyc);
                    break;
                }
                keyval[keyn] = '=';
                strcpy(keyval + 1, valc);
                unc_mfree(w, valc);
            }
            if (env_n == env_c) {
                Unc_Size env_z = env_c + 8;
                char **env_n = TMREALLOC(char *, alloc, Unc_AllocLibrary,
                                         env, env_c, env_z);
                if (!env_n) {
                    if (keyval) unc_mfree(w, keyval);
                    e = UNCIL_ERR_MEM;
                    break;
                }
                env = env_n;
                env_c = env_z;
            }
            env[env_n++] = keyval;
            if (!keyval) {
                --env_n;
                break;
            }
        }
        if (e) goto unc0_proc_popen_fail;
    }

    e = UNCIL_ERR_IO_UNDERLYING;
    if (unc0_posix_pipe(r_stat)) goto unc0_proc_popen_fail;
    if (fcntl(r_stat[1], F_SETFD, fcntl(r_stat[1], F_GETFD) | FD_CLOEXEC))
        goto unc0_proc_popen_fail;
    
    switch (stdin) {
    case PIPE_INHERIT:
        break;
    case PIPE_CLOSED:
        if (unc0_posix_pipe(r_in)) goto unc0_proc_popen_fail;
        close(r_in[1]);
        accountable[0] = 1;
        break;
    case PIPE_PIPE:
    case PIPE_PIPENB:
        if (unc0_posix_pipe(r_in)) goto unc0_proc_popen_fail;
        stdin_data->fd = r_in[1];
        if (stdin == PIPE_PIPENB
                && fcntl(stdin_data->fd, F_SETFL,
                   fcntl(stdin_data->fd, F_GETFL) | O_NONBLOCK))
            goto unc0_proc_popen_fail;
        accountable[0] = 1;
        break;
    case PIPE_NULL:
        if ((r_in[0] = open("/dev/null", O_RDONLY)) == -1)
            goto unc0_proc_popen_fail;
        accountable[0] = 1;
        break;
    case PIPE_FILE:
        if ((r_in[0] = fileno((FILE *)stdin_data->ptr)) == -1)
            goto unc0_proc_popen_fail;
        break;
    case PIPE_ALIAS:
        ASSERT(0);
        return UNCIL_ERR_INTERNAL;
    }
    
    switch (stdout) {
    case PIPE_INHERIT:
        break;
    case PIPE_CLOSED:
        if (unc0_posix_pipe(r_out)) goto unc0_proc_popen_fail;
        close(r_out[0]);
        accountable[1] = 1;
        break;
    case PIPE_PIPE:
    case PIPE_PIPENB:
        if (unc0_posix_pipe(r_out)) goto unc0_proc_popen_fail;
        stdout_data->fd = r_out[0];
        if (stdout == PIPE_PIPENB
                && fcntl(stdout_data->fd, F_SETFL,
                   fcntl(stdout_data->fd, F_GETFL) | O_NONBLOCK))
            goto unc0_proc_popen_fail;
        accountable[1] = 1;
        break;
    case PIPE_NULL:
        if ((r_out[1] = open("/dev/null", O_WRONLY)) == -1)
            goto unc0_proc_popen_fail;
        accountable[1] = 1;
        break;
    case PIPE_FILE:
        if ((r_out[1] = fileno((FILE *)stdout_data->ptr)) == -1)
            goto unc0_proc_popen_fail;
        break;
    case PIPE_ALIAS:
        ASSERT(0);
        return UNCIL_ERR_INTERNAL;
    }
    
    switch (stderr) {
    case PIPE_INHERIT:
        break;
    case PIPE_CLOSED:
        if (unc0_posix_pipe(r_err)) goto unc0_proc_popen_fail;
        close(r_err[0]);
        accountable[2] = 1;
        break;
    case PIPE_PIPE:
    case PIPE_PIPENB:
        if (unc0_posix_pipe(r_err)) goto unc0_proc_popen_fail;
        stderr_data->fd = r_err[0];
        if (stderr == PIPE_PIPENB
                && fcntl(stderr_data->fd, F_SETFL,
                   fcntl(stderr_data->fd, F_GETFL) | O_NONBLOCK))
            goto unc0_proc_popen_fail;
        accountable[2] = 1;
        break;
    case PIPE_NULL:
        if ((r_err[1] = open("/dev/null", O_WRONLY)) == -1)
            goto unc0_proc_popen_fail;
        accountable[2] = 1;
        break;
    case PIPE_FILE:
        if ((r_err[1] = fileno((FILE *)stderr_data->ptr)) == -1)
            goto unc0_proc_popen_fail;
        break;
    case PIPE_ALIAS:
        r_err[1] = 1;
    }

    switch ((pid = fork())) {
    case 0:
        close(r_stat[0]);
        if (r_in[0] != -1)
            while (dup2(r_in[0], STDIN_FILENO) == -1)
                if (errno != EINTR) goto unc0_proc_popen_fork_fail;
        if (r_out[1] != -1)
            while (dup2(r_out[1], STDOUT_FILENO) == -1)
                if (errno != EINTR) goto unc0_proc_popen_fork_fail;
        if (r_err[1] != -1)
            while (dup2(r_err[1], STDERR_FILENO) == -1)
                if (errno != EINTR) goto unc0_proc_popen_fork_fail;
        if (cwd && chdir(cwd)) goto unc0_proc_popen_fork_fail;
        if (env)
            execve(cmd, (char *const*)arg, env);
        else
            execv(cmd, (char *const*)arg);
unc0_proc_popen_fork_fail:
        write(r_stat[1], &errno, sizeof(int));
        _exit(0);
    default:
    {
        ssize_t count;
        int err;
        close(r_stat[1]);
        while ((count = read(r_stat[0], &err, sizeof(errno))) == -1)
            if (errno != EAGAIN && errno != EINTR) break;
        close(r_stat[0]);
        if (!count) {
            if (env) {
                Unc_Size i;
                for (i = 0; i < env_n; ++i)
                    unc0_mmfree(alloc, env[i]);
                TMFREE(char *, alloc, env, env_n + 1);
            }
            TMFREE(const char *, alloc, arg, argc + 1);
            job->pid = pid;
            job->finished = 0;
            unc0_proc_pcheck(job);
            return 0;
        }
        errno = err;
    }
    case -1:
        /* goto unc0_proc_popen_fail */;
    }

unc0_proc_popen_fail:
    {
        int old_errno = errno;
        if (env) {
            Unc_Size i;
            for (i = 0; i < env_n; ++i)
                unc0_mmfree(alloc, env[i]);
            TMFREE(char *, alloc, env, env_n + 1);
        }
        TMFREE(const char *, alloc, arg, argc + 1);
        if (accountable[0]) close(r_in[0]), close(r_in[1]);
        if (accountable[1]) close(r_out[0]), close(r_out[1]);
        if (accountable[2]) close(r_err[0]), close(r_err[1]);
        errno = old_errno;
    }
    return UNCIL_ERR_IO_UNDERLYING;
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static int unc0_proc_pmunge(Unc_View *w, Unc_Value *out3,
                      Unc_Size sin_n, const byte *sin,
                      Unc_Pipe stdin,
                      Unc_Pipe stdout,
                      Unc_Pipe stderr,
                      struct unc0_proc_job *job) {
#if UNCIL_IS_POSIX
    int e;
    byte *sout = NULL, *serr = NULL;
    byte buf[BUFSIZ];
    Unc_Allocator *alloc = &w->world->alloc;
    Unc_Size sin_i = 0;
    Unc_Size sout_n = 0, sout_c = 0;
    Unc_Size serr_n = 0, serr_c = 0;
    Unc_Pipe nfds = stdin;
    if (nfds < stdout) nfds = stdout;
    if (nfds < stderr) nfds = stderr;
    ++nfds;
    if (!sin_n) {
        close(stdin);
        stdin = -1;
    }
    while (!job->finished) {
        fd_set rfds, wfds;
        struct timeval tv;
        int rv;

        FD_ZERO(&rfds);
        FD_SET(stdout, &rfds);
        FD_SET(stderr, &rfds);
        FD_ZERO(&wfds);
        if (stdin != -1) FD_SET(stdin, &wfds);

        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        rv = select(nfds, &rfds, &wfds, NULL, &tv);
        if (rv) {
            if (rv < 0) {
                e = UNCIL_ERR_IO_UNDERLYING;
                goto unc0_proc_pmunge_fail;
            }
            if (FD_ISSET(stdin, &wfds)) {
                /* pipe in more data */
                while (sin_i < sin_n) {
                    size_t n = sin_n - sin_i;
                    ssize_t z;
                    if (n > BUFSIZ) n = BUFSIZ;
                    z = write(stdin, sin + sin_i, n);
                    if (z < 0) {
                        e = UNCIL_ERR_IO_UNDERLYING;
                        goto unc0_proc_pmunge_fail;
                    }
                    sin_i += z;
                    if (z < n)
                        break;
                }
                if (sin_i == sin_n) {
                    close(stdin);
                    stdin = -1;
                }
            }
            if (FD_ISSET(stdout, &rfds)) {
                /* read more data */
                for (;;) {
                    ssize_t z = read(stdout, buf, sizeof(buf));
                    if (!z) break;
                    e = unc0_strpush(alloc,
                                     &sout, &sout_n, &sout_c,
                                     10, z, buf);
                    if (e) goto unc0_proc_pmunge_fail;
                    if (z < sizeof(buf)) break;
                }
            }
            if (FD_ISSET(stderr, &rfds)) {
                /* read more data */
                for (;;) {
                    ssize_t z = read(stderr, buf, sizeof(buf));
                    if (!z) break;
                    e = unc0_strpush(alloc,
                                     &serr, &serr_n, &serr_c,
                                     10, z, buf);
                    if (e) goto unc0_proc_pmunge_fail;
                    if (z < sizeof(buf)) break;
                }
            }
        }
        unc0_proc_pcheck(job);
    }
    sout = unc0_mmrealloc(alloc, Unc_AllocLibrary, sout, sout_n);
    serr = unc0_mmrealloc(alloc, Unc_AllocLibrary, serr, serr_n);
    e = unc_newblobmove(w, &out3[1], sout);
    if (e) {
        unc0_mmfree(alloc, sout);
        unc0_mmfree(alloc, serr);
        return e;
    }
    e = unc_newblobmove(w, &out3[2], serr);
    if (e) {
        unc0_mmfree(alloc, serr);
        return e;
    }
    unc_setint(w, &out3[0], job->exitcode);
    return 0;
unc0_proc_pmunge_fail:
{
    int old_errno = errno;
    unc0_mmfree(alloc, sout);
    unc0_mmfree(alloc, serr);
    if (stdin != -1) close(stdin);
    close(stdout);
    close(stderr);
    unc0_posix_disown(job->pid);
    errno = old_errno;
    return e;
}
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static FILE *unc0_proc_fd_open(int fd, int stdin) {
#if UNCIL_IS_POSIX
    return fdopen(fd, stdin ? "rb" : "wb");
#else
    return NULL;
#endif
}

static void unc0_proc_fd_close(int fd) {
#if UNCIL_IS_POSIX
    close(fd);
#endif
}

static int unc0_proc_pipe_parse(Unc_View *w, Unc_Value *v,
                        enum pipe_type *t, union pipe_data *p,
                        int stderr) {
    switch (unc_gettype(w, v)) {
    case Unc_TNull:
        *t = PIPE_INHERIT;
        return 0;
    case Unc_TBool:
        if (unc_getbool(w, v, 0))
            *t = PIPE_PIPE;
        else
            *t = PIPE_CLOSED;
        return 0;
    case Unc_TString:
    {
        const char *cs;
        int e = unc_getstringc(w, v, &cs);
        if (e) return e;
        if (!strcmp(cs, "null")) {
            *t = PIPE_NULL;
            return 0;
        } else if (!strcmp(cs, "stdout")) {
            if (stderr)
                *t = PIPE_ALIAS;
            else
                return unc_throwexc(w, "value",
                    "stream mode \"stdout\" only allowed for stderr");
            return 0;
        } else {
            return unc_throwexc(w, "value",
                "invalid stream mode \"stdout\"");
        }
    }
    case Unc_TOpaque:
    {
        int e;
        Unc_Value proto = UNC_BLANK;
        unc_getprototype(w, v, &proto);
        e = !unc_issame(w, &proto, &w->world->io_file);
        unc_clear(w, &proto);
        if (e) return unc_throwexc(w, "type", "invalid stream mode value");
        *t = PIPE_FILE;
        p->ptr = v;
        return 0;
    }
    default:
        return unc_throwexc(w, "type", "invalid stream mode value");
    }
}

static int unc0_proc_pipe_copy(Unc_View *w, Unc_Value *v, int k,
                        enum pipe_type t, union pipe_data *p) {
    switch (t) {
    case PIPE_PIPE:
    case PIPE_PIPENB:
    {
        int e;
        Unc_Value fv = UNC_BLANK;
        FILE *ff = unc0_proc_fd_open(p->fd, k == 0);
        if (!ff)
            e = UNCIL_ERR_IO_UNDERLYING;
        else {
            e = unc0_io_fwrap(w, &fv, ff, k != 0);
            if (e) {
                fclose(ff);
                p->fd = -1;
            }
        }
        if (!e) unc_copy(w, unc_opaqueboundvalue(w, v, k), &fv);
        unc_clear(w, &fv);
        return e;
    }
    case PIPE_FILE:
        unc_copy(w, unc_opaqueboundvalue(w, v, k), (Unc_Value *)p->ptr);
        return 0;
    default:
        break;
    }
    return 0;
}

static void unc0_proc_pipe_close(Unc_View *w, enum pipe_type t,
                                 union pipe_data p) {
    switch (t) {
    case PIPE_PIPE:
    case PIPE_PIPENB:
        if (p.fd != -1) unc0_proc_fd_close(p.fd);
        break;
    case PIPE_FILE:
    {
        Unc_Value *v = p.ptr;
        struct ulib_io_file *fp;
        if (unc0_io_lockfile(w, v, &fp, 0))
            return;
        unc0_io_fclose_p(w, &w->world->alloc, (struct ulib_io_file *)fp);
        unc0_io_unlockfile(w, v);
        break;
    }
    default:
        break;
    }
}

static int unc0_proc_wait(Unc_View *w, struct unc0_proc_job *job) {
    unc0_proc_pcheck(job);
    if (!job->finished) {
#if UNCIL_IS_POSIX
        int status;
        pid_t pid;
        unc_vmpause(w);
        while ((pid = waitpid(job->pid, &status, 0)) != job->pid) {
            if (pid == -1) {
                unc_vmresume(w);
                return UNCIL_ERR_IO_UNDERLYING;
            }
        }
        unc_vmresume(w);
        job->finished = 1;
        if (WIFEXITED(status)) {
            job->exitcode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            job->exitcode = WTERMSIG(status) + 128;
        } else {
            job->exitcode = -1;
        }
#else
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
    return 0;
}

static int unc0_proc_waittimed(Unc_View *w, struct unc0_proc_job *job,
                               Unc_Float seconds) {
    unc0_proc_pcheck(job);
    if (!job->finished) {
#if UNCIL_IS_POSIX && _POSIX_VERSION >= 200809L || \
    (defined(_POSIX_REALTIME_SIGNALS) && defined(SIGRTMIN))
        sigset_t mask;
        struct timespec t, ts;
        int status;
        pid_t pid;
        clockid_t clk;
#ifdef CLOCK_MONOTONIC_RAW
        clk = CLOCK_MONOTONIC_RAW;
        if (!clock_gettime(clk, &ts)) goto unc0_proc_waittimed_clk;
#endif
#ifdef CLOCK_MONOTONIC
        clk = CLOCK_MONOTONIC;
        if (!clock_gettime(clk, &ts)) goto unc0_proc_waittimed_clk;
#endif
        clk = CLOCK_REALTIME;
unc0_proc_waittimed_clk:
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        t.tv_sec = (time_t)seconds;
        t.tv_nsec = (long)(unc0_ffrac(seconds) * 1000000000);
        unc_vmpause(w);
        do {
            if (sigtimedwait(&mask, NULL, &t) < 0) {
                if (errno == EINTR)
                    continue;
                else if (errno == EAGAIN) {
                    unc_vmresume(w);
                    return 0;
                } else {
                    unc_vmresume(w);
                    return UNCIL_ERR_IO_UNDERLYING;
                }
            } else {
                pid = waitpid(job->pid, &status, WNOHANG);
                if (pid == job->pid) {
                    unc_vmresume(w);
                    job->finished = 1;
                    if (WIFEXITED(status)) {
                        job->exitcode = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        job->exitcode = WTERMSIG(status) + 128;
                    } else {
                        job->exitcode = -1;
                    }
                    return 0;
                } else if (pid != -1) {
                    struct timespec bts;
                    clock_gettime(clk, &bts);
                    t.tv_sec = bts.tv_sec - ts.tv_sec;
                    t.tv_nsec = bts.tv_nsec - ts.tv_nsec;
                    if (t.tv_nsec < 0) {
                        --t.tv_sec;
                        t.tv_nsec += 1000000000L;
                    }
                    if (t.tv_sec < 0) {
                        unc_vmresume(w);
                        return 0;
                    }
                    continue;
                }
            }
        } while (0);
#elif UNCIL_IS_POSIX
        struct timespec ts;
        struct timeval t;
        int status, e;
        clockid_t clk;
#ifdef CLOCK_MONOTONIC_RAW
        clk = CLOCK_MONOTONIC_RAW;
        if (!clock_gettime(clk, &ts)) goto unc0_proc_waittimed_retry;
#endif
#ifdef CLOCK_MONOTONIC
        clk = CLOCK_MONOTONIC;
        if (!clock_gettime(clk, &ts)) goto unc0_proc_waittimed_retry;
#endif
        clk = CLOCK_REALTIME;
unc0_proc_waittimed_retry:
        t.tv_sec = (time_t)seconds;
        t.tv_usec = (suseconds_t)(unc0_ffrac(seconds) * 1000000);
unc0_proc_waittimed_retry2:
        clock_gettime(clk, &ts);
        unc_vmpause(w);
        e = select(0, NULL, NULL, NULL, &t);
        unc_vmresume(w);
        if (e == 0) {
            return 0;
        } else if (errno == EINTR) {
            pid_t pid = waitpid(job->pid, &status, WNOHANG);
            if (pid == job->pid) {
                job->finished = 1;
                if (WIFEXITED(status)) {
                    job->exitcode = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    job->exitcode = WTERMSIG(status) + 128;
                } else {
                    job->exitcode = -1;
                }
            } else if (pid != -1) {
                struct timespec bts, cts;
                clock_gettime(clk, &bts);
                cts.tv_sec = bts.tv_sec - ts.tv_sec;
                cts.tv_nsec = bts.tv_nsec - ts.tv_nsec;
                if (cts.tv_nsec < 0) {
                    --cts.tv_sec;
                    cts.tv_nsec += 1000000000L;
                }
                if (cts.tv_sec < 0)
                    return 0;
                t.tv_sec = cts.tv_sec;
                t.tv_usec = cts.tv_nsec / 1000;
                goto unc0_proc_waittimed_retry2;
            } else
                return UNCIL_ERR_IO_UNDERLYING;
        } else {
            return UNCIL_ERR_IO_UNDERLYING;
        }
#else
        return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
    }
    return 0;
}

static void unc0_proc_halt(struct unc0_proc_job *job) {
    unc0_proc_pcheck(job);
    if (!job->finished) {
#if UNCIL_IS_POSIX
        kill(job->pid, SIGTERM);
#endif
    }
}

static int unc0_proc_signal(Unc_View *w, struct unc0_proc_job *job, int sig) {
    unc0_proc_pcheck(job);
    if (job->finished) return 0;
#if UNCIL_IS_POSIX
    return kill(job->pid, sig);
#else
    return UNCIL_ERR_LOGIC_NOTSUPPORTED;
#endif
}

static Unc_RetVal unc0_proc_job_destr(Unc_View *w, size_t n, void *data) {
    struct unc0_proc_job *job = data;
    if (!job->finished) {
#if UNCIL_IS_POSIX
        unc0_posix_disown(job->pid);
#endif
    }
    return 0;
}

Unc_RetVal unc0_lib_process_open(Unc_View *w, Unc_Tuple args, void *udata) {
    int e, locked = 0;
    Unc_Value v = UNC_BLANK;
    const char *cmd, *cwd = NULL;
    Unc_Size an = 0;
    Unc_Value *av = NULL;
    enum pipe_type stdin, stdout, stderr;
    union pipe_data stdin_data, stdout_data, stderr_data;
    struct unc0_proc_job *job;
    /* PIPE_PIPE: fd = file descriptor
       PIPE_FILE: ptr = * to Unc_Value of opaque */

    e = unc_getstringc(w, &args.values[0], &cmd);
    if (e) return e;

    if (unc_gettype(w, &args.values[2])) {
        e = unc_getstringc(w, &args.values[2], &cwd);
        if (e) return e;
    }

    {
        Unc_ValueType vt = unc_gettype(w, &args.values[3]);
        if (vt && vt != Unc_TTable)
            return unc_throwexc(w, "type", "env must be a table if given");
    }

    if (!e)
        e = unc0_proc_pipe_parse(w, &args.values[4], &stdin, &stdin_data, 0);
    if (!e)
        e = unc0_proc_pipe_parse(w, &args.values[5], &stdout, &stdout_data, 0);
    if (!e)
        e = unc0_proc_pipe_parse(w, &args.values[6], &stderr, &stderr_data, 1);

    if (!e && unc_gettype(w, &args.values[1])) {
        e = unc_lockarray(w, &args.values[1], &an, &av);
        if (!e) locked = 1;
    }

    if (!e) {
        Unc_Value nuls[3] = UNC_BLANKS;
        e = unc_newopaque(w, &v, unc_boundvalue(w, 0),
                            sizeof(struct unc0_proc_job), (void **)&job,
                            &unc0_proc_job_destr,
                            3, nuls, 0, NULL);
    }

    if (!e)
        e = unc0_proc_popen(w, cmd, an, av,
                            cwd, &args.values[3],
                            stdin, &stdin_data,
                            stdout, &stdout_data,
                            stderr, &stderr_data,
                            job);

    if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
        e = unc0_proc_makeerr(w, "process.open()", errno);

    if (e) {
        unc0_proc_pipe_close(w, stdin, stdin_data);
        unc0_proc_pipe_close(w, stdout, stdout_data);
        unc0_proc_pipe_close(w, stderr, stderr_data);
    } else {
        e = unc0_proc_pipe_copy(w, &v, 0, stdin, &stdin_data);
        if (!e)
            e = unc0_proc_pipe_copy(w, &v, 1, stdout, &stdout_data);
        if (!e)
            e = unc0_proc_pipe_copy(w, &v, 2, stderr, &stderr_data);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            e = unc0_proc_makeerr(w, "process.open()", errno);
    }
    if (locked) unc_unlock(w, &args.values[1]);
    if (!e) e = unc_pushmove(w, &v, NULL);
    else unc_clear(w, &v);
    return e;
}

Unc_RetVal unc0_lib_process_munge(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value res[3] = UNC_BLANKS;
    const char *cmd, *cwd = NULL;
    Unc_Size an, bn;
    Unc_Value *av;
    byte *bv;
    enum pipe_type stdin, stdout, stderr;
    union pipe_data stdin_data, stdout_data, stderr_data;
    struct unc0_proc_job job;
    /* PIPE_PIPE: fd = file descriptor
       PIPE_FILE: ptr = * to Unc_Value of opaque */

    e = unc_getstringc(w, &args.values[0], &cmd);
    if (e) return e;

    if (unc_gettype(w, &args.values[3])) {
        e = unc_getstringc(w, &args.values[3], &cwd);
        if (e) return e;
    }

    {
        Unc_ValueType vt = unc_gettype(w, &args.values[4]);
        if (vt && vt != Unc_TTable)
            return unc_throwexc(w, "type", "env must be a table if given");
    }

    e = unc_lockarray(w, &args.values[1], &an, &av);
    if (e) return e;

    e = unc_lockblob(w, &args.values[2], &bn, &bv);
    if (e) {
        unc_unlock(w, &args.values[1]);
        return e;
    }

    stdin = stdout = stderr = PIPE_PIPENB;

    e = unc0_proc_popen(w, cmd, an, av,
                        cwd, &args.values[4],
                        stdin, &stdin_data,
                        stdout, &stdout_data,
                        stderr, &stderr_data,
                        &job);

    if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
        e = unc0_proc_makeerr(w, "process.munge -> .open()", errno);

    if (e) {
        unc0_proc_pipe_close(w, stdin, stdin_data);
        unc0_proc_pipe_close(w, stdout, stdout_data);
        unc0_proc_pipe_close(w, stderr, stderr_data);
    } else {
        e = unc0_proc_pmunge(w, res, bn, bv,
                             stdin_data.fd, 
                             stdout_data.fd,
                             stderr_data.fd,
                             &job);
        if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
            e = unc0_proc_makeerr(w, "process.munge()", errno);
    }
    unc_unlock(w, &args.values[2]);
    unc_unlock(w, &args.values[1]);
    if (!e) e = unc_push(w, 3, res, NULL);
    unc_clearmany(w, 3, res);
    return e;
}

Unc_RetVal unc0_lib_proc_job_exitcode(Unc_View *w,
                                      Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    unc0_proc_pcheck(job);
    if (job->finished)
        unc_setint(w, &tmp, job->exitcode);
    unc_unlock(w, &args.values[0]);
    if (e) return unc0_proc_makeerr(w, "process.job.exitcode()", errno);
    return unc_push(w, 1, &tmp, NULL);
}

Unc_RetVal unc0_lib_proc_job_halt(Unc_View *w,
                                  Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    unc0_proc_halt(job);
    unc_unlock(w, &args.values[0]);
    return 0;
}

Unc_RetVal unc0_lib_proc_job_signal(Unc_View *w,
                                    Unc_Tuple args, void *udata) {
    int e;
    Unc_Int sig;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_getint(w, &args.values[1], &sig);
    if (!e && (sig > INT_MAX || sig < INT_MIN))
        e = unc_throwexc(w, "value", "invalid signal value");
    if (e) return e;
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;;
    e = unc0_proc_signal(w, job, (int)sig);
    if (e && UNCIL_ERR_KIND(e) == UNCIL_ERR_KIND_IO)
        e = unc0_proc_makeerr(w, "process.job.signal()", errno);
    return e;
}

Unc_RetVal unc0_lib_proc_job_running(Unc_View *w,
                                     Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    unc0_proc_pcheck(job);
    unc_setbool(w, &tmp, !job->finished);
    unc_unlock(w, &args.values[0]);
    if (e) return unc0_proc_makeerr(w, "process.job.exitcode()", errno);
    return unc_push(w, 1, &tmp, NULL);
}

Unc_RetVal unc0_lib_proc_job_stdin(Unc_View *w,
                                     Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    unc0_proc_pcheck(job);
    unc_copy(w, &tmp, unc_opaqueboundvalue(w, &args.values[0], 0));
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &tmp, NULL);
}

Unc_RetVal unc0_lib_proc_job_stdout(Unc_View *w,
                                     Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    unc0_proc_pcheck(job);
    unc_copy(w, &tmp, unc_opaqueboundvalue(w, &args.values[0], 1));
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &tmp, NULL);
}

Unc_RetVal unc0_lib_proc_job_stderr(Unc_View *w,
                                     Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    unc0_proc_pcheck(job);
    unc_copy(w, &tmp, unc_opaqueboundvalue(w, &args.values[0], 2));
    unc_unlock(w, &args.values[0]);
    return unc_pushmove(w, &tmp, NULL);
}

Unc_RetVal unc0_lib_proc_job_wait(Unc_View *w,
                                  Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    e = unc0_proc_wait(w, job);
    if (!e) {
        ASSERT(job->finished);
        unc_setint(w, &tmp, job->exitcode);
        e = unc_pushmove(w, &tmp, NULL);
    }
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal unc0_lib_proc_job_waittimed(Unc_View *w,
                                       Unc_Tuple args, void *udata) {
    int e;
    Unc_Value tmp = UNC_BLANK;
    struct unc0_proc_job *job;
    Unc_Float seconds;
    unc_getprototype(w, &args.values[0], &tmp);
    if (unc_gettype(w, &args.values[0]) != Unc_TOpaque
            || !unc_issame(w, &tmp, unc_boundvalue(w, 0))) {
        unc_clear(w, &tmp);
        return unc_throwexc(w, "type", "argument is not a process.job");
    }
    unc_clear(w, &tmp);
    e = unc_getfloat(w, &args.values[1], &seconds);
    if (e) return e;
    if (seconds < 0)
        return unc_throwexc(w, "value", "timeout cannot be negative");
    if (!unc0_fisfinite(seconds))
        return unc_throwexc(w, "value", "timeout must be finite");
    e = unc_lockopaque(w, &args.values[0], NULL, (void **)&job);
    if (e) return e;
    e = unc0_proc_waittimed(w, job, seconds);
    if (!e && job->finished) {
        unc_setint(w, &tmp, job->exitcode);
        e = unc_pushmove(w, &tmp, NULL);
    }
    unc_unlock(w, &args.values[0]);
    return e;
}

Unc_RetVal uncilmain_process(struct Unc_View *w) {
    Unc_RetVal e;
    Unc_Value proc_job = UNC_BLANK;

    e = unc0_io_init(w);
    if (e) return e;

    e = unc_newobject(w, &proc_job, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "open", &unc0_lib_process_open,
                            UNC_CFUNC_DEFAULT,
                            1, 0, 6, NULL, 1, &proc_job, 0, NULL, NULL);
    if (e) return e;

    e = unc_exportcfunction(w, "munge", &unc0_lib_process_munge,
                            UNC_CFUNC_DEFAULT,
                            3, 0, 2, NULL, 0, NULL, 0, NULL, NULL);
    if (e) return e;

    {
        Unc_Value tmp = UNC_BLANK;

#if UNCIL_IS_POSIX
        unc_setint(w, &tmp, SIGHUP);
        if ((e = unc_setpublicc(w, "SIGHUP", &tmp))) return e;
        unc_setint(w, &tmp, SIGINT);
        if ((e = unc_setpublicc(w, "SIGINT", &tmp))) return e;
        unc_setint(w, &tmp, SIGQUIT);
        if ((e = unc_setpublicc(w, "SIGQUIT", &tmp))) return e;
        unc_setint(w, &tmp, SIGILL);
        if ((e = unc_setpublicc(w, "SIGILL", &tmp))) return e;
        unc_setint(w, &tmp, SIGABRT);
        if ((e = unc_setpublicc(w, "SIGABRT", &tmp))) return e;
        unc_setint(w, &tmp, SIGFPE);
        if ((e = unc_setpublicc(w, "SIGFPE", &tmp))) return e;
        unc_setint(w, &tmp, SIGKILL);
        if ((e = unc_setpublicc(w, "SIGKILL", &tmp))) return e;
        unc_setint(w, &tmp, SIGSEGV);
        if ((e = unc_setpublicc(w, "SIGSEGV", &tmp))) return e;
        unc_setint(w, &tmp, SIGPIPE);
        if ((e = unc_setpublicc(w, "SIGPIPE", &tmp))) return e;
        unc_setint(w, &tmp, SIGALRM);
        if ((e = unc_setpublicc(w, "SIGALRM", &tmp))) return e;
        unc_setint(w, &tmp, SIGTERM);
        if ((e = unc_setpublicc(w, "SIGTERM", &tmp))) return e;
        unc_setint(w, &tmp, SIGUSR1);
        if ((e = unc_setpublicc(w, "SIGUSR1", &tmp))) return e;
        unc_setint(w, &tmp, SIGUSR2);
        if ((e = unc_setpublicc(w, "SIGUSR2", &tmp))) return e;
        unc_setint(w, &tmp, SIGCHLD);
        if ((e = unc_setpublicc(w, "SIGCHLD", &tmp))) return e;
        unc_setint(w, &tmp, SIGCONT);
        if ((e = unc_setpublicc(w, "SIGCONT", &tmp))) return e;
        unc_setint(w, &tmp, SIGSTOP);
        if ((e = unc_setpublicc(w, "SIGSTOP", &tmp))) return e;
        unc_setint(w, &tmp, SIGTSTP);
        if ((e = unc_setpublicc(w, "SIGTSTP", &tmp))) return e;
#endif

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_exitcode,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "exitcode", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "exitcode", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_halt,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "halt", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "halt", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_running,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "running", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "running", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_signal,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "signal", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "signal", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_stderr,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "stderr", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "stderr", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_stdin,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "stdin", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "stdin", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_stdout,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "stdout", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "stdout", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_wait,
                             UNC_CFUNC_CONCURRENT,
                             1, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "wait", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "wait", &tmp);
        if (e) return e;

        e = unc_newcfunction(w, &tmp, &unc0_lib_proc_job_waittimed,
                             UNC_CFUNC_CONCURRENT,
                             2, 0, 0, NULL,
                             1, &proc_job, 0, NULL, "waittimed", NULL);
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "waittimed", &tmp);
        if (e) return e;

        e = unc_newstringc(w, &tmp, "process.job");
        if (e) return e;
        e = unc_setattrc(w, &proc_job, "__name", &tmp);
        if (e) return e;
        unc_clear(w, &tmp);
    }

    e = unc_setpublicc(w, "job", &proc_job);
    if (e) return e;

    unc_clear(w, &proc_job);
    return 0;
}
