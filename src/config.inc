
# These flags will be passed to the C compiler and linker.

# Configs: 
#		   RELEASE (default; multithreaded optimized build)
#		   SINGLE (singlethreaded optimized build)
#		   DEBUG (multithreaded debug build)
#		   DEBUGST (singlethreaded debug build)
#		   PEDANT (for checking that there are no easy to miss bugs)

# Available libraries
LIB_READLINE=1
LIB_PTHREAD=1
LIB_PCRE2=1
LIB_ICU=1

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
