/*******************************************************************************
 
Uncil -- main Uncil program / REPL

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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNCIL_DEFINES

#include "ulex.h"
#include "uncil.h"
#include "uncver.h"
#include "uosdef.h"

#define MAXHIST 1000

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#define UNCIL_EXIT_OK 0
#define UNCIL_EXIT_FAIL 1
#define UNCIL_EXIT_USE 2
#else
#define UNCIL_EXIT_OK EXIT_SUCCESS
#define UNCIL_EXIT_FAIL EXIT_FAILURE
#define UNCIL_EXIT_USE EXIT_FAILURE
#endif

#if UNCIL_LIB_READLINE
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif

static const char *myname;
static char errbuf[512];
static Unc_View *uncil_instance;
static int interactive = 0;
static int keepgoing = 1;

static void uncilerr(Unc_View *unc, int e) {
    size_t n = sizeof(errbuf);
    Unc_Value exc = unc_blank;
    unc_getexceptionfromcode(unc, &exc, e);
    if (!unc_exceptiontostring(unc, &exc, &n, errbuf))
        printf("%s\n", errbuf);
    else
        printf("%s: error %04x, could not get exception text\n", myname, e);
    unc_decref(unc, &exc);
}

static void uncil_atexit(void) {
    if (uncil_instance) unc_destroy(uncil_instance);
}

static void uncilintro(void) {
    printf("Uncil %s\n", UNCIL_VER_STRING);
}

#if UNCIL_LIB_READLINE

Unc_RetVal unc__input(Unc_View *w, Unc_Tuple args, void *udata) {
    int e;
    Unc_Value v = UNC_BLANK;
    const char *prompt = NULL;
    char *line;
    if (unc_gettype(w, &args.values[0])) {
        Unc_Size prompt_n;
        e = unc_getstring(w, &args.values[0], &prompt_n, &prompt);
        if (e) return e;
    }
    line = readline(prompt);
    if (line) {
        if (*line) add_history(line);
        e = unc_newstringc(w, &v, line);
        free(line);
    } else
        e = 0;
    if (!e) e = unc_pushmove(w, &v, NULL);
    return e;
}

#define UNCIL_CUSTOMINPUT 1

static const char *uncilgetline(int cont) {
    static char *last_line = NULL;
    if (last_line) {
        free(last_line);
        last_line = NULL;
    }
    last_line = readline(cont ? "~ " : "> ");
    if (last_line && *last_line) add_history(last_line);
    if (!last_line) {
        putchar('\n');
        keepgoing = 0;
        return "";
    }
    return last_line;
}

#else

static char buf[256];
static const char *uncilgetline(int cont) {
    const char *p;
    printf(cont ? "~ " : "> ");
    fflush(stdout);
    p = fgets(buf, sizeof(buf), stdin);
    if (!p) {
        putchar('\n');
        keepgoing = 0;
        strcpy(buf, "");
        return buf;
    }
    return p;
}

#endif

int sgetch_(void *p) {
    int c = *(*((const char **)p))++;
    return c ? c : -1;
}

static void unc__copyprogname(Unc_View *unc, const char *fn) {
    size_t sl = strlen(fn);
    Unc_Program *prog = unc->program;
    ASSERT(prog);
    if ((prog->pname = unc_malloc(unc, sl + 1)))
        unc__memcpy(prog->pname, fn, sl + 1);
}

static byte *textbuf = NULL;
static Unc_Size textbuf_n = 0;
static Unc_Size textbuf_c = 0;
static int textbuf_end = 0;

static long depth = 0;
static long fdepth = 0;
static long pdepth = 0;
static long next_else = 0;
static long next_do = 0;
static long fdepths_static[64];

static int uncilprocessline(const char *line, const char **outp) {
    int cont;
    Unc_Context cxt = { NULL };
    Unc_LexOut lexout;
    Unc_Allocator *alloc = &uncil_instance->world->alloc;
    const char *l = line;
    long *fdepths = fdepths_static;
    Unc_Size fdepths_n = ASIZEOF(fdepths_static);
    if (textbuf_end) {
        unc__mfree(alloc, textbuf, textbuf_c);
        textbuf = NULL;
        textbuf_n = textbuf_c = 0;
        textbuf_end = 0;
    }
    if (unc__newcontext(&cxt, alloc))
        abort();
    if (!unc__lexcode(&cxt, &lexout, &sgetch_, &l)) {
        Unc_Size i;
        byte b;
        for (i = 0; i < lexout.lc_sz; ++i) {
            b = lexout.lc[i];
            if (b != ULT_Kif)
                next_else = 0;
            switch (b) {
            case ULT_LInt:
                i += sizeof(Unc_Int);
                break;
            case ULT_LFloat:
                i += sizeof(Unc_Float);
                break;
            case ULT_LStr:
            case ULT_I:
                i += sizeof(Unc_Size);
                break;
            case ULT_Kelse:
                next_else = 1;
                break;
            case ULT_Kif:
                if (!next_else) ++depth;
                break;
            case ULT_Kfunction:
                if (fdepth == LONG_MAX) {
                    if (fdepths != fdepths_static)
                        TMFREE(long, alloc, fdepths, fdepths_n);
                    fdepths = NULL;
                } else if (fdepth >= fdepths_n) {
                    long *p;
                    Unc_Size fdepths_z = fdepths_n;
                    while (fdepths_z < fdepth)
                        fdepths_z *= 2;
                    p = fdepths == fdepths_static
                            ? TMALLOC(long, alloc, Unc_AllocOther, fdepths_z)
                            : TMREALLOC(long, alloc, Unc_AllocOther,
                                    fdepths, fdepths_n, fdepths_z);
                    if (!p && fdepths != fdepths_static)
                        TMFREE(long, alloc, fdepths, fdepths_n);
                    if ((fdepths = p))
                        fdepths_n = fdepths_z;
                }
                if (!fdepths) {
                    puts("Out of memory; current input will be discarded");
                    unc__mfree(alloc, textbuf, textbuf_c);
                    textbuf = NULL;
                    return -1;
                }
                fdepths[fdepth++] = pdepth;
                break;
            case ULT_Kfor:
            case ULT_Kwhile:
            case ULT_Kwith:
                ++next_do;
                ++depth;
                break;
            case ULT_Kdo:
                if (next_do)
                    --next_do;
                else
                    ++depth;
                break;
            case ULT_Kend:
                if (depth) --depth;
                break;
            case ULT_SParenL:
            case ULT_SBracketL:
            case ULT_SBraceL:
                ++pdepth;
                break;
            case ULT_SParenR:
            case ULT_SBracketR:
            case ULT_SBraceR:
                --pdepth;
                if (fdepth && fdepths[fdepth - 1] == pdepth
                                && b == ULT_SParenR) {
                    ++i;
                    --fdepth;
                    if (i >= lexout.lc_sz) break;
                    if (lexout.lc[i] != ULT_OSet && lexout.lc[i] != ULT_Kend)
                        ++depth;
                }
                break;
            }
        }
        unc__mfree(alloc, lexout.lc, lexout.lc_sz);
        unc__mfree(alloc, lexout.st, lexout.st_sz);
        unc__mfree(alloc, lexout.id, lexout.id_sz);
        if (fdepths != fdepths_static)
            TMFREE(long, alloc, fdepths, fdepths_n);
    }
    cont = !(depth < 0 || pdepth < 0) && (depth || pdepth);
    unc__dropcontext(&cxt);
    if (cont || textbuf) {
        if (unc__strputn(alloc, &textbuf, &textbuf_n, &textbuf_c,
                         5, strlen(line), (byte *)line)) {
            puts("Out of memory; current input will be discarded");
            unc__mfree(alloc, textbuf, textbuf_c);
            textbuf = NULL;
            return -1;
        }
#if UNCIL_LIB_READLINE
        if (unc__strput(alloc, &textbuf, &textbuf_n, &textbuf_c, 1, '\n')) {
            puts("Out of memory; current input will be discarded");
            unc__mfree(alloc, textbuf, textbuf_c);
            textbuf = NULL;
            return -1;
        }
#endif
        if (!cont) {
            if (unc__strput(alloc, &textbuf, &textbuf_n, &textbuf_c, 1, 0)) {
                puts("Out of memory; current input will be discarded");
                unc__mfree(alloc, textbuf, textbuf_c);
                textbuf = NULL;
                return -1;
            }
            *outp = (const char *)textbuf;
            textbuf_end = 1;
        }
    } else {
        *outp = line;
    }
    return cont;
}

static int uncildorepl(Unc_View *unc, Unc_Value unc_print) {
    int e, cont = 0;
    const char *text = NULL;
    Unc_Pile pile;
    uncilintro();
    while (keepgoing) {
        Unc_Tuple tuple;
        cont = uncilprocessline(uncilgetline(cont), &text);
        if (cont) continue;
        e = unc_compilestringc(unc, text);
        text = NULL;
        if (e) {
            uncilerr(unc, e);
            continue;
        }
        e = unc_call(unc, NULL, 0, &pile);
        if (e) {
            uncilerr(unc, e);
            continue;
        }
        unc_returnvalues(unc, &pile, &tuple);
        if (tuple.count)
            e = unc_callex(unc, &unc_print, tuple.count, &pile);
        /* print results if any */
        unc_discard(unc, &pile);
    }
    return UNCIL_EXIT_OK;
}

Unc_RetVal unc__exit_do(Unc_View *w, Unc_Value *retval) {
    int exitcode;
    Unc_Int ui;
    switch (unc_gettype(w, retval)) {
    case Unc_TBool:
        if (!unc_getbool(w, retval, 0))
            exitcode = EXIT_FAILURE;
        else
    case Unc_TNull:
            exitcode = EXIT_SUCCESS;
        break;
    case Unc_TInt:
        if (unc_getint(w, retval, &ui))
            exitcode = EXIT_FAILURE;
        else {
            if (ui > INT_MAX) ui = INT_MAX;
            if (ui < INT_MIN) ui = INT_MIN;
            exitcode = (int)ui;
        }
        break;
    default:
        {
            Unc_Size sn;
            char *ss;
            if (!unc_valuetostringn(w, retval, &sn, &ss)) {
                fprintf(stderr, "%s\n", ss);
                unc_mfree(w, ss);
            }
        }
        exitcode = EXIT_FAILURE;
    }
    exit(exitcode);
    return 0;
}

Unc_RetVal unc__exit(Unc_View *w, Unc_Tuple args, void *udata) {
    return unc__exit_do(w, &args.values[0]);
}

Unc_RetVal unc__quit(Unc_View *w, Unc_Tuple args, void *udata) {
    exit(0);
    return 0;
}

Unc_RetVal unc__quitrepl(Unc_View *w, Unc_Tuple args, void *udata) {
    keepgoing = 0;
    return 0;
}

static int unc__argvtostacklist(Unc_View *w, char *argv[], int start) {
    Unc_Value *v, s = UNC_BLANK;
    int e, i, c = 0;
    while (argv[start + c])
        ++c;
    v = calloc(c, sizeof(Unc_Value));
    if (!v)
        return UNCIL_ERR_MEM;
    for (i = 0; i < c; ++i) {
        e = unc_newstringc(w, &s, argv[start + i]);
        if (e) return e;
        unc_copy(w, &v[i], &s);
    }
    e = unc_newarrayfrom(w, &s, c, v);
    if (e) return e;
    unc_clearmany(w, c, &v[i]);
    free(v);
    return unc_pushmove(w, &s, NULL);
}

static int unc__setupfuncs(Unc_View *unc, int repl) {
    int e = 0;
    if (!e) e = unc_exportcfunction(unc, "exit",
                    &unc__exit, UNC_CFUNC_DEFAULT,
                    0, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (!e) e = unc_exportcfunction(unc, "quit",
                    repl ? &unc__quitrepl : &unc__quit, UNC_CFUNC_DEFAULT,
                    0, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
#if UNCIL_CUSTOMINPUT
    if (!e) e = unc_exportcfunction(unc, "input",
                    &unc__input, UNC_CFUNC_DEFAULT,
                    0, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
#endif
    return e;
}

static int unc__initpaths(Unc_View *w, const char *path) {
    Unc_World *world = w->world;
    Unc_Value *v, s = UNC_BLANK;
    int c = 1, e;
    Unc_Size i;
    const char *paths = getenv("UNCILPATH"), *p, *np;

    if (paths) {
        p = paths;
        ++c;
        while ((p = strchr(p, UNCIL_PATHSEP)))
            ++c, ++p;
    }

    v = calloc(c, sizeof(Unc_Value));
    if (!v)
        return UNCIL_ERR_MEM;
    p = paths;
    e = unc_newstringc(w, &s, path ? path : "");
    if (e) return e;
    unc_copy(w, &v[0], &s);
    for (i = 1; i < c; ++i) {
        np = strchr(p, UNCIL_PATHSEP);
        if (!np) np = strchr(p, 0);
        e = unc_newstring(w, &s, np - p, p);
        if (e) return e;
        unc_copy(w, &v[i], &s);
        p = np + 1;
    }
    e = unc_newarrayfrom(w, &s, c, v);
    if (e) return e;
    free(v);
    unc_copy(w, &world->modulepaths, &s);
    unc_clear(w, &s);
    return 0;
}

static int unc__initdlpaths(Unc_View *w, const char *path) {
    Unc_World *world = w->world;
    Unc_Value *v, s = UNC_BLANK;
    int c = 1, e;
    Unc_Size i;
    const char *paths = getenv("UNCILPATHDL"), *p, *np;

    if (paths) {
        p = paths;
        ++c;
        while ((p = strchr(p, UNCIL_PATHSEP)))
            ++c, ++p;
    }

    v = calloc(c, sizeof(Unc_Value));
    if (!v)
        return UNCIL_ERR_MEM;
    p = paths;
    e = unc_newstringc(w, &s, path ? path : "");
    if (e) return e;
    for (i = 0; i < c - 1; ++i) {
        np = strchr(p, UNCIL_PATHSEP);
        if (!np) np = strchr(p, 0);
        e = unc_newstring(w, &s, np - p, p);
        if (e) return e;
        unc_copy(w, &v[i], &s);
        p = np + 1;
    }
    unc_copy(w, &v[c - 1], &s);
    e = unc_newarrayfrom(w, &s, c, v);
    if (e) return e;
    free(v);
    unc_copy(w, &world->moduledlpaths, &s);
    unc_clear(w, &s);
    return 0;
}

static int uncilrepl(void) {
    int e;
    Unc_View *unc;
    Unc_Value unc_print = UNC_BLANK;

    unc = unc_createex(NULL, NULL, UNC_MMASK_ALL);
    if (!unc) {
        printf("%s: could not create an Uncil instance (low on memory?)\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }

    atexit(&uncil_atexit);
    uncil_instance = unc;
    unc->world->wmode = Unc_ModeREPL;
    if (unc_getpublicc(unc, "print", &unc_print)) {
        printf("%s: could not set up Uncil REPL instance: no print available\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }

    e = unc__setupfuncs(unc, 1);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }

    e = unc__initpaths(unc, NULL);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n", myname);
        return UNCIL_EXIT_FAIL;
    }

    e = unc__initdlpaths(unc, NULL);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n", myname);
        return UNCIL_EXIT_FAIL;
    }

    return uncildorepl(unc, unc_print);
}

static int uncilfilei(char* argv[], int fileat) {
    int e;
    FILE *f;
    int close;
    Unc_View *unc;
    Unc_Pile pile;
    Unc_Value main = UNC_BLANK, unc_print = UNC_BLANK;
    int binary = 0;

    if (!strcmp(argv[fileat], "-")) {
        int c;
        close = 0;
        f = stdin;
        c = getc(f);
        ungetc(c, f);
        binary = c >= 0x80 && c < 0xc0;
    } else {
        int c;
        close = 1;
        f = fopen(argv[fileat], "r");
        if (!f) {
            printf("%s: cannot open file '%s': %s\n",
                myname, argv[fileat], strerror(errno));
            return UNCIL_EXIT_FAIL;
        }
        c = getc(f);
        ungetc(c, f);
        binary = c >= 0x80 && c < 0xc0;
        if (binary) {
            fclose(f);
            f = fopen(argv[fileat], "rb");
            if (!f) {
                printf("%s: cannot open file '%s': %s\n",
                    myname, argv[fileat], strerror(errno));
                return UNCIL_EXIT_FAIL;
            }
        }
    }

    unc = unc_createex(NULL, NULL, UNC_MMASK_ALL);
    if (!unc) {
        printf("%s: could not create an Uncil instance (low on memory?)\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }
    if (unc_getpublicc(unc, "print", &unc_print)) {
        printf("%s: could not set up Uncil instance: no print available\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }
    
    e = unc__initpaths(unc, argv[fileat]);
    if (e) {
        printf("%s: could not set up Uncil instance\n", myname);
        return UNCIL_EXIT_FAIL;
    }
    e = unc__initdlpaths(unc, argv[fileat]);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n", myname);
        return UNCIL_EXIT_FAIL;
    }
    unc->world->wmode = Unc_ModeREPL;
    e = unc_exportcfunction(unc, "exit", &unc__exit, UNC_CFUNC_DEFAULT,
                    0, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }
#if UNCIL_CUSTOMINPUT
    e = unc_exportcfunction(unc, "input", &unc__input, UNC_CFUNC_DEFAULT,
                    0, 0, 1, NULL, 0, NULL, 0, NULL, NULL);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }
#endif

    atexit(&uncil_atexit); /* unc_destroy */
    uncil_instance = unc;
    e = binary ? unc_loadfile(unc, f) : unc_compilefile(unc, f);
    if (!e) unc__copyprogname(unc, close ? argv[fileat] : "<stdin>");
    if (close) fclose(f);
    if (e) {
        uncilerr(unc, e);
        return UNCIL_EXIT_FAIL;
    }

    e = unc_call(unc, NULL, 0, &pile);
    if (e) {
        uncilerr(unc, e);
        return UNCIL_EXIT_FAIL;
    }
    unc_discard(unc, &pile);

    if (!unc_getpublicc(unc, "main", &main)) {
        Unc_Tuple tuple;
        e = unc__argvtostacklist(unc, argv, fileat);
        if (e) {
            printf("%s: cannot run main: out of memory (or too many arguments"
                        " to fit in memory)\n", myname);
            return UNCIL_EXIT_FAIL;
        }
        e = unc_call(unc, &main, 1, &pile);
        if (e == UNCIL_ERR_ARG_TOOMANYARGS)
            e = unc_call(unc, &main, 0, &pile);
        if (e) {
            uncilerr(unc, e);
            return UNCIL_EXIT_FAIL;
        }
        unc_returnvalues(unc, &pile, &tuple);
        if (tuple.count > 0) {
            Unc_Size sn;
            char *ss;
            Unc_Value retval = UNC_BLANK;
            unc_copy(unc, &retval, &tuple.values[0]);
            unc_discard(unc, &pile);
            if (!unc_valuetostringn(unc, &retval, &sn, &ss)) {
                fprintf(stdout, "main() returned %s\n", ss);
                unc_mfree(unc, ss);
            }
            unc_clear(unc, &retval);
        } else
            unc_discard(unc, &pile);
    }

    e = unc__setupfuncs(unc, 1);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }

    return uncildorepl(unc, unc_print);
}

static int uncilfile(char* argv[], int fileat) {
    int e;
    FILE *f;
    int close;
    Unc_View *unc;
    Unc_Pile pile;
    Unc_Value main = UNC_BLANK;
    int binary = 0;

    if (!strcmp(argv[fileat], "-")) {
        int c;
        close = 0;
        f = stdin;
        c = getc(f);
        ungetc(c, f);
        binary = c >= 0x80 && c < 0xc0;
    } else {
        int c;
        close = 1;
        f = fopen(argv[fileat], "r");
        if (!f) {
            printf("%s: cannot open file '%s': %s\n",
                myname, argv[fileat], strerror(errno));
            return UNCIL_EXIT_FAIL;
        }
        c = getc(f);
        ungetc(c, f);
        binary = c >= 0x80 && c < 0xc0;
        if (binary) {
            fclose(f);
            f = fopen(argv[fileat], "rb");
            if (!f) {
                printf("%s: cannot open file '%s': %s\n",
                    myname, argv[fileat], strerror(errno));
                return UNCIL_EXIT_FAIL;
            }
        }
    }

    unc = unc_createex(NULL, NULL, UNC_MMASK_ALL);
    if (!unc) {
        printf("%s: could not create an Uncil instance (low on memory?)\n",
            myname);
        return UNCIL_EXIT_FAIL;
    }
    
    e = unc__setupfuncs(unc, 0);
    if (e) {
        printf("%s: could not set up Uncil instance\n", myname);
        return UNCIL_EXIT_FAIL;
    }
    e = unc__initpaths(unc, argv[fileat]);
    if (e) {
        printf("%s: could not set up Uncil instance\n", myname);
        return UNCIL_EXIT_FAIL;
    }
    e = unc__initdlpaths(unc, argv[fileat]);
    if (e) {
        printf("%s: could not set up Uncil REPL instance\n", myname);
        return UNCIL_EXIT_FAIL;
    }

    atexit(&uncil_atexit); /* unc_destroy */
    uncil_instance = unc;
    e = binary ? unc_loadfile(unc, f) : unc_compilefile(unc, f);
    if (!e) unc__copyprogname(unc, close ? argv[fileat] : "<stdin>");
    if (close) fclose(f);
    if (e) {
        uncilerr(unc, e);
        return UNCIL_EXIT_FAIL;
    }

    e = unc_call(unc, NULL, 0, &pile);
    if (e) {
        uncilerr(unc, e);
        return UNCIL_EXIT_FAIL;
    }
    unc_discard(unc, &pile);

    if (!unc_getpublicc(unc, "main", &main)) {
        Unc_Tuple tuple;
        e = unc__argvtostacklist(unc, argv, fileat);
        if (e) {
            printf("%s: cannot run main: out of memory (or too many arguments"
                        " to fit in memory)\n", myname);
            return UNCIL_EXIT_FAIL;
        }
        e = unc_call(unc, &main, 1, &pile);
        if (e == UNCIL_ERR_ARG_TOOMANYARGS)
            e = unc_call(unc, &main, 0, &pile);
        if (e) {
            uncilerr(unc, e);
            return UNCIL_EXIT_FAIL;
        }
        unc_returnvalues(unc, &pile, &tuple);
        if (tuple.count > 0) {
            Unc_Value retval = UNC_BLANK;
            unc_copy(unc, &retval, &tuple.values[0]);
            unc_discard(unc, &pile);
            unc__exit_do(unc, &retval);
            unc_clear(unc, &retval);
            return UNCIL_EXIT_OK;
        }
        unc_discard(unc, &pile);
    }
    return UNCIL_EXIT_OK;
}

int print_help(int err) {
    if (!err)
        printf("uncil - Uncil interpreter (%s)\n", UNCIL_VER_STRING);
    puts("Usage: uncil [OPTION]... [file.unc [ARGS]...]");
    if (err) {
        printf("Try '%s --help' for more information.\n", myname);
    } else {
        puts("Executes Uncil source code files (.unc)");
        puts("or byte code files (.cnu)");
        puts("Options:");
        puts("  -?, --help");
        puts("\t\tprints this message");
        puts("  --version");
        puts("\t\tprints version information");
        puts("  -i");
        puts("\t\tinteractive mode; open REPL after running file");
    }
    return err;
}

int main(int argc, char* argv[]) {
    int i;
    int argindex = 1, fileindex = 0;
    int flagok = 1;
    myname = argv[0];

#if UNCIL_LIB_READLINE
    using_history();
    if (getenv("UNCILMAXHIST")) {
        int i = atoi(getenv("UNCILMAXHIST"));
        if (i) stifle_history(i);
    } else
        stifle_history(MAXHIST);
#endif

    for (i = 1; i < argc; ++i) {
        if (flagok && argv[i][0] == '-' && argv[i][1]) {
            char *arg = argv[i];
            if (!strcmp(arg, "--")) {
                flagok = 0;
            } else if (!strcmp(arg, "-i")) {
                interactive = 1;
            } else if (!strcmp(arg, "-?") || !strcmp(arg, "--help")) {
                return print_help(UNCIL_EXIT_OK);
            } else if (!strcmp(arg, "--version")) {
                uncil_printversion();
                return 0;
            } else {
                /* unrecognized flag */
                return print_help(UNCIL_EXIT_USE);
            }
        } else {
            if (!fileindex) {
                fileindex = argindex;
                flagok = 0;
            }
            argv[argindex++] = argv[i];
        }
    }
    argv[argindex++] = NULL; /* terminate new argv */

    if (!fileindex)
        return uncilrepl();
    return interactive ? uncilfilei(argv, fileindex)
                       : uncilfile(argv, fileindex);
}
