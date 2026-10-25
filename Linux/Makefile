CC       = gcc
PKG      = gtk4
PROFILE ?= portable
ifeq ($(PROFILE),native)
CPU_FLAGS = -march=native
else ifeq ($(PROFILE),portable)
CPU_FLAGS = -march=x86-64 -mtune=generic
else
$(error Unknown PROFILE: $(PROFILE))
endif
CFLAGS   = -Wall -Wextra -O2 $(CPU_FLAGS) $(shell pkg-config --cflags $(PKG)) \
           -fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=2
LDFLAGS  = $(shell pkg-config --libs $(PKG)) -pie -lpthread -lgmp
SRCS     = $(wildcard src/*.c)
OBJS     = $(SRCS:.c=.o)
TARGET   = tuxtimings

all: $(TARGET)

.PHONY: FORCE
FORCE:

build/build_info.generated.h: FORCE ../VERSION tools/build-info.sh
	TUXTIMINGS_BUILD_PROFILE=$(PROFILE) bash tools/build-info.sh --generate build
	@printf '%s\n' '$(PROFILE) $(CFLAGS)' > build/compile-flags.tmp
	@cmp -s build/compile-flags.tmp build/compile-flags || mv build/compile-flags.tmp build/compile-flags
	@rm -f build/compile-flags.tmp

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

build/compile-flags: build/build_info.generated.h
	@test -f $@

src/%.o: src/%.c $(wildcard src/*.h) build/build_info.generated.h build/compile-flags
	$(CC) $(CFLAGS) -c -o $@ $<

# Build the tuxbench kernel module (optional — app works without it)
kmod:
	$(MAKE) -C src/tuxbench

kmod-install: kmod
	$(MAKE) -C src/tuxbench install

kmod-clean:
	$(MAKE) -C src/tuxbench clean

clean: kmod-clean
	rm -f src/*.o $(TARGET)

.PHONY: all clean kmod kmod-install kmod-clean
