#!/bin/bash

# 进入common目录构建
echo "===== 开始构建common模块 ====="
cd common && ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./android.mk
if [ $? -ne 0 ]; then
    echo "common模块构建失败!"
    exit 1
fi
echo "===== common模块构建完成 ====="

# 拷贝库文件
echo "===== 正在拷贝库文件 ====="
mkdir -p ../dll/local
cp -f libs/local/$(TARGET_ARCH_ABI)/*.a ../dll/local/
echo "拷贝文件: libs/local/$(TARGET_ARCH_ABI)/*.a -> ../dll/local/"
echo "===== 库文件拷贝完成 ====="

# 进入dll目录构建
echo "===== 开始构建dll模块 ====="
cd ../dll && ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./android.mk
if [ $? -ne 0 ]; then
    echo "dll模块构建失败!"
    exit 1
fi
echo "===== dll模块构建完成 ====="

echo "===== 全部构建完成 ====="
