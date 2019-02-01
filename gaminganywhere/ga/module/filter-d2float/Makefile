
include ../Makefile.common

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll $(AVCLD)
endif

OBJS	= filter-d2float.o
TARGET	= filter-d2float.$(EXT)

include ../Makefile.build

