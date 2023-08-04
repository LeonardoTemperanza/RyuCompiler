@echo off

IF NOT EXIST ..\Build mkdir ..\Build
pushd ..\Build

REM Check if llvm lib file is present
IF NOT EXIST "..\Project\Libs\LLVM-C.lib" (
echo Build failed, due to missing LLVM-C.lib. This shouldn't happen in the first place, but you could try building from source.
echo LLVM Source is not included here, for obvious reasons.

goto :exitlabel
)

REM Check if tilde backend is built
IF NOT EXIST "..\Project\Libs\tb.lib" (
echo Build failed. Please build tilde_backend, and copy tb.lib into Project^/Libs ^(for more info: https:^/^/github.com^/RealNeGate^/Cuik^)

goto :exitlabel
)

set debug_flags=/DDebug /MDd /Zi
set profile_flags=/DProfile

set warning_level=/W2

set include_dirs="..\Project\Source\tilde_backend\Cuik\tb\include"

set common=/FC /Feryu.exe /std:c++20 /permissive %warning_level% /we4061 /we4062 /we4714 /wd4530 /wd4200 /nologo ..\Project\Source\unity_build.cpp /I %include_dirs% /link ..\Project\Libs\LLVM-C.lib ..\Project\Libs\tb.lib

REM Development build, debug is enabled, profiling and optimization disabled
cl /Od %debug_flags% %common%

REM Optimized build with debug information
REM cl /O2 %debug_flags% %common%

REM Profiling of optimized, final build, no debug info
REM cl /O2 %profile_flags% %common%

REM Final build
REM cl /O2 %common%

echo Done.

REM For quicker testing
IF %ERRORLEVEL%==0 ( 
echo Running program:
pushd TestPrograms
..\ryu.exe interp_test.c
popd
)

:exitlabel

popd
