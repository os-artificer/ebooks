CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread

BINDIR := bin
SRCS := $(wildcard *.cpp)
BINS := $(SRCS:%.cpp=$(BINDIR)/%)

# ============================================================
# 各方案的特定编译参数
# ============================================================

# 方案 A：动态库加载替换（dlopen + dlsym）
# 需要链接 libdl
dynamic_loader_LDFLAGS := -ldl

# 方案 B：GOT/PLT 劫持
# 需要链接 libdl（dl_iterate_phdr / dladdr）
# 必须用 -O0，防止 rand() 被内联导致不走 GOT
got_hijack_CXXFLAGS := -O0
got_hijack_LDFLAGS := -ldl

# 方案 C：代码段直接补丁
# 必须用 -O0，防止函数被内联或优化掉
code_patch_CXXFLAGS := -O0

# 方案 D：信号驱动的模块替换
# 需要链接 libdl
signal_reload_LDFLAGS := -ldl

# 方案 E：进程级热替换（优雅重启）
# 需要链接 librt（POSIX 共享内存）
graceful_restart_LDFLAGS := -lrt

.PHONY: all clean
all: $(BINS)

$(BINDIR):
	mkdir -p $(BINDIR)

# 通用编译规则：通过 $(VAR) 展开各方案的特定参数
# $* 是词干匹配（去掉 .cpp 后缀的文件名）
$(BINDIR)/%: %.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $($*_CXXFLAGS) -o $@ $< $($*_LDFLAGS)

clean:
	rm -f $(BINS)
	@-rmdir $(BINDIR) 2>/dev/null || true
