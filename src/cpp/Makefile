CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread

BINDIR := bin

# ============================================================
# 按文章分子目录，源文件在各子目录中
# ============================================================
SUBDIRS := cpp-multibyte-encoding hot-updated bad-case lock-free-queue
SRCS := $(foreach d,$(SUBDIRS),$(wildcard $(d)/*.cpp))
BINS := $(patsubst %.cpp,$(BINDIR)/%,$(notdir $(SRCS)))

# ============================================================
# 各方案的特定编译参数（变量名基于目标二进制名）
# ============================================================

# --- cpp-multibyte-encoding ---
encoding_convert_LDFLAGS := -liconv
ICU_PATH ?= /usr/local/opt/icu4c@78
c2_normalize_CXXFLAGS := -I$(ICU_PATH)/include
c2_normalize_LDFLAGS := -L$(ICU_PATH)/lib -licuuc -licudata

# --- hot-updated ---
dynamic_loader_LDFLAGS := -ldl
got_hijack_CXXFLAGS := -O0
got_hijack_LDFLAGS := -ldl
code_patch_CXXFLAGS := -O0
signal_reload_LDFLAGS := -ldl
graceful_restart_LDFLAGS := -lrt

# --- lock-free-queue ---

.PHONY: all clean
all: $(BINS)

$(BINDIR):
	mkdir -p $(BINDIR)

# 为每个源文件生成编译规则
# 在 define 外部（即 call 调用处）解析 $(_CXXFLAGS)/$(_LDFLAGS)，
# 确保值在 eval 前就已固定
define COMPILE_RULE
$$(BINDIR)/$$(notdir $$(basename $(1))): $(1) | $$(BINDIR)
	$$(CXX) $$(CXXFLAGS) $(2) -o $$@ $$< $(3)
endef

$(foreach src,$(SRCS),\
	$(eval $(call COMPILE_RULE,\
		$(src),\
		$($(notdir $(basename $(src)))_CXXFLAGS),\
		$($(notdir $(basename $(src)))_LDFLAGS))))

clean:
	rm -f $(BINS)
	@-rmdir $(BINDIR) 2>/dev/null || true
