# macOS上保存Linux源码的方案

## 问题背景

在macOS默认的文件系统（APFS或HFS+，不区分大小写）上处理Linux内核源码时，会遇到Git无法正确跟踪文件名大小写的问题。

例如：

- `xt_TCPMSS.h` 和 `xt_tcpmss.h` 被视为同一个文件
- `git restore`、`git checkout` 等命令无法正确恢复文件
- 文件状态在大小写之间循环切换，无法清理工作区

## 解决方案：使用区分大小写的磁盘镜像

### 步骤1：创建区分大小写的磁盘镜像

```bash
# 创建一个20GB的稀疏磁盘镜像（支持动态扩展，实际使用多少占用多少）
hdiutil create -type SPARSE \
  -fs 'Case-sensitive Journaled HFS+' \
  -size 20g \
  -volname LinuxSource \
  ~/linux-source.dmg
```

**说明：**
- `-type SPARSE`：创建稀疏镜像，空间按需分配
- `-fs 'Case-sensitive Journaled HFS+'`：使用区分大小写的文件系统
- `-size 20g`：最大容量20GB（Linux源码约1-2GB，预留空间用于编译等）
- `~/linux-source.dmg`：镜像文件位置

### 步骤2：挂载磁盘镜像

```bash
hdiutil attach ~/linux-source.dmg
```

挂载后，镜像会显示在 `/Volumes/LinuxSource`。

### 步骤3：在区分大小写的分区上克隆Linux仓库

```bash
# 进入挂载的卷
cd /Volumes/LinuxSource

# 克隆Linux仓库（官方仓库示例）
git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git

# 或者克隆已有的仓库
git clone <your-repo-url>
```

### 步骤4：后续使用

```bash
# 每次需要使用仓库时，先挂载镜像
hdiutil attach ~/linux-source.dmg

# 进入仓库目录
cd /Volumes/LinuxSource/linux

# 正常使用git命令
git status
git restore .
git pull
```

### 卸载磁盘镜像

```bash
# 卸载镜像（不需要时）
hdiutil detach /Volumes/LinuxSource
```

## 验证方案有效性

在完成上述设置后，可以验证Git是否可以正确处理文件名大小写：

```bash
# 测试1：检查Git状态是否正常
cd /Volumes/LinuxSource/linux
git status
# 应该显示"干净的工作区"

# 测试2：修改文件并恢复
echo "/* test */" >> include/uapi/linux/netfilter/xt_TCPMSS.h
git status
# 应该显示"已修改"

git restore include/uapi/linux/netfilter/xt_TCPMSS.h
git status
# 应该显示"干净的工作区"

# 测试3：验证大小写敏感的文件名
git ls-files include/uapi/linux/netfilter/ | grep -E "(xt_TCPMSS|xt_tcpmss)"
# 应该同时显示 xt_TCPMSS.h 和 xt_tcpmss.h（两个不同文件）
```

## 迁移现有仓库

如果您已有现有的Linux仓库在不区分大小写的分区上，可以按以下方式迁移：

### 方法1：重新克隆（推荐）

```bash
# 在区分大小写的分区上重新克隆
cd /Volumes/LinuxSource
git clone <original-repo-url> linux
```

### 方法2：使用bundle备份和恢复

```bash
# 1. 在现有仓库中创建bundle
cd /path/to/existing/linux
git bundle create ~/linux-backup.bundle --all

# 2. 在区分大小写的分区上从bundle克隆
cd /Volumes/LinuxSource
git clone ~/linux-backup.bundle linux

# 3. 清理bundle文件
rm ~/linux-backup.bundle
```

## 磁盘镜像管理

### 调整镜像大小

提供了自动化脚本 `tools/macos-resize-image.sh`，可以安全地调整镜像大小：

```bash
# 进入仓库目录
cd /path/to/linux

# 查看当前大小
./tools/macos-resize-image.sh

# 调整到40GB
./tools/macos-resize-image.sh 40g

# 调整到60GB
./tools/macos-resize-image.sh 60g
```

> **脚本位置**：
> - 本地路径：`tools/macos-resize-image.sh`
> - GitHub仓库：[https://github.com/os-artificer/ebooks/blob/main/tools/macos-resize-image.sh](https://github.com/os-artificer/ebooks/blob/main/tools/macos-resize-image.sh)

**脚本功能：**
- 显示当前镜像大小和使用情况
- 自动卸载和重新挂载镜像
- 验证大小格式
- 显示调整后的状态

**手动调整（可选）：**

如果希望手动调整，可以使用以下命令：

```bash
# 先卸载镜像
hdiutil detach /Volumes/LinuxSource

# 调整镜像大小到40GB
hdiutil resize -size 40g ~/linux-source.dmg.sparseimage

# 重新挂载镜像
hdiutil attach ~/linux-source.dmg.sparseimage
```

### 自动挂载（推荐）

提供了自动化脚本 `tools/macos-setup-automount.sh`，可以一键配置自动挂载：

```bash
# 进入仓库目录
cd /path/to/linux

# 执行自动挂载配置脚本
./tools/macos-setup-automount.sh
```

> **脚本位置**：
> - 本地路径：`tools/macos-setup-automount.sh`
> - GitHub仓库：[https://github.com/os-artificer/ebooks/blob/main/tools/macos-setup-automount.sh](https://github.com/os-artificer/ebooks/blob/main/tools/macos-setup-automount.sh)

**脚本功能：**
- 自动检测Shell配置文件（`.zshrc` 或 `.bashrc`）
- 添加自动挂载配置
- 立即生效（重新加载Shell配置）
- 显示挂载状态和可用空间

**手动配置（可选）：**

如果希望手动配置，可以将以下命令添加到 `~/.zshrc` 或 `~/.bashrc`：

```bash
# 自动挂载Linux源码镜像
if [ -f ~/linux-source.dmg.sparseimage ] && [ ! -d /Volumes/LinuxSource ]; then
  hdiutil attach ~/linux-source.dmg.sparseimage -quiet
fi
```

## 工具脚本

在 `tools/` 目录下提供了两个自动化脚本：

### 1. `macos-setup-automount.sh` - 自动挂载配置脚本

**功能：**
- 自动配置Shell配置文件（`.zshrc` 或 `.bashrc`）
- 添加自动挂载磁盘镜像的命令
- 立即生效（重新加载Shell配置）

**使用：**
```bash
./tools/macos-setup-automount.sh
```

**配置后效果：**
- 每次启动新终端，自动挂载磁盘镜像
- 无需手动执行 `hdiutil attach`

**脚本位置**：
- 本地路径：`tools/macos-setup-automount.sh`
- GitHub仓库：[https://github.com/os-artificer/ebooks/blob/main/tools/macos-setup-automount.sh](https://github.com/os-artificer/ebooks/blob/main/tools/macos-setup-automount.sh)

### 2. `macos-resize-image.sh` - 镜像大小调整脚本

**功能：**
- 显示当前镜像大小和使用情况
- 安全地调整镜像大小
- 自动处理卸载和重新挂载

**使用：**
```bash
# 查看当前大小
./tools/macos-resize-image.sh

# 调整到40GB
./tools/macos-resize-image.sh 40g

# 调整到60GB
./tools/macos-resize-image.sh 60g
```

**支持的单位：**
- `g` - GB（推荐）
- `m` - MB

**脚本位置**：
- 本地路径：`tools/macos-resize-image.sh`
- GitHub仓库：[https://github.com/os-artificer/ebooks/blob/main/tools/macos-resize-image.sh](https://github.com/os-artificer/ebooks/blob/main/tools/macos-resize-image.sh)

## 常见问题

### Q1: 为什么不使用 `git config core.ignorecase false`？

A: 这个配置只是告诉Git忽略文件系统的大小写不敏感性，但并不能解决根本问题。在区分大小写的文件系统上工作是最彻底的解决方案。

### Q2: 稀疏镜像会占用实际空间吗？

A: 不会。稀疏镜像按需分配空间，初始可能只有几百MB，随着使用逐渐增长，但不超过设定的最大值（如20GB）。

### Q3: 可以将镜像文件放在其他位置吗？

A: 可以。只需修改 `hdiutil create` 和 `hdiutil attach` 命令中的路径即可。

## 总结

在macOS上处理Linux内核源码（或其他对文件名大小写敏感的项目）时，**必须使用区分大小写的文件系统**。

通过创建和使用区分大小写的磁盘镜像，可以彻底解决Git文件跟踪问题，确保所有Git命令正常工作。

---
**文档版本**: 1.1  
**更新日期**: 2026-06-27  
**适用系统**: macOS (APFS或HFS+，默认不区分大小写)

