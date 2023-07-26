set target_path=%1%
md %target_path%
set cur_path=%~dp0
for /r %%i in (*.cpp, *.h, *.mak, Makefile) do (
   set file_path=%%~dfi
   set dir_path=%%~dpi

   setlocal enabledelayedexpansion

   set "target_dir=!dir_path:%cur_path%=\!"
   echo %target_path%!target_dir!
   md %target_path%!target_dir!

   set "target_file=!file_path:%cur_path%=\!"
   echo %target_path%!target_file!

   mklink /h %target_path%!target_file! %file_path%
   )


pause
