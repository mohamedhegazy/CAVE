
include ../Makefile.common

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll $(AVCLD)
endif

OBJS	= rc.o
TARGET	= rc.$(EXT)

include ../Makefile.build

