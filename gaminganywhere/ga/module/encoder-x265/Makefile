
include ../Makefile.common

ifeq ($(OS), MSYS)
LDFLAGS	+= ../../core/libga.dll $(AVCLD)
endif

CFLAGS	+= $(shell pkg-config --cflags x265)
LDFLAGS	+= $(shell pkg-config --libs x265)

OBJS	= encoder-x265.o
TARGET	= encoder-x265.$(EXT)

include ../Makefile.build

