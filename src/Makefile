# 顶层 Makefile：一键编译 src/c 和 src/cpp 下所有代码
#
# 用法：
#   make          # 编译所有（C + C++）
#   make c        # 仅编译 C 代码
#   make cpp      # 仅编译 C++ 代码
#   make clean    # 清理所有编译产物
#   make run-<name>  # 编译并运行某个二进制（如 make run-hello）

SUBDIRS := c cpp

.PHONY: all clean $(SUBDIRS)
all: $(SUBDIRS)

# 递归调用子目录的 Makefile
c cpp:
	$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# 编译并运行某个程序
# 用法：make run-hello, make run-code_patch, make run-dynamic_loader 等
.PHONY: run-%
run-%: %
	@echo "=== 运行 $* ==="
	@./$<

# 便捷规则：先编译子目录再运行
.PHONY: run-c-% run-cpp-%
run-c-%:
	$(MAKE) -C c
	@echo "=== 运行 c/bin/$* ==="
	@./c/bin/$*

run-cpp-%:
	$(MAKE) -C cpp
	@echo "=== 运行 cpp/bin/$* ==="
	@./cpp/bin/$*
