#!/bin/bash
# macOS Linux源码磁盘镜像大小调整脚本
# 
# 功能：调整磁盘镜像的大小
# 使用：
#   ./macos-resize-image.sh          # 显示当前大小
#   ./macos-resize-image.sh 40g     # 调整到40GB
#   ./macos-resize-image.sh 60g     # 调整到60GB

set -e

LINUX_SOURCE_DMG="$HOME/linux-source.dmg.sparseimage"
MOUNT_POINT="/Volumes/LinuxSource"

echo "=== macOS Linux源码磁盘镜像大小调整工具 ==="
echo ""

# 检查镜像文件是否存在
if [ ! -f "$LINUX_SOURCE_DMG" ]; then
    echo "❌ 错误：磁盘镜像文件不存在: $LINUX_SOURCE_DMG"
    exit 1
fi

# 显示当前大小信息
echo "📊 当前磁盘镜像信息："
echo "  镜像文件: $LINUX_SOURCE_DMG"
ls -lh "$LINUX_SOURCE_DMG" | awk '{print "  文件大小: " $5}'
echo ""

# 如果镜像已挂载，显示使用情况
if [ -d "$MOUNT_POINT" ]; then
    echo "📂 挂载点使用情况："
    df -h "$MOUNT_POINT" | awk 'NR==1 {print "  " $0} NR==2 {print "  " $0 " (可用: " $4 ")"}'
    echo ""
fi

# 如果没有提供参数，显示使用方法
if [ $# -eq 0 ]; then
    echo "📖 使用方法："
    echo "  $0 [大小]"
    echo ""
    echo "示例："
    echo "  $0 30g     # 调整到30GB"
    echo "  $0 40g     # 调整到40GB"
    echo "  $0 60g     # 调整到60GB"
    echo "  $0 100g    # 调整到100GB"
    echo ""
    echo "⚠️  注意："
    echo "  1. 大小单位可以是 g (GB) 或 m (MB)"
    echo "  2. 调整后需要重新挂载镜像才能生效"
    echo "  3. 建议预留足够空间用于编译Linux内核"
    exit 0
fi

NEW_SIZE="$1"

# 验证大小格式
if [[ ! "$NEW_SIZE" =~ ^[0-9]+[gm]$ ]]; then
    echo "❌ 错误：大小格式不正确"
    echo "正确格式：数字+单位，例如 30g, 40g, 60g"
    exit 1
fi

echo "🎯 目标大小: $NEW_SIZE"
echo ""

# 检查镜像是否已挂载
if [ -d "$MOUNT_POINT" ]; then
    echo "⚠️  警告：磁盘镜像当前已挂载"
    echo "调整大小需要先卸载镜像。"
    read -p "是否现在卸载镜像？(y/N): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🔗 卸载磁盘镜像..."
        hdiutil detach "$MOUNT_POINT" -quiet 2>/dev/null || true
        sleep 2
        echo "✅ 镜像已卸载"
    else
        echo "❌ 取消操作"
        exit 0
    fi
fi

# 调整镜像大小
echo ""
echo "🔧 调整磁盘镜像大小到 $NEW_SIZE..."
hdiutil resize -size "$NEW_SIZE" "$LINUX_SOURCE_DMG"

if [ $? -eq 0 ]; then
    echo "✅ 大小调整成功！"
    echo ""
    
    # 显示调整后的信息
    echo "📊 调整后信息："
    ls -lh "$LINUX_SOURCE_DMG" | awk '{print "  文件大小: " $5}'
    echo ""
    
    # 询问是否重新挂载
    read -p "是否现在重新挂载镜像？(Y/n): " -n 1 -r
    echo ""
    
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        echo "🔗 重新挂载磁盘镜像..."
        hdiutil attach "$LINUX_SOURCE_DMG" -quiet 2>/dev/null || true
        sleep 2
        
        if [ -d "$MOUNT_POINT" ]; then
            echo "✅ 镜像已重新挂载: $MOUNT_POINT"
            echo ""
            echo "📂 新的使用情况："
            df -h "$MOUNT_POINT" | awk 'NR==1 {print "  " $0} NR==2 {print "  " $0 " (可用: " $4 ")"}'
        else
            echo "❌ 重新挂载失败"
        fi
    else
        echo ""
        echo "📌 手动挂载命令："
        echo "  hdiutil attach $LINUX_SOURCE_DMG"
    fi
else
    echo "❌ 大小调整失败"
    exit 1
fi

echo ""
echo "=== 操作完成 ==="
