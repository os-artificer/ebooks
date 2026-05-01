CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread

BINDIR := bin
SRCS := $(wildcard *.cpp)
BINS := $(SRCS:%.cpp=$(BINDIR)/%)

.PHONY: all clean
all: $(BINS)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/%: %.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(BINS)
	@-rmdir $(BINDIR) 2>/dev/null || true
