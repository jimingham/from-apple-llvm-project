EXE=a.out
SWIFT_SOURCES=main.swift
SWIFTFLAGS_EXTRAS= -I$(shell pwd)
LD_EXTRAS=-L$(shell pwd) -lmod

all: libmod.dylib a.out

include Makefile.rules

libmod.dylib: mod.swift
	$(MAKE) MAKE_DSYM=YES CC=$(CC) SWIFTC=$(SWIFTC) \
		ARCH=$(ARCH) DSYMUTIL=$(DSYMUTIL) \
		BASENAME=mod \
		SWIFTFLAGS_EXTRAS="-I$(BUILDDIR) -enable-library-evolution" \
		VPATH=$(SRCDIR) -I $(SRCDIR) -f $(SRCDIR)/dylib.mk all
