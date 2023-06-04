
# These flags will be passed to the C compiler and linker.

# Configs: 
#		   RELEASE (default; multithreaded optimized build)
#		   SINGLE (singlethreaded optimized build)
#		   DEBUG (multithreaded debug build)
#		   DEBUGST (singlethreaded debug build)
#		   PEDANT (for checking that there are no easy to miss bugs)
#          BASIC (non-debug build with only basic optimizations)
#          AUSTERE (non-debug build with no optimizations)
#          COMPACT (non-debug build optimized for binary size)

# Available libraries
LIB_READLINE=1
LIB_PTHREAD=1
LIB_PCRE2=1
LIB_ICU=1
LIB_JEMALLOC=0
LIB_TCMALLOC=0
LIB_MIMALLOC=0

# Standard *nix setup for glibc
CCLIBS=
LDLIBS=-lc -lm -ldl

# Modern POSIX requirement; "ld" does not tend to work anymore
LD=${CC}

# configurations; release is the default
ifdef PEDANT
CCFLAGS=-DUNCIL_SINGLETHREADED -ansi -pedantic -Wall -Werror -Wno-unused-function -g -O0
LDFLAGS=
else ifdef DEBUG
CCFLAGS=-D_REENTRANT -Wall -Werror -Wno-unused-function -g -O0
LDFLAGS=
else ifdef DEBUGST
CCFLAGS=-DUNCIL_SINGLETHREADED -Wall -Werror -Wno-unused-function -g -O0
LDFLAGS=
else ifdef AUSTERE
CCFLAGS=-O0 -DNDEBUG -D_REENTRANT
LDFLAGS=-O0
DEBUGOBJS=
else ifdef BASIC
CCFLAGS=-O1 -DNDEBUG -D_REENTRANT
LDFLAGS=-O1
DEBUGOBJS=
else ifdef COMPACT
CCFLAGS=-Os -DNDEBUG -D_REENTRANT
LDFLAGS=-s -Os
DEBUGOBJS=
else ifdef SINGLE
CCFLAGS=-DUNCIL_SINGLETHREADED -O3 -DNDEBUG -D_REENTRANT
LDFLAGS=-O3 -flto
DEBUGOBJS=
else
CCFLAGS=-D_REENTRANT -O3 -DNDEBUG -D_REENTRANT
LDFLAGS=-O3 -flto
DEBUGOBJS=
endif

# Add libreadline
ifeq ($(LIB_READLINE),1)
CCLIBS:=$(CCLIBS) -DUNCIL_LIB_READLINE
LDLIBS:=$(LDLIBS) -lreadline
endif

# Add libpthread
ifeq ($(LIB_PTHREAD),1)
CCLIBS:=$(CCLIBS) -pthread -DUNCIL_LIB_PTHREAD
LDLIBS:=$(LDLIBS) -lpthread
endif

# Add libpcre2
ifeq ($(LIB_PCRE2),1)
CCLIBS:=$(CCLIBS) -DUNCIL_LIB_PCRE2
LDLIBS:=$(LDLIBS) -lpcre2-8
endif

# Add ICU
ifeq ($(LIB_ICU),1)
CCLIBS:=$(CCLIBS) -DUNCIL_LIB_ICU
LDLIBS:=$(LDLIBS) -licuuc -licudata
endif

# Add jemalloc
ifeq ($(LIB_JEMALLOC),1)
ifeq ($(CUSTOM_ALLOCATOR),1)
$(error Multiple custom allocators enabled. Only one is allowed)
endif
CUSTOM_ALLOCATOR=1
CCLIBS:=$(CCLIBS) -DUNCIL_LIB_JEMALLOC
LDLIBS:=$(LDLIBS) -L`jemalloc-config --libdir` -Wl,-rpath,`jemalloc-config --libdir` -ljemalloc `jemalloc-config --libs`
endif

# Add TCMalloc
ifeq ($(LIB_TCMALLOC),1)
ifeq ($(CUSTOM_ALLOCATOR),1)
$(error Multiple custom allocators enabled. Only one is allowed)
endif
CUSTOM_ALLOCATOR=1
CCLIBS:=$(CCLIBS) -DUNCIL_LIB_TCMALLOC -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
LDLIBS:=$(LDLIBS) -ltcmalloc
endif

# Add mimalloc
ifeq ($(LIB_MIMALLOC),1)
ifeq ($(CUSTOM_ALLOCATOR),1)
$(error Multiple custom allocators enabled. Only one is allowed)
endif
CUSTOM_ALLOCATOR=1
CCLIBS:=$(CCLIBS) -DUNCIL_LIB_MIMALLOC
LDLIBS:=$(LDLIBS) -lmimalloc
endif
