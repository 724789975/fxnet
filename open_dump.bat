@echo off
md C:\CrashDump
echo 正在启用Dump...
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps"
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\fxnet.exe"
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\fxnet.exe" /v DumpFolder /t REG_EXPAND_SZ /d "C:\CrashDump" /f
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\fxnet.exe" /v DumpType /t REG_DWORD /d 2 /f
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\fxnet.exe" /v DumpCount /t REG_DWORD /d 10 /f
echo Dump已经启用
pause
@echo on
