@echo off
REM ���û�������
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM ʹ�� nmake �������
nmake /f Makefile.mak DEBUG=1
