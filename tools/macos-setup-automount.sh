#!/bin/bash
# macOS Linux源码磁盘镜像自动挂载配置脚本
# 
# 功能：将磁盘镜像配置为自动挂载
# 使用：./macos-setup-automount.sh

set -e

LINUX_SOURCE_DMG="$HOME/linux-source.dmg.sparseimage"
MOUNT_POINT="/Volumes/LinuxSource"

echo "=== macOS Linux源码磁盘镜像自动挂载配置 ==="
echo ""

# 检查镜像文件是否存在
if [ ! -f "$LINUX_SOURCE_DMG" ]; then
    echo "❌ 错误：磁盘镜像文件不存在: $LINUX_SOURCE_DMG"
    echo "请先执行以下命令创建磁盘镜像："
    echo "  hdiutil create -type SPARSE -fs 'Case-sensitive Journaled HFS+' -size 20g -volname LinuxSource ~/linux-source.dmg"
    exit 1
fi

# 确定shell配置文件
if [ -n "$ZSH_VERSION" ]; then
    SHELL_CONFIG="$HOME/.zshrc"
elif [ -n "$BASH_VERSION" ]; then
    SHELL_CONFIG="$HOME/.bashrc"
else
    # 默认使用.zshrc（macOS默认shell是zsh）
    SHELL_CONFIG="$HOME/.zshrc"
fi

echo "📋 检测到Shell配置文件: $SHELL_CONFIG"
echo ""

# 检查是否已经有自动挂载配置
if grep -q "linux-source.dmg" "$SHELL_CONFIG" 2>/dev/null; then
    echo "⚠️  警告：自动挂载配置已存在于 $SHELL_CONFIG"
    echo "内容："
    grep -A 10 "linux-source.dmg" "$SHELL_CONFIG"
    echo ""
    read -p "是否覆盖现有配置？(y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ 取消配置"
        exit 0
    fi
    
    # 删除旧配置
    echo "🗑️  删除旧配置..."
    grep -v "linux-source.dmg" "$SHELL_CONFIG" > "$SHELL_CONFIG.tmp" || true
    mv "$SHELL_CONFIG.tmp" "$SHELL_CONFIG"
fi

# 添加自动挂载配置
echo "📝 添加自动挂载配置到 $SHELL_CONFIG..."
cat >> "$SHELL_CONFIG" << 'EOF'

# === macOS Linux源码磁盘镜像自动挂载 ===
if [ -f "$HOME/linux-source.dmg.sparseimage" ] && [ ! -d "/Volumes/LinuxSource" ]; then
  echo "🔗 挂载Linux源码磁盘镜像..."
  hdiutil attach "$HOME/linux-source.dmg.sparseimage" -quiet 2>/dev/null || true
fi
# =========================================
EOF

echo "✅ 配置已添加"
echo ""

# 立即生效（重新加载配置）
echo "🔄 重新加载Shell配置..."
source "$SHELL_CONFIG" 2>/dev/null || true

# 检查挂载状态
if [ -d "$MOUNT_POINT" ]; then
    echo "✅ 磁盘镜像已挂载: $MOUNT_POINT"
else
    echo "⚠️  磁盘镜像未挂载，尝试手动挂载..."
    hdiutil attach "$LINUX_SOURCE_DMG" -quiet 2>/dev/null || true
    
    if [ -d "$MOUNT_POINT" ]; then
        echo "✅ 手动挂载成功: $MOUNT_POINT"
    else
        echo "❌ 手动挂载失败，请检查镜像文件"
    fi
fi

echo ""
echo "=== 配置完成 ==="
echo ""
echo "📌 后续操作："
echo "  1. 重新启动终端，自动挂载将生效"
echo "  2. 或者手动执行: source $SHELL_CONFIG"
echo "  3. 进入Linux仓库: cd /Volumes/LinuxSource/linux"
echo ""
echo "📊 当前状态："
echo "  镜像文件: $LINUX_SOURCE_DMG"
echo "  挂载点: $MOUNT_POINT"
if [ -d "$MOUNT_POINT" ]; then
    echo "  状态: ✅ 已挂载"
    echo "  可用空间: $(df -h "$MOUNT_POINT" | awk 'NR==2 {print $4}')"
else
    echo "  状态: ❌ 未挂载"
fi
