@echo off
REM 设置环境变量
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

cd common
nmake /f Makefile.mak DEBUG=1

cd ..
REM 使用 nmake 编译代码
nmake /f Makefile.mak DEBUG=1
