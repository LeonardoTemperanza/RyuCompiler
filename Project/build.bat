
@echo off
setlocal enabledelayedexpansion

IF NOT EXIST ..\Build mkdir ..\Build
pushd ..\Build

REM Check if llvm lib file is present
IF NOT EXIST "..\Project\Libs\LLVM-C.lib" (
echo Build failed, due to missing LLVM-C.lib. This shouldn't happen in the first place, but you could try building from source.
echo LLVM Source is not included here, for obvious reasons ^(it's gigantic^).

goto :exitlabel
)

REM Check if tilde backend is built
IF NOT EXIST "..\Project\Libs\tb.lib" (
echo Build failed. Please build tilde_backend, and copy tb.lib into Project^/Libs ^(for more info: https:^/^/github.com^/RealNeGate^/Cuik^)

goto :exitlabel
)

REM Enable to debug memory issues such as free after use
REM set sanitizer=/fsanitize=address
set sanitizer=

REM /DUseFixedAddr to enable fixed addresses for virtual memory allocations
REM /DDebugDep to enable printing debug information regarding the dep graph
set debug_flags=/DDebug /Zi %sanitizer%
set profile_flags=/DProfile

set warning_level=/W2

set include_dirs="..\Project\Source\tilde_backend\Cuik\tb\include"

REM Compile microsoft_craziness if needed (this one file takes a lot longer to compile
REM than the rest of the project simply because it needs to include windows.h)
IF NOT EXIST "microsoft_craziness.obj" (
cl /nologo /c /DMICROSOFT_CRAZINESS_IMPLEMENTATION /TP ..\Project\Source\os\microsoft_craziness.h -Fo:microsoft_craziness.obj
)

set common=/FC /Feryu.exe /std:c++20 /permissive %warning_level% /we4061 /we4062 /we4714 /wd4530 /wd4200 /nologo ..\Project\Source\unity_build.cpp microsoft_craziness.obj /I %include_dirs% /link ..\Project\Libs\LLVM-C.lib ..\Project\Libs\tb.lib Ole32.lib OleAut32.lib

REM Development build, debug is enabled, profiling and optimization disabled
cl /Zi /Od %debug_flags% %common%
set build_ret=%errorlevel%

REM Optimized build with debug information
REM cl /O2 %debug_flags% %common%
REM set build_ret=%errorlevel%

REM Profiling of optimized, final build, no debug info
REM cl /O2 %profile_flags% %common%
REM set build_ret=%errorlevel%

REM Final build
REM cl /O2 %common%
REM set build_ret=%errorlevel%

echo Done.

REM For quicker testing
if %build_ret%==0 ( 
echo Running program:
pushd .\TestPrograms
ryu interp_test.ryu -emit_bc -emit_ir -o output.exe

call output.exe

echo Result from program: !errorlevel!
popd
)

:exitlabel

popd
