CC ?= gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra

BINDIR := bin
SRCS := $(wildcard *.c)
BINS := $(SRCS:%.c=$(BINDIR)/%)

.PHONY: all clean
all: $(BINS)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/%: %.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)
	@-rmdir $(BINDIR) 2>/dev/null || true
