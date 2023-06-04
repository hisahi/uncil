
-include config.inc

CC?=cc
CCFLAGS?=
LD?=ld
LDFLAGS?=
CCLIBS?=
LDLIBS?=
PREFIX?=/usr/local

OBJS=uvlq.o uview.o utxt.o uarithm.o ustack.o uhash.o ubtree.o uctype.o uval.o \
     umem.o uutf.o ustr.o ublob.o uarr.o uobj.o ufunc.o uopaque.o umt.o uvop.o \
     ucxt.o ulex.o uparse.o uprog.o ucomp.o usort.o uoptim.o uvsio.o ugc.o     \
     uxprintf.o uxscanf.o uimpl.o  ulib.o umodule.o umodstub.o uerr.o uvm.o    \
     ulibsys.o ulibgc.o ulibmath.o ulibos.o ulibio.o ulibconv.o ulibrand.o     \
     ulibtime.o ulibregx.o ulibjson.o ulibcbor.o ulibfs.o ulibproc.o           \
     ulibunic.o ulibcoro.o ulibthrd.o

HEADERS=ualloc.h uarithm.h uarr.h ublob.h ubtree.h ucommon.h ucomp.h           \
        ucompdef.h ucstd.h uctype.h ucxt.h udebug.h udef.h uerr.h ufunc.h      \
        ugc.h uhash.h ulex.h ulibio.h umem.h umodule.h umt.h uncil.h uobj.h    \
        uopaque.h uops.h uoptim.h uosdef.h uparse.h uprog.h usort.h ustack.h   \
        ustr.h utxt.h uutf.h uval.h uvali.h uview.h uvlq.h uvm.h uvop.h        \
        uvsio.h uxprintf.h uxscanf.h

ALLOBJS := $(OBJS) uncver.o uncil.o uncilc.o dbguncil.o udebug.o uncil.o
DEBUGOBJS?=udebug.o

ifneq ($(prefix),)
    PREFIX:=$(prefix)
endif

.PHONY: all clean debug install

all: uncil uncilc
testparse: uncil uncilc dbguncil

%.o: %.c $(HEADERS) Makefile $(wildcard config.inc)
	$(CC) -c $(CCFLAGS) $(CCLIBS) -o $@ $<

uncil: $(OBJS) $(DEBUGOBJS) uncver.o uncil.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

uncilc: $(OBJS) $(DEBUGOBJS) uncver.o uncilc.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

dbguncil: $(OBJS) udebug.o dbguncil.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f uncil uncilc dbguncil $(ALLOBJS)

install:
	cp uncil $(PREFIX)/bin/
	cp uncilc $(PREFIX)/bin/
