set target_path=%1%
md %target_path%
set cur_path=%~dp0
for /r %%i in (*Makefile*, *.cpp, *.h, *.mak) do (
   set file_path=%%i
   set dir_path=%%~dpi
   echo %file_path%
   echo %%i

   REM dir %file_path%

   setlocal enabledelayedexpansion

   set "target_dir=!dir_path:%cur_path%=\!"
   echo %target_path%!target_dir!
   md %target_path%!target_dir!

   set "target_file=!file_path:%cur_path%=\!"
   echo %target_path%!target_file!
   echo %file_path%

   mklink /h %target_path%!target_file! %%i
   endlocal
   )


pause
