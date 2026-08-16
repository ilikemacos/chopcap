# Chopcap — build with a plain C toolchain. No dependencies.
#
#   make            build build/chopcap for this machine
#   make test       build and run the whole test suite
#   make universal  build a macOS arm64 + x86_64 universal binary
#   make install    copy the binary into PREFIX/bin (default /usr/local)

CC      ?= cc
CFLAGS  ?= -std=c11 -O2
CFLAGS  += -Wall -Wextra -Wno-unused-parameter -D_DEFAULT_SOURCE
LDLIBS  += -lm
PREFIX  ?= /usr/local

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,build/obj/%.o,$(SRCS))
BIN  := build/chopcap

.PHONY: all clean test install uninstall universal

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

build/obj/%.o: src/%.c src/chopcap.h
	@mkdir -p build/obj
	$(CC) $(CFLAGS) -c -o $@ $<

# A single binary that runs natively on both Apple Silicon and Intel Macs.
universal:
	@mkdir -p build/arm64 build/x86_64 build/universal
	$(CC) $(CFLAGS) -arch arm64  -o build/arm64/chopcap  $(SRCS) $(LDLIBS)
	$(CC) $(CFLAGS) -arch x86_64 -o build/x86_64/chopcap $(SRCS) $(LDLIBS)
	lipo -create -output build/universal/chopcap build/arm64/chopcap build/x86_64/chopcap
	@lipo -info build/universal/chopcap

LIB_OBJS := $(filter-out build/obj/main.o,$(OBJS))

build/unit_tests: tests/unit_tests.c $(LIB_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/unit_tests.c $(LIB_OBJS) $(LDLIBS)

test: $(BIN) build/unit_tests
	@./tests/run_tests.sh

bless: $(BIN)
	@./tests/run_tests.sh --bless

install: $(BIN)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/chopcap
	@echo "Installed $(DESTDIR)$(PREFIX)/bin/chopcap"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/chopcap

clean:
	rm -rf build
